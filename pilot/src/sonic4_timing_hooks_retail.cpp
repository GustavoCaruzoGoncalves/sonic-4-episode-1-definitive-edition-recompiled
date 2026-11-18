// Production Sonic 4 hooks: host frame pacing + native call stubs only.
// No per-draw / per-dispatch probe overhead (see sonic4_timing_hooks.cpp).

#include "generated/default/sonic4_pch.h"

#include <atomic>
#include <rex/chrono/clock.h>
#include <rex/kernel/xboxkrnl/video.h>
#include <rex/logging.h>
#ifdef REX_RENDER_TELEMETRY
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <rex/diag/frame_stats.h>
#endif

namespace {

#ifdef REX_RENDER_TELEMETRY
// Exp-015: frame-time distribution and per-spike attribution. Runs on the guest
// main-loop thread only (single writer).
struct FrameSpikeTracker {
  uint64_t last_frame_start = 0;
  uint64_t window_start = 0;
  std::vector<uint32_t> intervals_us;
  uint32_t spikes_logged = 0;
  uint64_t window_guest_us = 0;
  rex::diag::CumulativeCounters frame_start_counters;
  rex::diag::CumulativeCounters window_start_counters;
  uint32_t frame_index = 0;

  static uint64_t ReadRssKiB() {
    uint64_t rss = 0;
    if (FILE* f = std::fopen("/proc/self/statm", "r")) {
      unsigned long size = 0, resident = 0;
      if (std::fscanf(f, "%lu %lu", &size, &resident) == 2) {
        rss = uint64_t(resident) * 4;
      }
      std::fclose(f);
    }
    return rss;
  }

  // Called at the start of each guest main-loop iteration (after pacing).
  // previous_guest_us: guest work of the previous iteration (excluding pacing sleep).
  void OnFrameStart(uint64_t now, uint64_t freq, uint64_t previous_guest_us) {
    rex::diag::CumulativeCounters c = rex::diag::GetCumulative();
    ++frame_index;
    if (last_frame_start) {
      const uint32_t interval_us = uint32_t((now - last_frame_start) * 1000000ull / freq);
      intervals_us.push_back(interval_us);
      window_guest_us += previous_guest_us;
      if (interval_us > 33000 && spikes_logged < 24) {
        ++spikes_logged;
        const rex::diag::CumulativeCounters& p = frame_start_counters;
        // Exp-018: IssueDraw phase breakdown for this frame (ms, non-zero only).
        static const char* const kPhaseNames[16] = {
            "shaderprep", "primitive", "shadersampler", "textures", "rendertarget",
            "pipeline", "pipebind", "dynstate", "sysconst", "bindings", "vfetch",
            "memexport", "barriers_rp", "draw", "memexport_post", "p15"};
        std::string phases;
        for (uint32_t i = 0; i < 16; ++i) {
          const uint64_t ns = c.idraw_phase_ns[i] - p.idraw_phase_ns[i];
          if (ns >= 500000) {
            phases += fmt::format(" {}={:.1f}", kPhaseNames[i], double(ns) / 1e6);
          }
        }
        REXLOG_INFO("PROBE SPIKE_PHASES frame={} fence_waits={} fence_wait_us={}{}",
                    frame_index, c.fence_waits - p.fence_waits,
                    c.fence_wait_us - p.fence_wait_us, phases);
        REXLOG_INFO(
            "PROBE SPIKE frame={} interval_us={} guest_us={} pso={} pso_us={} sh_xlate={} "
            "sh_us={} tex_new={} tex_load={} shm_kib={} draws={} idraw_us={} subs={} "
            "submit_us={} qlock_us={} present_us={} present_lock_us={} acq_us={} rp={} wwf={} "
            "wwf_us={} tex_mib={}",
            frame_index, interval_us, previous_guest_us, c.pso_creates - p.pso_creates,
            c.pso_create_us - p.pso_create_us, c.shader_translations - p.shader_translations,
            c.shader_translate_us - p.shader_translate_us, c.texture_creates - p.texture_creates,
            c.texture_loads - p.texture_loads, (c.shm_upload_bytes - p.shm_upload_bytes) >> 10,
            c.draws - p.draws, c.issue_draw_us - p.issue_draw_us, c.submissions - p.submissions,
            c.submit_us - p.submit_us, c.queue_lock_wait_us - p.queue_lock_wait_us,
            c.present_us - p.present_us, c.present_lock_wait_us - p.present_lock_wait_us,
            c.acquire_image_us - p.acquire_image_us, c.render_pass_begins - p.render_pass_begins,
            c.write_watch_faults - p.write_watch_faults,
            c.write_watch_fault_us - p.write_watch_fault_us, c.texture_host_bytes >> 20);
      }
    }
    last_frame_start = now;
    frame_start_counters = c;
    if (!window_start) {
      window_start = now;
      window_start_counters = c;
      return;
    }
    if (now - window_start < freq * 5 || intervals_us.empty()) {
      return;
    }
    std::vector<uint32_t> sorted = intervals_us;
    std::sort(sorted.begin(), sorted.end());
    auto pct = [&sorted](double q) {
      return sorted[std::min(sorted.size() - 1, size_t(q * double(sorted.size())))];
    };
    uint32_t over20 = 0, over33 = 0, over50 = 0;
    for (uint32_t v : sorted) {
      over20 += v > 20000;
      over33 += v > 33000;
      over50 += v > 50000;
    }
    const rex::diag::CumulativeCounters& w = window_start_counters;
    REXLOG_INFO(
        "PROBE FRAMES n={} p50_us={} p90_us={} p99_us={} max_us={} gt20ms={} gt33ms={} gt50ms={} "
        "guest_us_avg={} | pso={} pso_ms={} sh_xlate={} sh_ms={} tex_new={} tex_load={} "
        "shm_mib={} wwf={} wwf_ms={} qlock_ms={} present_ms={} present_lock_ms={} acq_ms={} | "
        "rss_mib={} tex_mib={}",
        sorted.size(), pct(0.5), pct(0.9), pct(0.99), sorted.back(), over20, over33, over50,
        window_guest_us / sorted.size(), c.pso_creates - w.pso_creates,
        (c.pso_create_us - w.pso_create_us) / 1000, c.shader_translations - w.shader_translations,
        (c.shader_translate_us - w.shader_translate_us) / 1000,
        c.texture_creates - w.texture_creates, c.texture_loads - w.texture_loads,
        (c.shm_upload_bytes - w.shm_upload_bytes) >> 20,
        c.write_watch_faults - w.write_watch_faults,
        (c.write_watch_fault_us - w.write_watch_fault_us) / 1000,
        (c.queue_lock_wait_us - w.queue_lock_wait_us) / 1000, (c.present_us - w.present_us) / 1000,
        (c.present_lock_wait_us - w.present_lock_wait_us) / 1000,
        (c.acquire_image_us - w.acquire_image_us) / 1000, ReadRssKiB() >> 10,
        c.texture_host_bytes >> 20);
    intervals_us.clear();
    window_guest_us = 0;
    spikes_logged = 0;
    window_start = now;
    window_start_counters = c;
  }
};
FrameSpikeTracker g_spike_tracker;
uint64_t g_last_guest_us = 0;
#endif

std::atomic<uint64_t> g_win_start{0};
std::atomic<uint32_t> g_pace_frames{0};
std::atomic<uint64_t> g_pace_frame_us{0};

void RetailMaybeLogPace(uint64_t frame_us) {
  const uint64_t now = rex::chrono::Clock::QueryHostTickCount();
  const uint64_t freq = rex::chrono::Clock::QueryHostTickFrequency();
  uint64_t start = g_win_start.load(std::memory_order_relaxed);
  if (!start) {
    g_win_start.store(now, std::memory_order_relaxed);
    return;
  }
  if (now - start < freq * 5) {
    return;
  }
  const double sec = static_cast<double>(now - start) / static_cast<double>(freq);
  const uint32_t frames = g_pace_frames.exchange(0, std::memory_order_relaxed);
  const uint64_t fus = g_pace_frame_us.exchange(0, std::memory_order_relaxed);
  g_win_start.store(now, std::memory_order_relaxed);
  const double hz = sec > 0.0 ? frames / sec : 0.0;
  REXLOG_INFO(
      "PROBE PACE frames={:.1f}/s frame_us={:.0f} (retail hooks, REX_DIAG_PROFILE=off)",
      hz, frames ? static_cast<double>(fus) / frames : 0.0);
  REXLOG_INFO("PROBE PACE_821C0770/s={:.1f} target_fps=60", hz);
#ifdef REX_RENDER_TELEMETRY
  const double inv = sec > 0.0 ? 1.0 / sec : 0.0;
  const rex::diag::DiagWindowSnapshot r = rex::diag::ExchangeWindowAndReset();
  const double guest_present_hz = r.present_frames * inv;
  const double draws_per_present =
      r.present_frames ? static_cast<double>(r.draws) / r.present_frames : 0.0;
  const double issue_draw_us_pf =
      r.present_frames ? static_cast<double>(r.vk_issue_draw_us) / r.present_frames : 0.0;
  REXLOG_INFO(
      "PROBE RENDER_VK present_hz={:.1f} draws/s={:.1f} draws_per_present={:.1f} "
      "last_present_draws={} max_present_draws={} barriers/s={:.0f} rendering/s={:.0f} "
      "record_us_pf={:.0f} issue_draw_us_pf={:.0f} submit_us_pf={:.0f} gpu_exec_us_avg={:.0f} "
      "bind_skip_pipe={} bind_skip_desc={} tex_reuse_hit={} tex_reuse_miss={}",
      guest_present_hz, r.draws * inv, draws_per_present, r.draws_last_present,
      r.draws_max_per_present, r.barriers * inv, r.rendering_begins * inv,
      r.present_frames ? static_cast<double>(r.vk_record_us) / r.present_frames : 0.0,
      issue_draw_us_pf,
      r.present_frames ? static_cast<double>(r.vk_submit_us) / r.present_frames : 0.0,
      r.vk_gpu_exec_samples ? static_cast<double>(r.vk_gpu_exec_us) / r.vk_gpu_exec_samples : 0.0,
      r.vk_bind_pipeline_skipped, r.vk_bind_descriptor_skipped, r.vk_texture_descriptor_reuse_hits,
      r.vk_texture_descriptor_reuse_misses);
  const double pf = r.present_frames ? 1.0 / r.present_frames : 0.0;
  const uint32_t* rp = r.rp_end_reasons;
  REXLOG_INFO(
      "PROBE RP_BREAK per_present: begins={:.1f} (rp={:.1f} dyn={:.1f}) ends: shm_upload={:.1f} "
      "texture={:.1f} resolve={:.1f} rt_cache={:.1f} barrier={:.1f} fb_switch={:.1f} "
      "submission={:.1f} primitive={:.1f} unknown={:.1f} | shm_uploads={:.1f} hoisted={:.1f} shm_kib={:.1f}",
      (r.render_pass_begins + r.rendering_begins) * pf, r.render_pass_begins * pf,
      r.rendering_begins * pf, rp[rex::diag::kRpEndShmUpload] * pf,
      rp[rex::diag::kRpEndTextureLoad] * pf, rp[rex::diag::kRpEndResolve] * pf,
      rp[rex::diag::kRpEndRtCache] * pf, rp[rex::diag::kRpEndBarrier] * pf,
      rp[rex::diag::kRpEndFbSwitch] * pf, rp[rex::diag::kRpEndSubmission] * pf,
      rp[rex::diag::kRpEndPrimitive] * pf, rp[rex::diag::kRpEndUnknown] * pf, r.shm_uploads * pf, r.shm_uploads_hoisted * pf,
      double(r.shm_upload_bytes) / 1024.0 * pf);
#endif
  (void)frame_us;
}

}  // namespace

extern "C" void Sonic4NotePaceExit(int, uint64_t, uint64_t) {}
extern "C" int Sonic4PaceRearm() { return 0; }
extern "C" void Sonic4PaceRearmReset() {}
extern "C" void Sonic4NoteTaskCallback(uint32_t) {}

#define SONIC4_PASSTHROUGH(fn)                    \
  extern "C" void __imp__##fn(PPCContext& ctx, uint8_t* base); \
  extern "C" REX_FUNC(fn) { __imp__##fn(ctx, base); }

SONIC4_PASSTHROUGH(sub_821B35C8)
SONIC4_PASSTHROUGH(sub_821C1C80)
SONIC4_PASSTHROUGH(sub_8249CC00)
SONIC4_PASSTHROUGH(sub_8249D308)
SONIC4_PASSTHROUGH(sub_821D30E0)
SONIC4_PASSTHROUGH(sub_82163E00)
SONIC4_PASSTHROUGH(sub_8249ECA8)
SONIC4_PASSTHROUGH(sub_82488738)
SONIC4_PASSTHROUGH(sub_82223E98)
SONIC4_PASSTHROUGH(sub_82488958)
SONIC4_PASSTHROUGH(sub_82488698)
SONIC4_PASSTHROUGH(sub_82223A00)
SONIC4_PASSTHROUGH(sub_82446628)
SONIC4_PASSTHROUGH(sub_821C0E58)
SONIC4_PASSTHROUGH(sub_821C47A8)
SONIC4_PASSTHROUGH(sub_82197F28)
SONIC4_PASSTHROUGH(sub_821A8488)
SONIC4_PASSTHROUGH(sub_8249CF80)
SONIC4_PASSTHROUGH(sub_821C0770)
SONIC4_PASSTHROUGH(sub_821D0020)
SONIC4_PASSTHROUGH(sub_82223E78)
SONIC4_PASSTHROUGH(sub_821C0650)
SONIC4_PASSTHROUGH(sub_821D2478)

extern "C" void __imp__sub_8249E2F8(PPCContext& ctx, uint8_t* base);

extern "C" REX_FUNC(sub_8249E2F8) {
  const uint64_t freq = rex::chrono::Clock::QueryHostTickFrequency();
  const uint64_t t0 = rex::chrono::Clock::QueryHostTickCount();
  rex::kernel::xboxkrnl::PaceGuestMainLoopFrame();
#ifdef REX_RENDER_TELEMETRY
  const uint64_t t_paced = rex::chrono::Clock::QueryHostTickCount();
  g_spike_tracker.OnFrameStart(t_paced, freq, g_last_guest_us);
#endif
  __imp__sub_8249E2F8(ctx, base);
  const uint64_t t1 = rex::chrono::Clock::QueryHostTickCount();
#ifdef REX_RENDER_TELEMETRY
  g_last_guest_us = freq ? (t1 - t_paced) * 1000000ull / freq : 0;
#endif
  const uint64_t us = freq ? (t1 - t0) * 1000000ull / freq : 0;
  g_pace_frames.fetch_add(1, std::memory_order_relaxed);
  g_pace_frame_us.fetch_add(us, std::memory_order_relaxed);
  RetailMaybeLogPace(us);
}

#undef SONIC4_PASSTHROUGH