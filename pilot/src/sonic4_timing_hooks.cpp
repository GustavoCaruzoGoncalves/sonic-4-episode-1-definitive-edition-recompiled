// Observation-only timing probe. Does not modify guest registers.

#include "generated/default/sonic4_pch.h"

#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>

#include <rex/chrono/clock.h>
#include <rex/diag/frame_stats.h>
#include <rex/kernel/xboxkrnl/video.h>
#include <rex/logging.h>

#ifdef REX_DIAG_PROFILE
extern "C" void __imp__sub_821C62C8(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821C4870(PPCContext& ctx, uint8_t* base);
#endif

extern "C" void __imp__sub_8249E2F8(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_8249CC00(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_8249D308(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821B35C8(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821C1C80(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821D30E0(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_82446628(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821C0E58(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821C47A8(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_82197F28(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821A8488(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_8249CF80(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821C0770(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821D0020(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_82223E78(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821C0650(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_8249ECA8(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_82488738(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_82223E98(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_82488958(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_82488698(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_82223A00(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_82163E00(PPCContext& ctx, uint8_t* base);

namespace {

struct Hist {
  std::atomic<uint32_t> calls{0};
  std::atomic<uint32_t> neg{0};
  std::atomic<uint32_t> zero{0};
  std::atomic<uint32_t> lt_1e3{0};
  std::atomic<uint32_t> lt_0_05{0};
  std::atomic<uint32_t> lt_1{0};
  std::atomic<uint32_t> lt_100{0};
  std::atomic<uint32_t> lt_2000{0};
  std::atomic<uint32_t> huge{0};
  std::atomic<uint64_t> f1_bits_sum{0};
  std::atomic<uint32_t> per_frame_max{0};
  std::atomic<uint32_t> sample_bits{0};
  std::array<std::atomic<uint32_t>, 8> caller{};
  std::array<std::atomic<uint32_t>, 8> caller_hits{};
};

Hist g_main;
Hist g_viewport;
Hist g_scene;
Hist g_dispatch_cc;
Hist g_dispatch_d3;
std::atomic<uint32_t> g_viewport_this_frame{0};
std::atomic<uint64_t> g_window_start{0};

// Observation-only: measures the effective guest timebase rate (REX_QUERY_TIMEBASE /
// QueryGuestTickCount) against host wall clock over a 5s window. A healthy guest clock
// advances at ~50 MHz. A value far below that means time is being discarded (e.g. by a
// baseline reset) and the guest clock runs behind real time.
std::atomic<uint64_t> g_clock_window_host{0};
std::atomic<uint64_t> g_clock_window_guest{0};

// Observation-only call-rate counters for the two identified guest timers:
//  - sub_821D30E0: scene timer; writes the per-frame delta (ticks) into field 21792.
//  - sub_82446628: module timer (0x82446/7); computes its own delta and calls sub_82446148.
std::atomic<uint32_t> g_scene_timer_calls{0};
std::atomic<uint32_t> g_module_timer_calls{0};
// Actual tick delta written to world+21792 by sub_821D30E0. Prior runs counted
// the call but never read the stored value.
std::atomic<uint32_t> g_dt21792_n{0};
std::atomic<uint64_t> g_dt21792_sum{0};
std::atomic<uint32_t> g_dt21792_min{0xFFFFFFFFu};
std::atomic<uint32_t> g_dt21792_max{0};
std::atomic<uint32_t> g_dt21792_last{0};

// sub_8249ECA8 walks a task list and indirect-calls task+28. The address is the
// real per-frame callback; static constructors do not show which ones run.
constexpr uint32_t kTaskSlots = 48;
std::atomic<uint32_t> g_task_dispatches{0};
std::atomic<uint32_t> g_task_calls{0};
std::atomic<uint32_t> g_task_dropped{0};
std::array<std::atomic<uint32_t>, kTaskSlots> g_task_addr{};
std::array<std::atomic<uint32_t>, kTaskSlots> g_task_hits{};
Hist g_eca8_hist;
// sub_82488738 writes the task-thread frame interval (object+32) from r4.
std::atomic<uint32_t> g_iv_calls{0};
std::atomic<uint32_t> g_iv_r4_last{0};
std::atomic<uint32_t> g_iv_r4_min{0xFFFFFFFFu};
std::atomic<uint32_t> g_iv_r4_max{0};
std::atomic<uint64_t> g_iv_interval{0};
std::atomic<uint32_t> g_iv_scale_bits{0};
std::atomic<uint32_t> g_iv_lr{0};
// sub_82488958 is the worker-thread frame wait. 82488698 is its deadline path.
std::atomic<uint32_t> g_pace_frames{0};
std::atomic<uint64_t> g_pace_frame_us{0};
std::atomic<uint32_t> g_pace_sync{0};
std::atomic<uint64_t> g_pace_sync_us{0};
std::atomic<uint32_t> g_pace_evt_ok{0};
std::atomic<uint32_t> g_pace_evt_to{0};
std::atomic<int64_t> g_pace_entry_sum{0};
std::atomic<int64_t> g_pace_entry_min{INT64_MAX};
std::atomic<int64_t> g_pace_entry_max{INT64_MIN};
std::atomic<int64_t> g_pace_exit_sum{0};
std::atomic<uint32_t> g_pace_exit_past{0};
std::atomic<uint32_t> g_pace_us_min{0xFFFFFFFFu};
std::atomic<uint32_t> g_pace_us_max{0};
std::atomic<uint32_t> g_pace_us_short{0};
std::atomic<uint32_t> g_pace_us_long{0};
std::atomic<int64_t> g_pace_short_entry{0};
std::atomic<int64_t> g_pace_long_entry{0};
std::atomic<int64_t> g_pace_short_exit{0};
std::atomic<int64_t> g_pace_long_exit{0};
std::atomic<uint32_t> g_pace_exit_signal{0};
std::atomic<uint32_t> g_pace_exit_deadline{0};
std::atomic<int64_t> g_pace_deadline_cmp{0};
std::atomic<int64_t> g_sig_remain_sum{0};
std::atomic<uint32_t> g_sig_n{0};

// T7_SPEED_21 Â§4 observation-only probes:
//  - sub_821C0E58 (frame update) classified by return address (ctx.lr):
//      0x8249E418 = main-loop path A (r5 = world+13852 queue)
//      0x8249E44C = main-loop path B (r5 = 0, r4 = table[idx])
//      0x821D44EC = worker thread sub_821D43E0 (r4 = world+15056, r5 = 0)
//    plus gate byte world+10942 bit2 and wall-clock duration per call.
//  - sub_821C47A8 (delta consumer): poll delta (ticks), descriptor mode,
//    callback pointer at world+13644 and the scale/const floats.
//  - sub_82197F28 (scene path under state==3, reached 2x/frame from master loop).
std::atomic<uint32_t> g_fu_mainA{0};
std::atomic<uint32_t> g_fu_mainB{0};
std::atomic<uint32_t> g_fu_thread{0};
std::atomic<uint32_t> g_fu_other{0};
std::atomic<uint32_t> g_fu_gate2{0};
std::atomic<uint64_t> g_fu_us_sum{0};
std::atomic<uint32_t> g_fu_us_max{0};
Hist g_fu_hist;  // caller LR slots for sub_821C0E58 beyond the three known sites

std::atomic<uint32_t> g_poll_calls{0};
std::atomic<uint32_t> g_poll_mode0{0};
std::atomic<uint32_t> g_poll_mode3{0};
std::atomic<uint32_t> g_poll_mode_other{0};
std::atomic<uint64_t> g_poll_sum{0};
std::atomic<uint32_t> g_poll_max{0};
std::atomic<uint32_t> g_poll_cb_nonzero{0};
std::atomic<uint64_t> g_poll_cb_ptr{0};
std::atomic<uint32_t> g_poll_reported{0};
Hist g_poll_hist;  // caller LR slots for sub_821C47A8 (which stampers actually run)

std::atomic<uint32_t> g_sceneupd_calls{0};
Hist g_sceneupd_hist;  // caller LR slots for sub_82197F28
std::atomic<uint32_t> g_scenedisp_calls{0};
Hist g_scenedisp_hist;  // caller LR slots for sub_821A8488 (scene dispatcher)
std::atomic<uint32_t> g_objupd_calls{0};
Hist g_objupd_hist;  // caller LR slots for sub_8249CF80 (per-object update)
std::atomic<uint32_t> g_objupd_state_seen{0xFFFFFFFFu};

#ifdef REX_DIAG_PROFILE
std::atomic<uint64_t> g_scenedisp_us_sum{0};
std::atomic<uint64_t> g_sceneupd_us_sum{0};
std::atomic<uint64_t> g_objupd_us_sum{0};
std::atomic<uint32_t> g_guest_821c62c8_calls{0};
std::atomic<uint64_t> g_guest_821c62c8_us_sum{0};
std::atomic<uint32_t> g_guest_821c4870_calls{0};
std::atomic<uint64_t> g_guest_821c4870_us_sum{0};

uint64_t HostUsNow() {
  const uint64_t ticks = rex::chrono::Clock::QueryHostTickCount();
  const uint64_t freq = rex::chrono::Clock::QueryHostTickFrequency();
  return freq ? (ticks * 1'000'000ull) / freq : 0;
}
#endif

// T7_SPEED_21 Â§4: guest-side frame pacing sub_821C0770 (target fps world+21772).
std::atomic<uint32_t> g_pace_calls{0};
std::atomic<uint32_t> g_pace_target{0};

// T7_SPEED_23: sub_821C0770 is the guest frame SCHEDULER. It converts elapsed ticks
// (mftb - world[16764]) into a number of target-frames (r7) and bumps the frame
// counters. If it ever yields >1 frame per call, gameplay advances extra steps ->
// sudden speedup. Record the tick baseline, counters, and inferred frames-per-call.
std::atomic<uint32_t> g_sched_calls{0};
std::atomic<uint32_t> g_sched_frames_gt1{0};   // estimates where frames>1
std::atomic<uint64_t> g_sched_max_delta_ticks{0};
std::atomic<uint32_t> g_sched_last_16764{0};
std::atomic<uint32_t> g_sched_16760{0};
std::atomic<uint32_t> g_sched_16768{0};
std::atomic<uint32_t> g_sched_16776{0};
std::atomic<uint32_t> g_sched_16784{0};
std::atomic<uint32_t> g_sched_prev_16784{0};
std::atomic<uint32_t> g_sched_queue_jumps{0};  // 16784 advanced by >1 in one call
std::atomic<uint32_t> g_sched_world{0};
// T7_SPEED_33: measure the scheduler's actual WORK-PRODUCTION rate, not just its call rate.
// In sub_821C0770 the argument (r3) decomposes as:
//   r29 = (r3 >> 24) & 0xF   (requested frame advance hint, drives the "+1 catch-up" step)
//   r27 =  r3        & 0xFF  (comparison threshold for that +1)
// When a frame is queued for processing, world[16920] (the queue producer index) increments
// and world[16792] receives the new target frame. So (world[16920] delta per second) is the
// number of game frames actually submitted for processing per second. If that exceeds the
// vblank rate (60/s) the game steps faster than the display -> acceleration.
std::atomic<uint32_t> g_sched_arg_last{0};
std::atomic<uint32_t> g_sched_qidx_last{0};
std::atomic<uint32_t> g_sched_qidx_prev{0};
std::atomic<uint32_t> g_sched_qidx_max{0};
std::atomic<uint32_t> g_sched_queued{0};      // calls where 16920 changed (work queued)
std::atomic<uint32_t> g_sched_r29_nonzero{0}; // calls with r29 != 0 (catch-up eligible)
std::array<std::atomic<uint32_t>, 16> g_sched_r29_hist{};
std::atomic<uint32_t> g_sched_r27_last{0};

// T7_SPEED_24: sub_821D0020 returns `(now_ms - world[21892])` - the game's
// "elapsed time since last frame" query, i.e. the gameplay dt source. Measure its
// return distribution and pick up who calls it (indirect - LR note).
std::atomic<uint32_t> g_dt_calls{0};
std::atomic<uint64_t> g_dt_sum{0};
std::atomic<uint32_t> g_dt_max{0};
std::atomic<uint32_t> g_dt_zero{0};
std::atomic<uint32_t> g_dt_big{0};   // >= 50 ms (multiple frames)
std::atomic<uint32_t> g_dt_last{0};
std::atomic<uint32_t> g_dt_21892{0};
std::atomic<uint32_t> g_dt_21892_changes{0};
Hist g_dt_hist;

// T7_SPEED_25: the engine time service sub_821D2478 (switch 0..19) and the raw
// per-frame tick delta world[21792] it consumes. world[21792] is set once per frame by
// sub_821D30E0 from `mftb - world[21800/21796]`; scaled by world[21808]=2e-8 -> seconds.
// If world[21792] ~ 833333 the dt is 1/60s (correct); a larger value => gameplay dt too
// big => everything moves faster. Also sample the value the service returns (f1) and the
// requested case (r4) so we can see which gameplay units are being fed.
extern "C" void __imp__sub_821D2478(PPCContext& ctx, uint8_t* base);
std::atomic<uint32_t> g_tc_calls{0};
std::atomic<uint32_t> g_tc_21792_last{0};
std::atomic<uint32_t> g_tc_21792_max{0};
std::atomic<uint64_t> g_tc_21792_sum{0};
std::atomic<uint32_t> g_tc_case_seen{0xFFFFFFFFu};
std::atomic<uint32_t> g_tc_case_mask_lo{0};
std::atomic<uint32_t> g_tc_ret_bits{0};
std::array<std::atomic<uint32_t>, 24> g_tc_case_hits{};
std::array<std::atomic<uint32_t>, 24> g_tc_case_retbits{};

// T7_SPEED_26: ground-truth gameplay motion. sub_8249CF80's object is obj=[r3+8].
// Snapshot the first N dwords of that object at the end of every window and, on the next
// window, report which slots changed and by how much (as float and as int). A slot that
// grows smoothly is a position/coordinate. This tells us if/how fast objects advance.
std::atomic<uint32_t> g_obj_last_ptr{0};
std::array<uint32_t, 128> g_obj_snap{};
std::atomic<uint32_t> g_obj_snap_valid{0};

// T7_SPEED_26b: per-update trace of the latched object's candidate position fields.
// Ring buffer of the last few values seen, reported with host timestamps so we can see a
// smooth per-frame delta (real position) vs a rewrite (command object).
struct ObjTrace {
  static constexpr uint32_t kCap = 24;
  std::atomic<uint32_t> n{0};
  std::atomic<uint64_t> f16[kCap];
  std::atomic<uint32_t> v16[kCap];
  std::atomic<uint32_t> v24[kCap];
  std::atomic<uint32_t> v44[kCap];
};
ObjTrace g_obj_trace;

// T7_SPEED_29: per-frame displacement meter for the tracked object. Accumulates the
// magnitude of the object's world-space position delta per frame over a wall-clock window
// and reports world-units/second. Comparing "normal" vs "accelerated" segments of the same
// level gives the real speed ratio without needing to identify the object symbolically.
struct MoveRate {
  std::atomic<uint32_t> frames{0};
  std::atomic<uint64_t> dist_bits_sum{0};   // sum of per-frame |delta| as double bits
  std::atomic<uint64_t> window_host_start{0};
  std::atomic<uint32_t> last_valid{0};
  std::atomic<uint32_t> last_x{0};
  std::atomic<uint32_t> last_z{0};
  std::atomic<uint32_t> moved_frames{0};    // frames with a non-trivial delta
  std::atomic<uint32_t> max_step_bits{0};   // max |delta| in one frame
};
MoveRate g_move;

// T7_SPEED_27: world-struct latch detector. Snapshot a window of the world struct each
// window and print every word that changed since the previous window. Correlating the
// words that change exactly at the user-marked "normal -> accelerated" transition and then
// STAY changed identifies the latched state flag / wrong constant.
constexpr uint32_t kWorldScanWords = 8192;  // 32 KB from world base
std::array<uint32_t, kWorldScanWords> g_world_snap{};   // previous window
std::array<uint32_t, kWorldScanWords> g_world_snap2{};  // window before that
std::atomic<uint32_t> g_world_snap_valid{0};

// T7_SPEED_22: per-main-loop-iteration guest tick delta. Measures whether
// QueryGuestTickCount() advances smoothly (~832k ticks at 60 Hz / 50 MHz) or JUMPs.
// A jump means the game sees a large elapsed time -> extra simulation steps -> sudden
// speedup (matches user report: idle in same area, suddenly accelerated).
std::atomic<uint32_t> g_ct_samples{0};
std::atomic<uint64_t> g_ct_prev{0};
std::atomic<uint32_t> g_ct_below{0};     // delta < 0.5 expected (clock stalled/stale return)
std::atomic<uint32_t> g_ct_near{0};      // 0.5x .. 1.5x expected (healthy)
std::atomic<uint32_t> g_ct_high{0};      // 1.5x .. 3x expected (mild jump)
std::atomic<uint32_t> g_ct_jump{0};      // > 3x expected (big jump forward)
std::atomic<uint64_t> g_ct_maxdelta{0};
std::atomic<uint32_t> g_ct_host_below{0};  // host delta < 0.5 frame (loop ran early)

void ProbeClockRate() {
  const uint64_t host_now = rex::chrono::Clock::QueryHostTickCount();
  const uint64_t host_freq = rex::chrono::Clock::QueryHostTickFrequency();
  const uint64_t guest_now = rex::chrono::Clock::QueryGuestTickCount();
  uint64_t host_start = g_clock_window_host.load(std::memory_order_relaxed);
  if (!host_start) {
    g_clock_window_host.store(host_now, std::memory_order_relaxed);
    g_clock_window_guest.store(guest_now, std::memory_order_relaxed);
    return;
  }
  if (host_now - host_start < host_freq * 5) {
    return;
  }
  const uint64_t guest_start = g_clock_window_guest.load(std::memory_order_relaxed);
  const double host_sec = static_cast<double>(host_now - host_start) / static_cast<double>(host_freq);
  const double guest_ticks = static_cast<double>(guest_now - guest_start);
  const double guest_hz = host_sec > 0.0 ? guest_ticks / host_sec : 0.0;
  REXLOG_INFO(
      "CLOCKPROBE host {:.3f}s guest_ticks {:.0f} guest_effective_hz {:.0f} (target 50000000) "
      "ratio_vs_target {:.4f}",
      host_sec, guest_ticks, guest_hz, guest_hz / 50000000.0);
  g_clock_window_host.store(host_now, std::memory_order_relaxed);
  g_clock_window_guest.store(guest_now, std::memory_order_relaxed);
}

int Bucket(float v, Hist& h) {
  if (v < 0.0f) {
    h.neg.fetch_add(1, std::memory_order_relaxed);
    return 0;
  }
  if (v == 0.0f) {
    h.zero.fetch_add(1, std::memory_order_relaxed);
    return 1;
  }
  if (v < 0.001f) {
    h.lt_1e3.fetch_add(1, std::memory_order_relaxed);
    return 2;
  }
  if (v < 0.05f) {
    h.lt_0_05.fetch_add(1, std::memory_order_relaxed);
    return 3;
  }
  if (v < 1.0f) {
    h.lt_1.fetch_add(1, std::memory_order_relaxed);
    return 4;
  }
  if (v < 100.0f) {
    h.lt_100.fetch_add(1, std::memory_order_relaxed);
    return 5;
  }
  if (v < 2000.0f) {
    h.lt_2000.fetch_add(1, std::memory_order_relaxed);
    return 6;
  }
  h.huge.fetch_add(1, std::memory_order_relaxed);
  return 7;
}

void NoteCaller(Hist& h, uint32_t lr) {
  for (size_t i = 0; i < h.caller.size(); ++i) {
    uint32_t cur = h.caller[i].load(std::memory_order_relaxed);
    if (cur == lr) {
      h.caller_hits[i].fetch_add(1, std::memory_order_relaxed);
      return;
    }
    if (cur == 0 && h.caller[i].compare_exchange_strong(cur, lr, std::memory_order_relaxed)) {
      h.caller_hits[i].fetch_add(1, std::memory_order_relaxed);
      return;
    }
  }
}

void Observe(Hist& h, const PPCContext& ctx) {
  h.calls.fetch_add(1, std::memory_order_relaxed);
  const float f1 = static_cast<float>(ctx.f1.f64);
  Bucket(f1, h);
  uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(f1));
  std::memcpy(&bits, &f1, sizeof(bits));
  uint32_t expected = 0;
  h.sample_bits.compare_exchange_strong(expected, bits, std::memory_order_relaxed);
  NoteCaller(h, ctx.lr);
}

void MaybeLog(uint8_t* base) {
  const uint64_t now = rex::chrono::Clock::QueryHostTickCount();
  const uint64_t freq = rex::chrono::Clock::QueryHostTickFrequency();
  uint64_t start = g_window_start.load(std::memory_order_relaxed);
  if (!start) {
    g_window_start.store(now, std::memory_order_relaxed);
    return;
  }
  if (now - start < freq * 5) {
    return;
  }
  const double sec = static_cast<double>(now - start) / static_cast<double>(freq);
  auto dump = [&](const char* name, Hist& h) {
    const uint32_t calls = h.calls.exchange(0, std::memory_order_relaxed);
    const uint32_t sample = h.sample_bits.exchange(0, std::memory_order_relaxed);
    float sample_f = 0.0f;
    std::memcpy(&sample_f, &sample, sizeof(sample_f));
    REXLOG_INFO(
        "PROBE {} {:.1f}/s sample_f1={:.6f} f1 buckets neg={} zero={} <0.001={} <0.05={} <1={} <100={} "
        "<2000={} huge={} callers {:08X}:{} {:08X}:{} {:08X}:{} {:08X}:{}",
        name, sec > 0.0 ? calls / sec : 0.0, sample_f, h.neg.exchange(0), h.zero.exchange(0),
        h.lt_1e3.exchange(0), h.lt_0_05.exchange(0), h.lt_1.exchange(0), h.lt_100.exchange(0),
        h.lt_2000.exchange(0), h.huge.exchange(0), h.caller[0].exchange(0),
        h.caller_hits[0].exchange(0), h.caller[1].exchange(0), h.caller_hits[1].exchange(0),
        h.caller[2].exchange(0), h.caller_hits[2].exchange(0), h.caller[3].exchange(0),
        h.caller_hits[3].exchange(0));
  };
  const uint32_t main_calls = g_main.calls.load(std::memory_order_relaxed);
  const uint32_t viewport_calls = g_viewport.calls.load(std::memory_order_relaxed);
  REXLOG_INFO("PROBE ratio viewport/main {:.2f} max viewport calls in one main iteration {}",
              main_calls ? static_cast<double>(viewport_calls) / main_calls : 0.0,
              g_viewport.per_frame_max.exchange(0));
  dump("8249E2F8", g_main);
  dump("821B35C8", g_viewport);
  dump("821C1C80", g_scene);
  dump("8249CC00", g_dispatch_cc);
  dump("8249D308", g_dispatch_d3);
  const uint32_t scene_timer = g_scene_timer_calls.exchange(0, std::memory_order_relaxed);
  const uint32_t module_timer = g_module_timer_calls.exchange(0, std::memory_order_relaxed);
  REXLOG_INFO("TIMERS scene_821D30E0 {:.1f}/s module_82446628 {:.1f}/s (vs main {:.1f}/s)",
              sec > 0.0 ? scene_timer / sec : 0.0, sec > 0.0 ? module_timer / sec : 0.0,
              sec > 0.0 ? g_main.calls.load(std::memory_order_relaxed) / sec : 0.0);
  {
    const uint32_t n = g_dt21792_n.exchange(0, std::memory_order_relaxed);
    const uint64_t sum = g_dt21792_sum.exchange(0, std::memory_order_relaxed);
    const uint32_t dmin = g_dt21792_min.exchange(0xFFFFFFFFu, std::memory_order_relaxed);
    const uint32_t dmax = g_dt21792_max.exchange(0, std::memory_order_relaxed);
    const double sim_sec = static_cast<double>(sum) / 50000000.0;
    REXLOG_INFO(
        "PROBE DT21792 n={} avg={} min={} max={} last={} sim_sec={:.4f} sim_ratio={:.3f}",
        n, n ? static_cast<uint32_t>(sum / n) : 0, n ? dmin : 0, dmax,
        g_dt21792_last.load(std::memory_order_relaxed), sim_sec,
        sec > 0.0 ? sim_sec / sec : 0.0);
  }
  {
    const uint32_t dispatches = g_task_dispatches.exchange(0, std::memory_order_relaxed);
    const uint32_t calls = g_task_calls.exchange(0, std::memory_order_relaxed);
    const uint32_t dropped = g_task_dropped.exchange(0, std::memory_order_relaxed);
    struct Slot {
      uint32_t addr;
      uint32_t hits;
    };
    Slot top[12]{};
    for (uint32_t i = 0; i < kTaskSlots; ++i) {
      const uint32_t hits = g_task_hits[i].exchange(0, std::memory_order_relaxed);
      const uint32_t addr = g_task_addr[i].load(std::memory_order_relaxed);
      if (addr == 0 || hits == 0) {
        continue;
      }
      Slot s{addr, hits};
      for (uint32_t k = 0; k < 12; ++k) {
        if (s.hits > top[k].hits) {
          const Slot tmp = top[k];
          top[k] = s;
          s = tmp;
        }
      }
    }
    char buf[320];
    size_t off = 0;
    for (uint32_t k = 0; k < 12 && top[k].hits != 0; ++k) {
      off += static_cast<size_t>(std::snprintf(
          buf + off, sizeof(buf) - off, "%08X:%u ", top[k].addr, top[k].hits));
    }
    REXLOG_INFO("PROBE TASKCB disp/s={:.1f} calls/s={:.1f} dropped={} top[{}]",
                dispatches * (sec > 0.0 ? 1.0 / sec : 0.0),
                calls * (sec > 0.0 ? 1.0 / sec : 0.0), dropped, buf);
    dump("ECA8_LR", g_eca8_hist);
  }
  {
    const uint32_t n = g_iv_calls.exchange(0, std::memory_order_relaxed);
    const uint64_t interval = g_iv_interval.load(std::memory_order_relaxed);
    float scale = 0.0f;
    const uint32_t sb = g_iv_scale_bits.load(std::memory_order_relaxed);
    std::memcpy(&scale, &sb, sizeof(scale));
    const double hz = interval ? 50000000.0 / static_cast<double>(interval) : 0.0;
    REXLOG_INFO(
        "PROBE INTERVAL n={:.1f}/s r4_last={} r4_min={} r4_max={} interval={} "
        "interval_hz={:.2f} scale={:.6g} lr={:08X}",
        n * (sec > 0.0 ? 1.0 / sec : 0.0), g_iv_r4_last.load(std::memory_order_relaxed),
        g_iv_r4_min.exchange(0xFFFFFFFFu, std::memory_order_relaxed),
        g_iv_r4_max.exchange(0, std::memory_order_relaxed), interval, hz, scale,
        g_iv_lr.load(std::memory_order_relaxed));
  }
  {
    const uint32_t frames = g_pace_frames.exchange(0, std::memory_order_relaxed);
    const uint64_t frame_us = g_pace_frame_us.exchange(0, std::memory_order_relaxed);
    const uint32_t sync = g_pace_sync.exchange(0, std::memory_order_relaxed);
    const uint64_t sync_us = g_pace_sync_us.exchange(0, std::memory_order_relaxed);
    const double inv = sec > 0.0 ? 1.0 / sec : 0.0;
    REXLOG_INFO(
        "PROBE PACE frames={:.1f}/s frame_us={:.0f} sync={:.1f}/s sync_us={:.0f} "
        "evt_signaled={} evt_timeout={}",
        frames * inv, frames ? static_cast<double>(frame_us) / frames : 0.0, sync * inv,
        sync ? static_cast<double>(sync_us) / sync : 0.0,
        g_pace_evt_ok.exchange(0, std::memory_order_relaxed),
        g_pace_evt_to.exchange(0, std::memory_order_relaxed));
    const int64_t entry_sum = g_pace_entry_sum.exchange(0, std::memory_order_relaxed);
    const int64_t exit_sum = g_pace_exit_sum.exchange(0, std::memory_order_relaxed);
    REXLOG_INFO(
        "PROBE DEADLINE entry_avg={:.0f} entry_min={} entry_max={} exit_avg={:.0f} "
        "exit_past={} (ticks, 833333 = 1/60s)",
        frames ? static_cast<double>(entry_sum) / frames : 0.0,
        g_pace_entry_min.exchange(INT64_MAX, std::memory_order_relaxed),
        g_pace_entry_max.exchange(INT64_MIN, std::memory_order_relaxed),
        frames ? static_cast<double>(exit_sum) / frames : 0.0,
        g_pace_exit_past.exchange(0, std::memory_order_relaxed));
    const uint32_t short_n = g_pace_us_short.exchange(0, std::memory_order_relaxed);
    const uint32_t long_n = g_pace_us_long.exchange(0, std::memory_order_relaxed);
    const uint32_t exit_deadline_n = g_pace_exit_deadline.exchange(0, std::memory_order_relaxed);
    const int64_t cmp_sum = g_pace_deadline_cmp.exchange(0, std::memory_order_relaxed);
    REXLOG_INFO("PROBE WAITSHAPE us_min={} us_max={} short<3ms={} long>12ms={} "
                "short_entry={:.0f} short_exit={:.0f} long_entry={:.0f} long_exit={:.0f} "
                "exit_signal={} exit_deadline={} deadline_minus_now={:.0f}",
                g_pace_us_min.exchange(0xFFFFFFFFu, std::memory_order_relaxed),
                g_pace_us_max.exchange(0, std::memory_order_relaxed), short_n, long_n,
                short_n ? static_cast<double>(g_pace_short_entry.exchange(0)) / short_n : 0.0,
                short_n ? static_cast<double>(g_pace_short_exit.exchange(0)) / short_n : 0.0,
                long_n ? static_cast<double>(g_pace_long_entry.exchange(0)) / long_n : 0.0,
                long_n ? static_cast<double>(g_pace_long_exit.exchange(0)) / long_n : 0.0,
                g_pace_exit_signal.exchange(0, std::memory_order_relaxed), exit_deadline_n,
                exit_deadline_n ? static_cast<double>(cmp_sum) / exit_deadline_n : 0.0);
    const uint32_t sig_n = g_sig_n.exchange(0, std::memory_order_relaxed);
    REXLOG_INFO("PROBE SIGNAL n={:.1f}/s remain_avg={:.0f}", sig_n * (sec > 0.0 ? 1.0 / sec : 0.0),
                sig_n ? static_cast<double>(g_sig_remain_sum.exchange(0, std::memory_order_relaxed)) / sig_n
                      : 0.0);
  }
  // T7_SPEED_21 Â§4: frame-update caller mix, poll-delta stats, scene-path rate, state gate.
  const double inv = sec > 0.0 ? 1.0 / sec : 0.0;
  const uint32_t fuA = g_fu_mainA.exchange(0, std::memory_order_relaxed);
  const uint32_t fuB = g_fu_mainB.exchange(0, std::memory_order_relaxed);
  const uint32_t fuT = g_fu_thread.exchange(0, std::memory_order_relaxed);
  const uint32_t fuO = g_fu_other.exchange(0, std::memory_order_relaxed);
  const uint32_t fuG = g_fu_gate2.exchange(0, std::memory_order_relaxed);
  const uint64_t fuUs = g_fu_us_sum.exchange(0, std::memory_order_relaxed);
  const uint32_t fuTot = fuA + fuB + fuT + fuO;
  REXLOG_INFO(
      "PROBE FRAMEUPD/s mainA={:.1f} mainB={:.1f} thread={:.1f} other={:.1f} gate2={:.1f} "
      "dur_us avg={:.1f} max={}",
      fuA * inv, fuB * inv, fuT * inv, fuO * inv, fuG * inv,
      fuTot > 0 ? static_cast<double>(fuUs) / fuTot : 0.0,
      g_fu_us_max.exchange(0, std::memory_order_relaxed));
  dump("FRAMEUPD_LR", g_fu_hist);
  const uint32_t pollC = g_poll_calls.exchange(0, std::memory_order_relaxed);
  const uint64_t pollSum = g_poll_sum.exchange(0, std::memory_order_relaxed);
  REXLOG_INFO(
      "PROBE POLL/s={:.1f} avg_ticks={} max_ticks={} mode0={:.1f} mode3={:.1f} other_mode={:.1f} "
      "cb_nonzero={:.1f} cb={:08X}",
      pollC * inv, pollC > 0 ? static_cast<uint32_t>(pollSum / pollC) : 0,
      g_poll_max.exchange(0, std::memory_order_relaxed), g_poll_mode0.exchange(0) * inv,
      g_poll_mode3.exchange(0) * inv, g_poll_mode_other.exchange(0) * inv,
      g_poll_cb_nonzero.exchange(0) * inv, g_poll_cb_ptr.load(std::memory_order_relaxed));
  dump("POLL_LR", g_poll_hist);
  const uint32_t sceneU = g_sceneupd_calls.exchange(0, std::memory_order_relaxed);
  const uint32_t state = REX_LOAD_U32(0x82585E40u);
  REXLOG_INFO("PROBE SCENEUPD_82197F28/s={:.1f} STATE[82585E40]={}", sceneU * inv, state);
  dump("SCENEUPD_LR", g_sceneupd_hist);
  const uint32_t dispU = g_scenedisp_calls.exchange(0, std::memory_order_relaxed);
  REXLOG_INFO("PROBE SCENEDISP_821A8488/s={:.1f}", dispU * inv);
  dump("SCENEDISP_LR", g_scenedisp_hist);
  const uint32_t objU = g_objupd_calls.exchange(0, std::memory_order_relaxed);
  REXLOG_INFO("PROBE OBJUPD_8249CF80/s={:.1f} first_state={}", objU * inv,
              g_objupd_state_seen.load(std::memory_order_relaxed));
  dump("OBJUPD_LR", g_objupd_hist);
  const uint32_t paceU = g_pace_calls.exchange(0, std::memory_order_relaxed);
  REXLOG_INFO("PROBE PACE_821C0770/s={:.1f} target_fps={}", paceU * inv,
              g_pace_target.load(std::memory_order_relaxed));
  // T7_SPEED_23: frame-scheduler multi-step detector.
  const uint32_t schCalls = g_sched_calls.exchange(0, std::memory_order_relaxed);
  const uint32_t schFrames = g_sched_frames_gt1.exchange(0, std::memory_order_relaxed);
  const uint32_t schJumps = g_sched_queue_jumps.exchange(0, std::memory_order_relaxed);
  REXLOG_INFO(
      "PROBE SCHED_821C0770 world={:08X} n={} frames_gt1={} queue_jumps={} max_delta_ticks={} "
      "f16760={} f16764={} f16768={} f16776={} f16784={}",
      g_sched_world.load(std::memory_order_relaxed), schCalls, schFrames, schJumps,
      g_sched_max_delta_ticks.exchange(0, std::memory_order_relaxed),
      g_sched_16760.load(std::memory_order_relaxed), g_sched_last_16764.load(std::memory_order_relaxed),
      g_sched_16768.load(std::memory_order_relaxed), g_sched_16776.load(std::memory_order_relaxed),
      g_sched_16784.load(std::memory_order_relaxed));
  // T7_SPEED_33: work-production rate. queue_delta = how many frames the scheduler submitted
  // for processing during this window; /sec should track the vblank rate (60/s) if the game is
  // stepping at the intended rate. r29!=0 count tells us how often the "+1 catch-up" path was
  // eligible, which is the mechanism that could push the step rate above 60/s.
  {
    const uint32_t qidx = g_sched_qidx_last.load(std::memory_order_relaxed);
    const uint32_t qprev = g_sched_qidx_prev.exchange(qidx, std::memory_order_relaxed);
    const uint32_t r29nz = g_sched_r29_nonzero.exchange(0, std::memory_order_relaxed);
    char r29buf[160];
    size_t o = 0;
    for (uint32_t i = 0; i < 16; i++) {
      const uint32_t v = g_sched_r29_hist[i].exchange(0, std::memory_order_relaxed);
      o += static_cast<size_t>(std::snprintf(r29buf + o, sizeof(r29buf) - o, "%u:%u ", i, v));
    }
    REXLOG_INFO(
        "PROBE SCHEDQ/s calls={:.1f} qidx={} queue_delta={} queue_delta_per_s={:.1f} "
        "r29_nonzero={} r27_last={} arg_last={:08X} r29_hist[{}]",
        schCalls * inv, qidx, qprev ? (qidx - qprev) : 0, qprev ? (qidx - qprev) * inv : 0.0,
        r29nz, g_sched_r27_last.load(std::memory_order_relaxed),
        g_sched_arg_last.load(std::memory_order_relaxed), r29buf);
  }

  // T7_SPEED_24: gameplay frame-delta query sub_821D0020.
  const uint32_t dtCalls = g_dt_calls.exchange(0, std::memory_order_relaxed);
  if (dtCalls > 0) {
    const uint64_t dtSum = g_dt_sum.exchange(0, std::memory_order_relaxed);
    REXLOG_INFO(
        "PROBE DT_821D0020/s={:.1f} n={} avg_ms={} max_ms={} zero={} big50={} last={} "
        "w21892={} w21892_changes={}",
        dtCalls * inv, dtCalls, dtCalls ? static_cast<uint32_t>(dtSum / dtCalls) : 0,
        g_dt_max.exchange(0, std::memory_order_relaxed), g_dt_zero.exchange(0),
        g_dt_big.exchange(0), g_dt_last.load(std::memory_order_relaxed),
        g_dt_21892.load(std::memory_order_relaxed),
        g_dt_21892_changes.exchange(0, std::memory_order_relaxed));
    dump("DT_LR", g_dt_hist);
  }

  // T7_SPEED_25: engine time service + raw per-frame tick delta.
  const uint32_t tcCalls = g_tc_calls.exchange(0, std::memory_order_relaxed);
  if (tcCalls > 0) {
    const uint64_t tcSum = g_tc_21792_sum.exchange(0, std::memory_order_relaxed);
    float ret_f = 0.0f;
    const uint32_t rb = g_tc_ret_bits.load(std::memory_order_relaxed);
    std::memcpy(&ret_f, &rb, sizeof(ret_f));
    // Build a compact "cases hit" string for the 20 switch cases.
    char cases[160] = {0};
    size_t off = 0;
    for (uint32_t i = 0; i < 20; ++i) {
      const uint32_t h = g_tc_case_hits[i].exchange(0, std::memory_order_relaxed);
      if (h) {
        off += static_cast<size_t>(std::snprintf(cases + off, sizeof(cases) - off, "%u:%u ", i, h));
      }
    }
    REXLOG_INFO(
        "PROBE TIMESVC_821D2478/s={:.1f} n={} case_first={} 21792_avg={} 21792_max={} "
        "21792_last={} ret_sample={:.8f} cases[{}]",
        tcCalls * inv, tcCalls, g_tc_case_seen.load(std::memory_order_relaxed),
        tcCalls ? static_cast<uint32_t>(tcSum / tcCalls) : 0,
        g_tc_21792_max.exchange(0, std::memory_order_relaxed),
        g_tc_21792_last.load(std::memory_order_relaxed), ret_f, cases);
    g_tc_ret_bits.store(0, std::memory_order_relaxed);
  }

  // T7_SPEED_26b: dump the object position trace (index, host_ms, +16, +24, +44 as floats).
  {
    const uint32_t tn = g_obj_trace.n.load(std::memory_order_relaxed);
    if (tn > 0) {
      const uint32_t count = tn < ObjTrace::kCap ? tn : ObjTrace::kCap;
      const uint32_t start = tn >= ObjTrace::kCap ? tn % ObjTrace::kCap : 0;
      char buf[900];
      size_t off = 0;
      for (uint32_t k = 0; k < count; ++k) {
        const uint32_t idx = (start + k) % ObjTrace::kCap;
        float a = 0, b = 0, c = 0;
        const uint32_t ba = g_obj_trace.v16[idx].load(std::memory_order_relaxed);
        const uint32_t bb = g_obj_trace.v24[idx].load(std::memory_order_relaxed);
        const uint32_t bc = g_obj_trace.v44[idx].load(std::memory_order_relaxed);
        std::memcpy(&a, &ba, 4);
        std::memcpy(&b, &bb, 4);
        std::memcpy(&c, &bc, 4);
        off += static_cast<size_t>(std::snprintf(buf + off, sizeof(buf) - off,
                                                 "%.3f/%.3f/%.3f ", a, b, c));
      }
      REXLOG_INFO("PROBE OBJTRACE n={} seq +16/+24/+44[{}]", tn, buf);
      g_obj_trace.n.store(0, std::memory_order_relaxed);
    }
  }

  // report the slots whose value moved and stayed changed (candidate coordinates).
  if (g_obj_snap_valid.load(std::memory_order_relaxed)) {
    const uint32_t objp = g_obj_last_ptr.load(std::memory_order_relaxed);
    if (objp) {
      char buf[900];
      size_t off = 0;
      uint32_t changed = 0;
      for (uint32_t i = 0; i < 128; ++i) {
        const uint32_t now = REX_LOAD_U32(objp + i * 4u);
        const uint32_t old = g_obj_snap[i];
        if (now != old) {
          ++changed;
          if (off < sizeof(buf) - 40) {
            float fn = 0.0f, fo = 0.0f;
            std::memcpy(&fn, &now, 4);
            std::memcpy(&fo, &old, 4);
            off += static_cast<size_t>(std::snprintf(
                buf + off, sizeof(buf) - off, "+%u:%08X>(%g|%d) ", i * 4, now, fn,
                static_cast<int32_t>(now)));
          }
        }
      }
      REXLOG_INFO("PROBE OBJDIFF obj={:08X} changed={}/128 slots[{}]", objp, changed, buf);
    }
    g_obj_snap_valid.store(0, std::memory_order_relaxed);
  }

  // T7_SPEED_29: per-frame displacement rate (world-units/second) over the wall window.
  {
    const uint32_t frames = g_move.frames.exchange(0, std::memory_order_relaxed);
    const uint64_t sum_bits = g_move.dist_bits_sum.exchange(0, std::memory_order_relaxed);
    const uint32_t moved = g_move.moved_frames.exchange(0, std::memory_order_relaxed);
    const uint32_t maxb = g_move.max_step_bits.exchange(0, std::memory_order_relaxed);
    const uint64_t prev_host = g_move.window_host_start.exchange(now, std::memory_order_relaxed);
    if (frames > 0 && prev_host != 0 && now > prev_host) {
      double dist = 0.0;
      std::memcpy(&dist, &sum_bits, 8);
      float maxstep = 0.0f;
      std::memcpy(&maxstep, &maxb, 4);
      const double host_freq_d = static_cast<double>(freq);
      const double sec = static_cast<double>(now - prev_host) / host_freq_d;
      const uint32_t objp = g_obj_last_ptr.load(std::memory_order_relaxed);
      REXLOG_INFO(
          "PROBE MOVERATE obj={:08X} window {:.3f}s frames={} moving={} units_per_sec "
          "{:.3f} units_per_frame {:.5f} max_step {:.3f}",
          objp, sec, frames, moved, sec > 0.0 ? dist / sec : 0.0,
          static_cast<double>(frames) > 0.0 ? dist / static_cast<double>(frames) : 0.0, maxstep);
    }
  }

  // T7_SPEED_22: guest-clock jump detector across main-loop iterations.
  const uint32_t ctN = g_ct_samples.exchange(0, std::memory_order_relaxed);
  if (ctN > 0) {
    const uint32_t below = g_ct_below.exchange(0, std::memory_order_relaxed);
    const uint32_t near_ = g_ct_near.exchange(0, std::memory_order_relaxed);
    const uint32_t high = g_ct_high.exchange(0, std::memory_order_relaxed);
    const uint32_t jump = g_ct_jump.exchange(0, std::memory_order_relaxed);
    const uint32_t hostBelow = g_ct_host_below.exchange(0, std::memory_order_relaxed);
    REXLOG_INFO(
        "PROBE GUESTCLK n={} expected_ticks~{:.0f} below={} near={} high={} jump={} "
        "host_early={} max_delta={}",
        ctN, 50000000.0 / (paceU * inv > 1.0 ? paceU * inv : 60.0), below, near_, high, jump,
        hostBelow, g_ct_maxdelta.exchange(0, std::memory_order_relaxed));
  }
  // T7_SPEED_27: world-struct latch detector. Report offsets that were STABLE in the
  // previous window but CHANGED since (a latch/step), which is what a persistent
  // "accelerated mode" flag looks like. Also count total churn.
  {
    const uint32_t w = g_sched_world.load(std::memory_order_relaxed);
    if (w && g_world_snap_valid.load(std::memory_order_relaxed) >= 2) {
      char latch_buf[900];
      size_t latch_off = 0;
      uint32_t latched = 0;
      uint32_t churn = 0;
      for (uint32_t i = 0; i < kWorldScanWords; ++i) {
        const uint32_t v = REX_LOAD_U32(w + i * 4u);
        const uint32_t p1 = g_world_snap[i];
        const uint32_t p2 = g_world_snap2[i];
        if (v != p1) {
          ++churn;
          if (p1 == p2) {  // was stable, just changed -> latch
            ++latched;
            if (latch_off < sizeof(latch_buf) - 48) {
              float f = 0.0f;
              std::memcpy(&f, &v, 4);
              latch_off += static_cast<size_t>(std::snprintf(
                  latch_buf + latch_off, sizeof(latch_buf) - latch_off,
                  "+0x%X:%08X>%08X(f=%g|i=%d) ", i * 4, p1, v, f, static_cast<int32_t>(v)));
            }
          }
        }
      }
      REXLOG_INFO("PROBE WORLDLATCH latched={} churn={}/{} [{}]", latched, churn,
                  kWorldScanWords, latch_buf);
    }
  }

#ifdef REX_DIAG_PROFILE
  {
    const rex::diag::DiagWindowSnapshot diag = rex::diag::ExchangeWindowAndReset();
    const uint64_t scenedisp_us = g_scenedisp_us_sum.exchange(0, std::memory_order_relaxed);
    const uint64_t sceneupd_us = g_sceneupd_us_sum.exchange(0, std::memory_order_relaxed);
    const uint64_t objupd_us = g_objupd_us_sum.exchange(0, std::memory_order_relaxed);
    const uint32_t pace_calls_pace = g_guest_821c62c8_calls.exchange(0, std::memory_order_relaxed);
    const uint64_t pace_us_pace = g_guest_821c62c8_us_sum.exchange(0, std::memory_order_relaxed);
    const uint32_t poll_calls = g_guest_821c4870_calls.exchange(0, std::memory_order_relaxed);
    const uint64_t poll_us = g_guest_821c4870_us_sum.exchange(0, std::memory_order_relaxed);
    const double inv = sec > 0.0 ? 1.0 / sec : 0.0;
    const double guest_present_hz =
        diag.present_frames > 0 ? diag.present_frames * inv : 0.0;
    const double draws_per_present =
        diag.present_frames ? static_cast<double>(diag.draws) / diag.present_frames : 0.0;
    REXLOG_INFO(
        "PROBE DIAG_VK present_hz={:.1f} draws/s={:.1f} draws_per_present_avg={:.1f} "
        "last_present_draws={} max_present_draws={} idx_draws/s={:.1f} vertices/s={:.0f} "
        "indices/s={:.0f} pipeline_binds/s={:.1f} renderpass/s={:.1f} rendering/s={:.1f} "
        "barriers/s={:.1f} desc_binds/s={:.1f} record_us_avg={:.0f} submit_us_avg={:.0f} "
        "pso_create/s={:.1f} pso_create_us_avg={:.0f} gpu_exec_us_avg={:.0f} gpu_exec_samples={}",
        guest_present_hz, diag.draws * inv, draws_per_present, diag.draws_last_present,
        diag.draws_max_per_present, diag.draws_indexed * inv,
        static_cast<double>(diag.vertices) * inv, static_cast<double>(diag.indices) * inv,
        diag.pipeline_binds * inv, diag.render_pass_begins * inv, diag.rendering_begins * inv,
        diag.barriers * inv, diag.descriptor_binds * inv,
        diag.present_frames ? static_cast<double>(diag.vk_record_us) / diag.present_frames : 0.0,
        diag.present_frames ? static_cast<double>(diag.vk_submit_us) / diag.present_frames : 0.0,
        diag.pso_creates * inv,
        diag.pso_creates ? static_cast<double>(diag.pso_create_us_sum) / diag.pso_creates : 0.0,
        diag.vk_gpu_exec_samples
            ? static_cast<double>(diag.vk_gpu_exec_us) / diag.vk_gpu_exec_samples
            : 0.0,
        diag.vk_gpu_exec_samples);
    const double issue_draw_per_frame =
        diag.present_frames ? static_cast<double>(diag.vk_issue_draw_us) / diag.present_frames : 0.0;
    const double desc_upd_per_frame = diag.present_frames
                                          ? static_cast<double>(diag.vk_descriptor_update_us) /
                                                diag.present_frames
                                          : 0.0;
    const double exec_pipe =
        diag.vk_exec_pipeline_cmds
            ? static_cast<double>(diag.vk_exec_pipeline_us) / diag.vk_exec_pipeline_cmds
            : 0.0;
    const double exec_desc =
        diag.vk_exec_descriptor_cmds
            ? static_cast<double>(diag.vk_exec_descriptor_us) / diag.vk_exec_descriptor_cmds
            : 0.0;
    REXLOG_INFO(
        "PROBE DIAG_VK_BREAKDOWN issue_draw_us_pf={:.0f} desc_update_us_pf={:.0f} "
        "record_us_pf={:.0f} exec_pipeline_us/call={:.1f} exec_descriptor_us/call={:.1f} "
        "exec_draw_us/call={:.1f} exec_barrier_us/call={:.1f} "
        "bind_skip_pipe={} bind_skip_desc={} tex_desc_reuse_hit={} tex_desc_reuse_miss={}",
        issue_draw_per_frame, desc_upd_per_frame,
        diag.present_frames ? static_cast<double>(diag.vk_record_us) / diag.present_frames : 0.0,
        exec_pipe, exec_desc,
        diag.vk_exec_draw_cmds
            ? static_cast<double>(diag.vk_exec_draw_us) / diag.vk_exec_draw_cmds
            : 0.0,
        diag.vk_exec_barrier_cmds
            ? static_cast<double>(diag.vk_exec_barrier_us) / diag.vk_exec_barrier_cmds
            : 0.0,
        diag.vk_bind_pipeline_skipped, diag.vk_bind_descriptor_skipped,
        diag.vk_texture_descriptor_reuse_hits, diag.vk_texture_descriptor_reuse_misses);
    const auto idraw_pf = [&](uint32_t phase) {
      return diag.present_frames
                 ? static_cast<double>(diag.issue_draw_phase_us[phase]) / diag.present_frames
                 : 0.0;
    };
    REXLOG_INFO(
        "PROBE ISSUE_DRAW_BREAKDOWN "
        "issue_draw_us_pf={:.0f} shader_prep_us_pf={:.0f} primitive_us_pf={:.0f} "
        "shader_sampler_us_pf={:.0f} textures_us_pf={:.0f} render_target_us_pf={:.0f} "
        "configure_pipeline_us_pf={:.0f} pipeline_bind_us_pf={:.0f} dynamic_state_us_pf={:.0f} "
        "system_constants_us_pf={:.0f} bindings_us_pf={:.0f} vertex_fetch_us_pf={:.0f} "
        "memexport_sync_us_pf={:.0f} barriers_rp_us_pf={:.0f} deferred_draw_us_pf={:.0f} "
        "memexport_post_us_pf={:.0f} pipe_desc_cache_hit={} pipe_desc_cache_miss={}",
        issue_draw_per_frame, idraw_pf(rex::diag::kIssueDrawShaderPrep),
        idraw_pf(rex::diag::kIssueDrawPrimitive), idraw_pf(rex::diag::kIssueDrawShaderSampler),
        idraw_pf(rex::diag::kIssueDrawRequestTextures), idraw_pf(rex::diag::kIssueDrawRenderTarget),
        idraw_pf(rex::diag::kIssueDrawConfigurePipeline), idraw_pf(rex::diag::kIssueDrawPipelineBind),
        idraw_pf(rex::diag::kIssueDrawDynamicState), idraw_pf(rex::diag::kIssueDrawSystemConstants),
        idraw_pf(rex::diag::kIssueDrawUpdateBindings), idraw_pf(rex::diag::kIssueDrawVertexFetch),
        idraw_pf(rex::diag::kIssueDrawMemexportSync), idraw_pf(rex::diag::kIssueDrawBarriersRenderPass),
        idraw_pf(rex::diag::kIssueDrawDeferredDraw), idraw_pf(rex::diag::kIssueDrawMemexportPost),
        diag.vk_pipeline_description_cache_hits, diag.vk_pipeline_description_cache_misses);
    REXLOG_INFO(
        "PROBE DIAG_AUDIO sdl_cb/s={:.1f} underrun/s={:.1f} cb_us_avg={:.0f} queue_depth_max={} "
        "worker_disp/s={:.1f} worker_disp_us_avg={:.0f} wait_ok/s={:.1f} wait_to/s={:.1f}",
        diag.sdl_callbacks * inv, diag.sdl_underruns * inv,
        diag.sdl_callbacks ? static_cast<double>(diag.sdl_callback_us) / diag.sdl_callbacks : 0.0,
        diag.sdl_queue_depth_max, diag.audio_worker_dispatches * inv,
        diag.audio_worker_dispatches
            ? static_cast<double>(diag.audio_worker_dispatch_us) / diag.audio_worker_dispatches
            : 0.0,
        diag.audio_wait_success * inv, diag.audio_wait_timeout * inv);
    REXLOG_INFO(
        "PROBE DIAG_GUEST scenedisp_us_avg={:.1f} sceneupd_us_avg={:.1f} objupd_us_avg={:.1f} "
        "821C62C8/s={:.1f} 821C62C8_us_avg={:.1f} 821C4870/s={:.1f} 821C4870_us_avg={:.1f}",
        dispU ? static_cast<double>(scenedisp_us) / dispU : 0.0,
        sceneU ? static_cast<double>(sceneupd_us) / sceneU : 0.0,
        objU ? static_cast<double>(objupd_us) / objU : 0.0, pace_calls_pace * inv,
        pace_calls_pace ? static_cast<double>(pace_us_pace) / pace_calls_pace : 0.0,
        poll_calls * inv, poll_calls ? static_cast<double>(poll_us) / poll_calls : 0.0);
  }
#endif

  g_window_start.store(now, std::memory_order_relaxed);

  // T7_SPEED_27: rotate the world snapshots for the next window.
  {
    const uint32_t w = g_sched_world.load(std::memory_order_relaxed);
    if (w) {
      const uint32_t valid = g_world_snap_valid.load(std::memory_order_relaxed);
      g_world_snap2 = g_world_snap;
      for (uint32_t i = 0; i < kWorldScanWords; ++i) {
        g_world_snap[i] = REX_LOAD_U32(w + i * 4u);
      }
      g_world_snap_valid.store(valid < 2 ? valid + 1 : 2, std::memory_order_relaxed);
    }
  }

  {
    const uint32_t objp = g_obj_last_ptr.load(std::memory_order_relaxed);
    if (objp) {
      for (uint32_t i = 0; i < 128; ++i) {
        g_obj_snap[i] = REX_LOAD_U32(objp + i * 4u);
      }
      g_obj_snap_valid.store(1, std::memory_order_relaxed);
    }
  }
}

// T7_SPEED_29: per-frame displacement meter. Track the first object seen via the per-object
// update hook whose position fields look like a finite world position, then measure
// |delta(position)| once per main-loop frame. The object is re-baselined (not re-latched) on
// teleports so a single relocation cannot inflate the measured speed. Sampling is driven from
// the master loop (once per frame) so per-frame deltas are not double-counted.
void MeterTrackedObject(uint8_t* base) {
  const uint32_t obj = g_obj_last_ptr.load(std::memory_order_relaxed);
  if (!obj) {
    return;
  }
  const uint32_t bx = REX_LOAD_U32(obj + 16u);
  const uint32_t bz = REX_LOAD_U32(obj + 24u);
  float xf = 0.0f, zf = 0.0f;
  std::memcpy(&xf, &bx, 4);
  std::memcpy(&zf, &bz, 4);
  const bool plausible = std::isfinite(xf) && std::isfinite(zf) && std::fabs(xf) < 1.0e7f &&
                         std::fabs(zf) < 1.0e7f;
  if (!plausible) {
    g_move.last_valid.store(0, std::memory_order_relaxed);
    return;
  }
  if (g_move.last_valid.load(std::memory_order_relaxed)) {
    float x0 = 0, z0 = 0;
    const uint32_t px = g_move.last_x.load(std::memory_order_relaxed);
    const uint32_t pz = g_move.last_z.load(std::memory_order_relaxed);
    std::memcpy(&x0, &px, 4);
    std::memcpy(&z0, &pz, 4);
    const double dx = static_cast<double>(xf) - x0;
    const double dz = static_cast<double>(zf) - z0;
    const double step = std::sqrt(dx * dx + dz * dz);
    if (std::isfinite(step) && step < 5000.0) {
      g_move.frames.fetch_add(1, std::memory_order_relaxed);
      if (step > 0.001) {
        g_move.moved_frames.fetch_add(1, std::memory_order_relaxed);
      }
      const uint64_t sum_bits = g_move.dist_bits_sum.load(std::memory_order_relaxed);
      double acc = 0.0;
      std::memcpy(&acc, &sum_bits, 8);
      acc += step;
      uint64_t out_bits = 0;
      std::memcpy(&out_bits, &acc, 8);
      g_move.dist_bits_sum.store(out_bits, std::memory_order_relaxed);
      uint32_t mb = g_move.max_step_bits.load(std::memory_order_relaxed);
      float mbf = 0.0f;
      std::memcpy(&mbf, &mb, 4);
      if (step > static_cast<double>(mbf)) {
        float sf = static_cast<float>(step);
        uint32_t sb32 = 0;
        std::memcpy(&sb32, &sf, 4);
        g_move.max_step_bits.store(sb32, std::memory_order_relaxed);
      }
    } else {
      g_move.last_valid.store(0, std::memory_order_relaxed);
    }
  }
  g_move.last_x.store(bx, std::memory_order_relaxed);
  g_move.last_z.store(bz, std::memory_order_relaxed);
  g_move.last_valid.store(1, std::memory_order_relaxed);
}

}  // namespace

extern "C" void Sonic4NotePaceExit(int kind, uint64_t now, uint64_t deadline) {
  if (kind == 1) {
    g_pace_exit_signal.fetch_add(1, std::memory_order_relaxed);
  } else {
    g_pace_exit_deadline.fetch_add(1, std::memory_order_relaxed);
    g_pace_deadline_cmp.fetch_add(static_cast<int64_t>(deadline - now), std::memory_order_relaxed);
  }
}

namespace {
thread_local int g_pace_rearms = 0;
}

extern "C" int Sonic4PaceRearm() {
  // One re-arm per return keeps a just-missed deadline from dispatching a
  // second time. A second miss in the same call falls through so a lost
  // signal cannot stall the task thread.
  if (g_pace_rearms >= 1) {
    g_pace_rearms = 0;
    return 0;
  }
  ++g_pace_rearms;
  return 1;
}

extern "C" void Sonic4PaceRearmReset() { g_pace_rearms = 0; }

extern "C" REX_FUNC(sub_8249E2F8) {
  const uint32_t seen = g_viewport_this_frame.exchange(0, std::memory_order_relaxed);
  uint32_t prev = g_viewport.per_frame_max.load(std::memory_order_relaxed);
  while (seen > prev &&
         !g_viewport.per_frame_max.compare_exchange_weak(prev, seen, std::memory_order_relaxed)) {
  }
  Observe(g_main, ctx);
  MaybeLog(base);
  ProbeClockRate();
  // T7_SPEED_29: sample the tracked object's position once per main-loop frame so the
  // displacement meter measures per-frame motion, not per-update-call motion.
  MeterTrackedObject(base);
  // T7_SPEED_22: sample guest tick delta and host delta for this main-loop iteration
  // BEFORE pacing/baseline reset, so we see the clock the game actually observes.
  {
    const uint64_t guest_now = rex::chrono::Clock::QueryGuestTickCount();
    const uint64_t host_now = rex::chrono::Clock::QueryHostTickCount();
    const uint64_t host_freq = rex::chrono::Clock::QueryHostTickFrequency();
    const uint64_t gprev = g_ct_prev.exchange(guest_now, std::memory_order_relaxed);
    if (gprev != 0 && guest_now >= gprev) {
      const uint64_t d = guest_now - gprev;
      g_ct_samples.fetch_add(1, std::memory_order_relaxed);
      // expected ~ one refresh interval at 50 MHz; refresh ~60 Hz, but pacing may target
      // other rates, so use a fixed 60 Hz expectation for the bucket boundaries.
      const uint64_t expected = 50000000ull / 60ull;  // ~833333
      if (d * 2ull < expected) {
        g_ct_below.fetch_add(1, std::memory_order_relaxed);
      } else if (d * 2ull < expected * 3ull) {
        g_ct_near.fetch_add(1, std::memory_order_relaxed);
      } else if (d < expected * 3ull) {
        g_ct_high.fetch_add(1, std::memory_order_relaxed);
      } else {
        g_ct_jump.fetch_add(1, std::memory_order_relaxed);
      }
      uint64_t m = g_ct_maxdelta.load(std::memory_order_relaxed);
      while (d > m && !g_ct_maxdelta.compare_exchange_weak(m, d, std::memory_order_relaxed)) {
      }
      (void)host_now;
      (void)host_freq;
    }
  }
  rex::kernel::xboxkrnl::PaceGuestMainLoopFrame();
  __imp__sub_8249E2F8(ctx, base);
}

extern "C" REX_FUNC(sub_821B35C8) {
  g_viewport_this_frame.fetch_add(1, std::memory_order_relaxed);
  Observe(g_viewport, ctx);
  __imp__sub_821B35C8(ctx, base);
}

extern "C" REX_FUNC(sub_821C1C80) {
  Observe(g_scene, ctx);
  __imp__sub_821C1C80(ctx, base);
}

extern "C" REX_FUNC(sub_8249CC00) {
  Observe(g_dispatch_cc, ctx);
  __imp__sub_8249CC00(ctx, base);
}

extern "C" REX_FUNC(sub_8249D308) {
  Observe(g_dispatch_d3, ctx);
  __imp__sub_8249D308(ctx, base);
}

extern "C" REX_FUNC(sub_821D30E0) {
  g_scene_timer_calls.fetch_add(1, std::memory_order_relaxed);
  const uint32_t world = ctx.r3.u32;
  __imp__sub_821D30E0(ctx, base);
  if (world != 0) {
    const uint32_t d = REX_LOAD_U32(world + 21792u);
    g_dt21792_n.fetch_add(1, std::memory_order_relaxed);
    g_dt21792_sum.fetch_add(d, std::memory_order_relaxed);
    g_dt21792_last.store(d, std::memory_order_relaxed);
    uint32_t prev_max = g_dt21792_max.load(std::memory_order_relaxed);
    while (d > prev_max &&
           !g_dt21792_max.compare_exchange_weak(prev_max, d, std::memory_order_relaxed)) {
    }
    uint32_t prev_min = g_dt21792_min.load(std::memory_order_relaxed);
    while (d < prev_min &&
           !g_dt21792_min.compare_exchange_weak(prev_min, d, std::memory_order_relaxed)) {
    }
  }
}

extern "C" void Sonic4NoteTaskCallback(uint32_t guest_ctr) {
  // Bit 0 set means "skip" in the caller; those never reach here.
  if ((guest_ctr & 1u) != 0 || guest_ctr < 0x82000000u) {
    return;
  }
  g_task_calls.fetch_add(1, std::memory_order_relaxed);
  for (uint32_t i = 0; i < kTaskSlots; ++i) {
    const uint32_t cur = g_task_addr[i].load(std::memory_order_relaxed);
    if (cur == guest_ctr) {
      g_task_hits[i].fetch_add(1, std::memory_order_relaxed);
      return;
    }
    if (cur == 0) {
      uint32_t expected = 0;
      if (g_task_addr[i].compare_exchange_strong(expected, guest_ctr, std::memory_order_relaxed)) {
        g_task_hits[i].fetch_add(1, std::memory_order_relaxed);
        return;
      }
      if (expected == guest_ctr) {
        g_task_hits[i].fetch_add(1, std::memory_order_relaxed);
        return;
      }
    }
  }
  g_task_dropped.fetch_add(1, std::memory_order_relaxed);
}

extern "C" REX_FUNC(sub_82163E00) {
  const uint32_t obj = ctx.r3.u32;
  __imp__sub_82163E00(ctx, base);
  if (obj != 0) {
    const uint32_t trial = REX_LOAD_U32(obj + 160u);
    REXLOG_INFO("LICENSE flag160={} (0 = full, 1 = demo)", trial);
  }
}

extern "C" REX_FUNC(sub_8249ECA8) {
  g_task_dispatches.fetch_add(1, std::memory_order_relaxed);
  NoteCaller(g_eca8_hist, static_cast<uint32_t>(ctx.lr));
  __imp__sub_8249ECA8(ctx, base);
}

extern "C" REX_FUNC(sub_82488738) {
  const uint32_t obj = ctx.r3.u32;
  const uint32_t arg = ctx.r4.u32;
  g_iv_calls.fetch_add(1, std::memory_order_relaxed);
  g_iv_r4_last.store(arg, std::memory_order_relaxed);
  g_iv_lr.store(static_cast<uint32_t>(ctx.lr), std::memory_order_relaxed);
  uint32_t prev = g_iv_r4_max.load(std::memory_order_relaxed);
  while (arg > prev && !g_iv_r4_max.compare_exchange_weak(prev, arg, std::memory_order_relaxed)) {
  }
  prev = g_iv_r4_min.load(std::memory_order_relaxed);
  while (arg < prev && !g_iv_r4_min.compare_exchange_weak(prev, arg, std::memory_order_relaxed)) {
  }
  if (obj != 0) {
    g_iv_scale_bits.store(REX_LOAD_U32(obj + 12u), std::memory_order_relaxed);
  }
  __imp__sub_82488738(ctx, base);
  if (obj != 0) {
    g_iv_interval.store(REX_LOAD_U64(obj + 32u), std::memory_order_relaxed);
  }
}

extern "C" REX_FUNC(sub_82223E98) {
  const uint32_t timer_handle = REX_LOAD_U32(0x824F0FB0u);
  if (ctx.r3.u32 == timer_handle && timer_handle != 0) {
    const uint64_t now = REX_QUERY_TIMEBASE();
    const uint64_t deadline = REX_LOAD_U64(0x824F0FC0u);
    g_sig_n.fetch_add(1, std::memory_order_relaxed);
    g_sig_remain_sum.fetch_add(static_cast<int64_t>(deadline - now), std::memory_order_relaxed);
  }
  __imp__sub_82223E98(ctx, base);
}

extern "C" REX_FUNC(sub_82488958) {
  constexpr uint32_t kTimer = 0x824F0FA8u;
  const uint64_t now0 = REX_QUERY_TIMEBASE();
  const uint64_t deadline0 = REX_LOAD_U64(kTimer + 24u);
  const int64_t entry = static_cast<int64_t>(deadline0 - now0);
  const uint64_t t0 = rex::chrono::Clock::QueryHostTickCount();
  __imp__sub_82488958(ctx, base);
  const uint64_t freq = rex::chrono::Clock::QueryHostTickFrequency();
  const uint64_t dt = rex::chrono::Clock::QueryHostTickCount() - t0;
  const uint64_t us = freq ? (dt * 1000000ull) / freq : 0;
  const uint64_t now1 = REX_QUERY_TIMEBASE();
  const uint64_t deadline1 = REX_LOAD_U64(kTimer + 24u);
  const int64_t exit_delta = static_cast<int64_t>(deadline1 - now1);
  g_pace_frames.fetch_add(1, std::memory_order_relaxed);
  g_pace_frame_us.fetch_add(us, std::memory_order_relaxed);
  g_pace_entry_sum.fetch_add(entry, std::memory_order_relaxed);
  g_pace_exit_sum.fetch_add(exit_delta, std::memory_order_relaxed);
  if (exit_delta <= 0) {
    g_pace_exit_past.fetch_add(1, std::memory_order_relaxed);
  }
  if (us < 3000) {
    g_pace_us_short.fetch_add(1, std::memory_order_relaxed);
    g_pace_short_entry.fetch_add(entry, std::memory_order_relaxed);
    g_pace_short_exit.fetch_add(exit_delta, std::memory_order_relaxed);
  } else if (us > 12000) {
    g_pace_us_long.fetch_add(1, std::memory_order_relaxed);
    g_pace_long_entry.fetch_add(entry, std::memory_order_relaxed);
    g_pace_long_exit.fetch_add(exit_delta, std::memory_order_relaxed);
  }
  uint32_t uprev = g_pace_us_max.load(std::memory_order_relaxed);
  const uint32_t us32 = static_cast<uint32_t>(us);
  while (us32 > uprev &&
         !g_pace_us_max.compare_exchange_weak(uprev, us32, std::memory_order_relaxed)) {
  }
  uprev = g_pace_us_min.load(std::memory_order_relaxed);
  while (us32 < uprev &&
         !g_pace_us_min.compare_exchange_weak(uprev, us32, std::memory_order_relaxed)) {
  }
  int64_t prev = g_pace_entry_max.load(std::memory_order_relaxed);
  while (entry > prev &&
         !g_pace_entry_max.compare_exchange_weak(prev, entry, std::memory_order_relaxed)) {
  }
  prev = g_pace_entry_min.load(std::memory_order_relaxed);
  while (entry < prev &&
         !g_pace_entry_min.compare_exchange_weak(prev, entry, std::memory_order_relaxed)) {
  }
}

extern "C" REX_FUNC(sub_82488698) {
  const uint64_t t0 = rex::chrono::Clock::QueryHostTickCount();
  __imp__sub_82488698(ctx, base);
  const uint64_t freq = rex::chrono::Clock::QueryHostTickFrequency();
  const uint64_t dt = rex::chrono::Clock::QueryHostTickCount() - t0;
  const uint64_t us = freq ? (dt * 1000000ull) / freq : 0;
  g_pace_sync.fetch_add(1, std::memory_order_relaxed);
  g_pace_sync_us.fetch_add(us, std::memory_order_relaxed);
}

extern "C" REX_FUNC(sub_82223A00) {
  const uint32_t ret = static_cast<uint32_t>(ctx.lr);
  __imp__sub_82223A00(ctx, base);
  // Return sites inside sub_82488958. Status 0 is WAIT_OBJECT_0.
  if (ret == 0x82488A14u || ret == 0x82488A70u || ret == 0x82488A98u || ret == 0x82488AB0u) {
    if (ctx.r3.u32 == 0) {
      g_pace_evt_ok.fetch_add(1, std::memory_order_relaxed);
    } else {
      g_pace_evt_to.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

extern "C" REX_FUNC(sub_82446628) {
  g_module_timer_calls.fetch_add(1, std::memory_order_relaxed);
  __imp__sub_82446628(ctx, base);
}

// T7_SPEED_21 Â§4: frame update. Classify caller by return address, read the worker-gate
// byte, time the call, then forward untouched.
extern "C" REX_FUNC(sub_821C0E58) {
  NoteCaller(g_fu_hist, ctx.lr);
  switch (static_cast<uint32_t>(ctx.lr)) {
    case 0x8249E418:  // main-loop path A (queue r5 = world+13852)
      g_fu_mainA.fetch_add(1, std::memory_order_relaxed);
      break;
    case 0x8249E44C:  // main-loop path B (r5 = 0)
      g_fu_mainB.fetch_add(1, std::memory_order_relaxed);
      break;
    case 0x821D44EC:  // worker thread sub_821D43E0
      g_fu_thread.fetch_add(1, std::memory_order_relaxed);
      break;
    default:
      g_fu_other.fetch_add(1, std::memory_order_relaxed);
      break;
  }
  if (REX_LOAD_U8(ctx.r3.u32 + 10942u) & 0x2u) {
    g_fu_gate2.fetch_add(1, std::memory_order_relaxed);
  }
  const uint64_t t0 = rex::chrono::Clock::QueryHostTickCount();
  __imp__sub_821C0E58(ctx, base);
  const uint64_t t1 = rex::chrono::Clock::QueryHostTickCount();
  const uint64_t freq = rex::chrono::Clock::QueryHostTickFrequency();
  const uint64_t us = freq ? (t1 - t0) * 1000000ull / freq : 0;
  g_fu_us_sum.fetch_add(us, std::memory_order_relaxed);
  uint32_t prev = g_fu_us_max.load(std::memory_order_relaxed);
  while (us > prev &&
         !g_fu_us_max.compare_exchange_weak(prev, static_cast<uint32_t>(us),
                                            std::memory_order_relaxed)) {
  }
}

// T7_SPEED_21 Â§4: poll-delta consumer. Descriptor: +0 world, +4 mode, +20 stamp.
// world+13644 = subscriber callback (never written in generated code -> value comes
// from the XEX image; read it here). Forward untouched.
extern "C" REX_FUNC(sub_821C47A8) {
  NoteCaller(g_poll_hist, ctx.lr);
  const uint32_t world = REX_LOAD_U32(ctx.r3.u32 + 0u);
  const uint32_t mode = REX_LOAD_U32(ctx.r3.u32 + 4u);
  if (mode == 0) {
    g_poll_mode0.fetch_add(1, std::memory_order_relaxed);
  } else {
    const uint32_t stamp = REX_LOAD_U32(ctx.r3.u32 + 20u);
    const uint32_t now32 = static_cast<uint32_t>(rex::chrono::Clock::QueryGuestTickCount());
    const uint32_t delta = now32 - stamp;  // wrapping u32, same as guest subf
    g_poll_calls.fetch_add(1, std::memory_order_relaxed);
    g_poll_sum.fetch_add(delta, std::memory_order_relaxed);
    uint32_t prev = g_poll_max.load(std::memory_order_relaxed);
    while (delta > prev &&
           !g_poll_max.compare_exchange_weak(prev, delta, std::memory_order_relaxed)) {
    }
    if (mode == 3) {
      g_poll_mode3.fetch_add(1, std::memory_order_relaxed);
    } else {
      g_poll_mode_other.fetch_add(1, std::memory_order_relaxed);
    }
    if (world) {
      const uint32_t cb = REX_LOAD_U32(world + 13644u);
      if (cb) {
        g_poll_cb_nonzero.fetch_add(1, std::memory_order_relaxed);
        uint64_t expected = 0;
        g_poll_cb_ptr.compare_exchange_strong(expected, cb, std::memory_order_relaxed);
      }
    }
  }
  if (!g_poll_reported.exchange(1, std::memory_order_relaxed) && world) {
    uint32_t rate_bits = REX_LOAD_U32(world + 21804u);
    uint32_t scale_bits = REX_LOAD_U32(world + 21808u);
    uint32_t const_bits = REX_LOAD_U32(0x820940E0u);
    float rate = 0.0f, scale = 0.0f, ticks_to_s = 0.0f;
    std::memcpy(&rate, &rate_bits, sizeof(rate));
    std::memcpy(&scale, &scale_bits, sizeof(scale));
    std::memcpy(&ticks_to_s, &const_bits, sizeof(ticks_to_s));
    REXLOG_INFO("POLLINIT world={:08X} cb={:08X} rate21804={:g} scale21808={:g} "
                "ticks2s_820940E0={:g}",
                world, REX_LOAD_U32(world + 13644u), rate, scale, ticks_to_s);
  }
  __imp__sub_821C47A8(ctx, base);
}

// T7_SPEED_21 Â§4: scene path reached from master loop only when state[0x82585E40]==3
// (2x per main-loop iteration). Count and record callers.
extern "C" REX_FUNC(sub_82197F28) {
  g_sceneupd_calls.fetch_add(1, std::memory_order_relaxed);
  NoteCaller(g_sceneupd_hist, ctx.lr);
#ifdef REX_DIAG_PROFILE
  const uint64_t t0 = HostUsNow();
#endif
  __imp__sub_82197F28(ctx, base);
#ifdef REX_DIAG_PROFILE
  g_sceneupd_us_sum.fetch_add(HostUsNow() - t0, std::memory_order_relaxed);
#endif
}

// T7_SPEED_21 Â§4: scene dispatcher sub_821A8488 (sets mode global, flags world obj,
// calls sub_82197F28). Count and record callers to identify the hot indirect path.
extern "C" REX_FUNC(sub_821A8488) {
  g_scenedisp_calls.fetch_add(1, std::memory_order_relaxed);
  NoteCaller(g_scenedisp_hist, ctx.lr);
#ifdef REX_DIAG_PROFILE
  const uint64_t t0 = HostUsNow();
#endif
  __imp__sub_821A8488(ctx, base);
#ifdef REX_DIAG_PROFILE
  g_scenedisp_us_sum.fetch_add(HostUsNow() - t0, std::memory_order_relaxed);
#endif
}

// T7_SPEED_21 Â§4: per-object update sub_8249CF80 (job-dispatched, no static callers).
// Record the indirect caller (ctx.lr) and sample the object's state byte/subcode.
extern "C" REX_FUNC(sub_8249CF80) {
  g_objupd_calls.fetch_add(1, std::memory_order_relaxed);
  NoteCaller(g_objupd_hist, ctx.lr);
#ifdef REX_DIAG_PROFILE
  const uint64_t t0_obj = HostUsNow();
#endif
  if (ctx.r3.u32) {
    const uint32_t obj = REX_LOAD_U32(ctx.r3.u32 + 8u);
    if (obj) {
      const uint32_t st = REX_LOAD_U32(obj + 8u);
      uint32_t expected = 0xFFFFFFFFu;
      g_objupd_state_seen.compare_exchange_strong(expected, st, std::memory_order_relaxed);
      // T7_SPEED_29: latch the first object this hook reports; it is metered once per frame.
      if (g_obj_last_ptr.load(std::memory_order_relaxed) == 0) {
        uint32_t exp0 = 0;
        g_obj_last_ptr.compare_exchange_strong(exp0, obj, std::memory_order_relaxed);
      }
    }
  }
  __imp__sub_8249CF80(ctx, base);
#ifdef REX_DIAG_PROFILE
  g_objupd_us_sum.fetch_add(HostUsNow() - t0_obj, std::memory_order_relaxed);
#endif
}

#ifdef REX_DIAG_PROFILE
extern "C" REX_FUNC(sub_821C62C8) {
  g_guest_821c62c8_calls.fetch_add(1, std::memory_order_relaxed);
  const uint64_t t0 = HostUsNow();
  __imp__sub_821C62C8(ctx, base);
  g_guest_821c62c8_us_sum.fetch_add(HostUsNow() - t0, std::memory_order_relaxed);
}

extern "C" REX_FUNC(sub_821C4870) {
  g_guest_821c4870_calls.fetch_add(1, std::memory_order_relaxed);
  const uint64_t t0 = HostUsNow();
  __imp__sub_821C4870(ctx, base);
  g_guest_821c4870_us_sum.fetch_add(HostUsNow() - t0, std::memory_order_relaxed);
}
#endif

// T7_SPEED_21 Â§4: guest-side frame pacing. Records call rate and the target fps field.
// T7_SPEED_23: also samples the frame-scheduler state and detects multi-frame advances.
extern "C" REX_FUNC(sub_821C0770) {
  g_pace_calls.fetch_add(1, std::memory_order_relaxed);
  // sub_821C0770 resolves its world via a double indirection:
  //   r11 = mem32[0x820004F8];  r31 = mem32[r11]
  const uint32_t slot = REX_LOAD_U32(0x820004F8u);
  const uint32_t world = slot ? REX_LOAD_U32(slot) : 0;
  if (world) {
    uint32_t expected = 0;
    g_pace_target.compare_exchange_strong(expected, REX_LOAD_U32(world + 21772u),
                                          std::memory_order_relaxed);
    // T7_SPEED_23: snapshot scheduler counters before the call.
    const uint32_t c16764 = REX_LOAD_U32(world + 16764u);
    const uint32_t c16760 = REX_LOAD_U32(world + 16760u);
    const uint32_t c16768 = REX_LOAD_U32(world + 16768u);
    const uint32_t c16776 = REX_LOAD_U32(world + 16776u);
    const uint32_t c16784 = REX_LOAD_U32(world + 16784u);
    g_sched_calls.fetch_add(1, std::memory_order_relaxed);
    g_sched_world.store(world, std::memory_order_relaxed);
    g_sched_last_16764.store(c16764, std::memory_order_relaxed);
    g_sched_16760.store(c16760, std::memory_order_relaxed);
    g_sched_16768.store(c16768, std::memory_order_relaxed);
    g_sched_16776.store(c16776, std::memory_order_relaxed);
    // guest tick delta since baseline
    const uint32_t now32 = static_cast<uint32_t>(rex::chrono::Clock::QueryGuestTickCount());
    const uint32_t delta_ticks = now32 - c16764;
    uint64_t m = g_sched_max_delta_ticks.load(std::memory_order_relaxed);
    while (delta_ticks > m &&
           !g_sched_max_delta_ticks.compare_exchange_weak(m, delta_ticks, std::memory_order_relaxed)) {
    }
    // Estimate frames-per-call the same way the guest does: (delta*100)/(freq/target).
    // freq guest = 50 MHz; target = c16760>0 ? c16760 : 60.
    const uint32_t target = c16760 > 0 ? c16760 : 60u;
    const uint32_t frames = static_cast<uint32_t>((static_cast<uint64_t>(delta_ticks) * 100ull) /
                                                  (50000000ull / target));
    if (frames > 1) {
      g_sched_frames_gt1.fetch_add(1, std::memory_order_relaxed);
    }
    // Detect the frame counter jumping by more than 1 between calls.
    const uint32_t prev84 = g_sched_prev_16784.exchange(c16784, std::memory_order_relaxed);
    if (prev84 != 0 && c16784 > prev84 + 1) {
      g_sched_queue_jumps.fetch_add(1, std::memory_order_relaxed);
    }
    g_sched_16784.store(c16784, std::memory_order_relaxed);
    // T7_SPEED_33: decompose the scheduler argument and track the queue producer index.
    const uint32_t arg = ctx.r3.u32;
    const uint32_t r29 = (arg >> 24) & 0xFu;
    const uint32_t r27 = arg & 0xFFu;
    const uint32_t qidx = REX_LOAD_U32(world + 16920u);
    g_sched_arg_last.store(arg, std::memory_order_relaxed);
    g_sched_r27_last.store(r27, std::memory_order_relaxed);
    if (r29 != 0) {
      g_sched_r29_nonzero.fetch_add(1, std::memory_order_relaxed);
    }
    if (r29 < 16) {
      g_sched_r29_hist[r29].fetch_add(1, std::memory_order_relaxed);
    }
    uint32_t qmax = g_sched_qidx_max.load(std::memory_order_relaxed);
    while (qidx > qmax && !g_sched_qidx_max.compare_exchange_weak(qmax, qidx, std::memory_order_relaxed)) {
    }
    g_sched_qidx_last.store(qidx, std::memory_order_relaxed);
  }
  __imp__sub_821C0770(ctx, base);
}

// T7_SPEED_24: the game's frame-delta query. Capture the returned ms value and callers.
extern "C" REX_FUNC(sub_821D0020) {
  const uint32_t slot = REX_LOAD_U32(0x820004F8u);
  const uint32_t world = slot ? REX_LOAD_U32(slot) : 0;
  const uint32_t world21892_before = world ? REX_LOAD_U32(world + 21892u) : 0;
  uint64_t stack_mftb = 0;
  __imp__sub_821D0020(ctx, base);
  NoteCaller(g_dt_hist, ctx.lr);
  const uint32_t ms = static_cast<uint32_t>(ctx.r3.u32);
  const uint32_t world21892_after = world ? REX_LOAD_U32(world + 21892u) : 0;
  g_dt_calls.fetch_add(1, std::memory_order_relaxed);
  g_dt_sum.fetch_add(ms, std::memory_order_relaxed);
  g_dt_last.store(ms, std::memory_order_relaxed);
  g_dt_21892.store(world21892_after, std::memory_order_relaxed);
  if (world21892_before != world21892_after) {
    g_dt_21892_changes.fetch_add(1, std::memory_order_relaxed);
  }
  if (ms == 0) {
    g_dt_zero.fetch_add(1, std::memory_order_relaxed);
  }
  if (ms >= 50) {
    g_dt_big.fetch_add(1, std::memory_order_relaxed);
  }
  uint32_t prev = g_dt_max.load(std::memory_order_relaxed);
  while (ms > prev && !g_dt_max.compare_exchange_weak(prev, ms, std::memory_order_relaxed)) {
  }
  static std::atomic<uint32_t> dt_log_count{0};
  if (dt_log_count.fetch_add(1, std::memory_order_relaxed) < 80) {
    REXLOG_INFO("DTRAW ms={} w21892_before={} w21892_after={} world={:08X} lr={:08X}", ms, world21892_before, world21892_after, world, static_cast<uint32_t>(ctx.lr));
  }
}

extern "C" REX_FUNC(sub_82223E78) {
  static std::atomic<uint32_t> log_count{0};
  if (log_count.fetch_add(1, std::memory_order_relaxed) < 200) {
    const uint64_t mftb = REX_QUERY_TIMEBASE();
    REXLOG_INFO("MFTB_GET lr={:08X} mftb={} (>>10)={} r3={:08X}", static_cast<uint32_t>(ctx.lr), mftb, mftb >> 10, static_cast<uint32_t>(ctx.r3.u32));
  }
  __imp__sub_82223E78(ctx, base);
  if (log_count.load(std::memory_order_relaxed) <= 200 && (log_count.load(std::memory_order_relaxed) % 5 == 0)) {
    // log after return to see what was stored
    const uint64_t stored = ctx.r3.u64; // r3 might be clobbered, but we can't easily read stack; just log call count
  }
}

extern "C" REX_FUNC(sub_821C0650) {
  static std::atomic<uint32_t> log_count{0};
  const uint32_t slot = REX_LOAD_U32(0x820004F8u);
  const uint32_t world = slot ? REX_LOAD_U32(slot) : 0;
  const uint32_t w21892_before = world ? REX_LOAD_U32(world + 21892u) : 0;
  const uint32_t w16764_before = world ? REX_LOAD_U32(world + 16764u) : 0;
  const uint32_t w16776_before = world ? REX_LOAD_U32(world + 16776u) : 0;
  const uint32_t w16784_before = world ? REX_LOAD_U32(world + 16784u) : 0;
  if (log_count.fetch_add(1, std::memory_order_relaxed) < 150) {
    REXLOG_INFO("TIMERWRITER_ENTER lr={:08X} world={:08X} w21892={} w16764={} w16776={} w16784={} mftb={}", static_cast<uint32_t>(ctx.lr), world, w21892_before, w16764_before, w16776_before, w16784_before, REX_QUERY_TIMEBASE());
  }
  __imp__sub_821C0650(ctx, base);
  const uint32_t w21892_after = world ? REX_LOAD_U32(world + 21892u) : 0;
  const uint32_t w16776_after = world ? REX_LOAD_U32(world + 16776u) : 0;
  const uint32_t w16784_after = world ? REX_LOAD_U32(world + 16784u) : 0;
  if (log_count.fetch_add(1, std::memory_order_relaxed) < 300 && (log_count.load(std::memory_order_relaxed) % 3 == 1)) {
    REXLOG_INFO("TIMERWRITER_EXIT world21892={} w16776={} w16784={} mftb={} backlog={}", w21892_after, w16776_after, w16784_after, REX_QUERY_TIMEBASE(), (world && w16776_after>=w16784_after) ? (w16776_after - w16784_after) : 0);
  }
}

// T7_SPEED_25: engine time service sub_821D2478. Records the requested case (r4),
// the raw tick delta world[21792] currently held, and the returned seconds (f1).
extern "C" REX_FUNC(sub_821D2478) {
  const uint32_t world = ctx.r3.u32;
  const uint32_t req_case = ctx.r4.u32;
  if (world) {
    const uint32_t d = REX_LOAD_U32(world + 21792u);
    g_tc_21792_last.store(d, std::memory_order_relaxed);
    g_tc_21792_sum.fetch_add(d, std::memory_order_relaxed);
    uint32_t prev = g_tc_21792_max.load(std::memory_order_relaxed);
    while (d > prev && !g_tc_21792_max.compare_exchange_weak(prev, d, std::memory_order_relaxed)) {
    }
    uint32_t expected = 0xFFFFFFFFu;
    g_tc_case_seen.compare_exchange_strong(expected, req_case, std::memory_order_relaxed);
    if (req_case < 24) {
      g_tc_case_hits[req_case].fetch_add(1, std::memory_order_relaxed);
    }
  }
  __imp__sub_821D2478(ctx, base);
  const float f1 = static_cast<float>(ctx.f1.f64);
  uint32_t bits = 0;
  std::memcpy(&bits, &f1, sizeof(bits));
  g_tc_ret_bits.store(bits, std::memory_order_relaxed);
  if (req_case < 24) {
    g_tc_case_retbits[req_case].store(bits, std::memory_order_relaxed);
  }
  g_tc_calls.fetch_add(1, std::memory_order_relaxed);
}

