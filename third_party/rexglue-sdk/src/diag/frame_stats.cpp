/**
 * Diagnostic counters (Android diagnostic builds only).
 */

#include <rex/diag/frame_stats.h>

#ifdef REX_FRAME_STATS

#include <algorithm>
#include <array>
#include <atomic>

namespace rex::diag {
namespace {

std::atomic<uint32_t> g_present_frames{0};
std::atomic<uint32_t> g_draws{0};
std::atomic<uint32_t> g_draws_indexed{0};
std::atomic<uint64_t> g_vertices{0};
std::atomic<uint64_t> g_indices{0};
std::atomic<uint32_t> g_pipeline_binds{0};
std::atomic<uint32_t> g_render_pass_begins{0};
std::atomic<uint32_t> g_rendering_begins{0};
std::atomic<uint32_t> g_barriers{0};
std::atomic<uint32_t> g_descriptor_binds{0};
std::atomic<uint32_t> g_draws_this_present{0};
std::atomic<uint32_t> g_draws_max_present{0};
std::atomic<uint32_t> g_draws_last_present{0};
std::atomic<uint64_t> g_vk_record_us{0};
std::atomic<uint64_t> g_vk_submit_us{0};
std::atomic<uint64_t> g_vk_gpu_exec_us{0};
std::atomic<uint32_t> g_vk_gpu_exec_samples{0};
std::atomic<uint32_t> g_pso_creates{0};
std::atomic<uint64_t> g_pso_create_us{0};

std::atomic<uint64_t> g_vk_issue_draw_us{0};
std::atomic<uint32_t> g_vk_issue_draw_calls{0};
std::atomic<uint64_t> g_vk_descriptor_update_us{0};
std::atomic<uint32_t> g_vk_descriptor_update_calls{0};
std::array<std::atomic<uint64_t>, kVkExecCategoryCount> g_vk_exec_category_us{};
std::array<std::atomic<uint32_t>, kVkExecCategoryCount> g_vk_exec_category_cmds{};
std::atomic<uint32_t> g_vk_bind_pipeline_skipped{0};
std::atomic<uint32_t> g_vk_bind_descriptor_skipped{0};
std::atomic<uint32_t> g_vk_texture_desc_reuse_hits{0};
std::atomic<uint32_t> g_vk_texture_desc_reuse_misses{0};
std::array<std::atomic<uint64_t>, DiagWindowSnapshot::kIssueDrawPhaseCount> g_issue_draw_phase_us{};
std::array<std::atomic<uint32_t>, DiagWindowSnapshot::kIssueDrawPhaseCount>
    g_issue_draw_phase_calls{};
std::array<std::atomic<uint64_t>, DiagWindowSnapshot::kIssueDrawPhaseCount>
    g_issue_draw_phase_max_us{};
std::atomic<uint32_t> g_pipeline_description_cache_hits{0};
std::atomic<uint32_t> g_pipeline_description_cache_misses{0};

std::atomic<uint32_t> g_sdl_callbacks{0};
std::atomic<uint32_t> g_sdl_underruns{0};
std::atomic<uint64_t> g_sdl_callback_us{0};
std::atomic<uint32_t> g_sdl_queue_depth_max{0};
std::atomic<uint32_t> g_audio_dispatches{0};
std::atomic<uint64_t> g_audio_dispatch_us{0};
std::atomic<uint32_t> g_audio_wait_ok{0};
std::atomic<uint32_t> g_audio_wait_to{0};
std::array<std::atomic<uint32_t>, kRpEndReasonCount> g_rp_end_reasons{};
std::atomic<uint32_t> g_shm_uploads{0};
std::atomic<uint64_t> g_shm_upload_bytes{0};
std::atomic<uint32_t> g_shm_uploads_hoisted{0};

struct CumulativeAtomics {
  std::atomic<uint64_t> pso_creates{0}, pso_create_us{0}, shader_translations{0},
      shader_translate_us{0}, texture_creates{0}, texture_loads{0}, shm_uploads{0},
      shm_upload_bytes{0}, issue_draw_us{0}, draws{0}, submissions{0}, submit_us{0},
      render_pass_begins{0}, queue_lock_wait_us{0}, present_us{0}, present_lock_wait_us{0},
      acquire_image_us{0}, write_watch_faults{0}, write_watch_fault_us{0},
      texture_host_bytes{0}, fence_waits{0}, fence_wait_us{0};
  std::array<std::atomic<uint64_t>, 16> idraw_phase_ns{};
} g_cum;
inline void CumAdd(std::atomic<uint64_t>& a, uint64_t v) { a.fetch_add(v, std::memory_order_relaxed); }

void BumpDrawsPerPresent() {
  const uint32_t cur = g_draws_this_present.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t maxv = g_draws_max_present.load(std::memory_order_relaxed);
  while (cur > maxv &&
         !g_draws_max_present.compare_exchange_weak(maxv, cur, std::memory_order_relaxed)) {
  }
}

}  // namespace

void OnVkDraw(uint32_t vertex_count, uint32_t index_count, bool indexed) {
  g_draws.fetch_add(1, std::memory_order_relaxed);
  CumAdd(g_cum.draws, 1);
  BumpDrawsPerPresent();
  if (indexed) {
    g_draws_indexed.fetch_add(1, std::memory_order_relaxed);
    g_indices.fetch_add(index_count, std::memory_order_relaxed);
  } else {
    g_vertices.fetch_add(vertex_count, std::memory_order_relaxed);
  }
}

void OnVkBindPipeline() {
  g_pipeline_binds.fetch_add(1, std::memory_order_relaxed);
}

void OnVkBeginRenderPass() {
  g_render_pass_begins.fetch_add(1, std::memory_order_relaxed);
  CumAdd(g_cum.render_pass_begins, 1);
}

void OnVkBeginRendering() {
  g_rendering_begins.fetch_add(1, std::memory_order_relaxed);
  CumAdd(g_cum.render_pass_begins, 1);
}

void OnVkPipelineBarrier() {
  g_barriers.fetch_add(1, std::memory_order_relaxed);
}

void OnVkBindDescriptorSets(uint32_t set_count) {
  g_descriptor_binds.fetch_add(set_count, std::memory_order_relaxed);
}

void OnGuestPresentFrame() {
  g_draws_last_present.store(g_draws_this_present.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
  g_draws_this_present.store(0, std::memory_order_relaxed);
  g_present_frames.fetch_add(1, std::memory_order_relaxed);
}

void OnVkRecordMicros(uint64_t micros) {
  g_vk_record_us.fetch_add(micros, std::memory_order_relaxed);
}

void OnVkSubmitMicros(uint64_t micros) {
  g_vk_submit_us.fetch_add(micros, std::memory_order_relaxed);
  CumAdd(g_cum.submissions, 1);
  CumAdd(g_cum.submit_us, micros);
}

void OnVkGpuExecMicros(uint64_t micros) {
  g_vk_gpu_exec_us.fetch_add(micros, std::memory_order_relaxed);
  g_vk_gpu_exec_samples.fetch_add(1, std::memory_order_relaxed);
}

void OnPsoCreateMicros(uint64_t micros) {
  g_pso_creates.fetch_add(1, std::memory_order_relaxed);
  CumAdd(g_cum.pso_creates, 1);
  CumAdd(g_cum.pso_create_us, micros);
  g_pso_create_us.fetch_add(micros, std::memory_order_relaxed);
}

void OnVkIssueDrawMicros(uint64_t micros) {
  g_vk_issue_draw_calls.fetch_add(1, std::memory_order_relaxed);
  CumAdd(g_cum.issue_draw_us, micros);
  g_vk_issue_draw_us.fetch_add(micros, std::memory_order_relaxed);
}

void OnVkDescriptorUpdateMicros(uint64_t micros) {
  g_vk_descriptor_update_calls.fetch_add(1, std::memory_order_relaxed);
  g_vk_descriptor_update_us.fetch_add(micros, std::memory_order_relaxed);
}

void OnVkExecCategoryMicros(uint32_t category, uint64_t micros) {
  if (category < kVkExecCategoryCount) {
    g_vk_exec_category_us[category].fetch_add(micros, std::memory_order_relaxed);
  }
}

void OnVkExecCategoryCommand(uint32_t category) {
  if (category < kVkExecCategoryCount) {
    g_vk_exec_category_cmds[category].fetch_add(1, std::memory_order_relaxed);
  }
}

void OnVkBindPipelineSkipped() {
  g_vk_bind_pipeline_skipped.fetch_add(1, std::memory_order_relaxed);
}

void OnVkBindDescriptorSkipped() {
  g_vk_bind_descriptor_skipped.fetch_add(1, std::memory_order_relaxed);
}

void OnVkTextureDescriptorReuseHit() {
  g_vk_texture_desc_reuse_hits.fetch_add(1, std::memory_order_relaxed);
}

void OnVkTextureDescriptorReuseMiss() {
  g_vk_texture_desc_reuse_misses.fetch_add(1, std::memory_order_relaxed);
}

void OnIssueDrawPhaseMicros(uint32_t phase, uint64_t micros) {
  if (phase >= DiagWindowSnapshot::kIssueDrawPhaseCount) {
    return;
  }
  g_issue_draw_phase_calls[phase].fetch_add(1, std::memory_order_relaxed);
  g_issue_draw_phase_us[phase].fetch_add(micros, std::memory_order_relaxed);
  uint64_t prev_max = g_issue_draw_phase_max_us[phase].load(std::memory_order_relaxed);
  while (micros > prev_max &&
         !g_issue_draw_phase_max_us[phase].compare_exchange_weak(prev_max, micros,
                                                                 std::memory_order_relaxed)) {
  }
}

void OnPipelineDescriptionCacheHit() {
  g_pipeline_description_cache_hits.fetch_add(1, std::memory_order_relaxed);
}

void OnPipelineDescriptionCacheMiss() {
  g_pipeline_description_cache_misses.fetch_add(1, std::memory_order_relaxed);
}

void OnSdlCallbackBegin() {}

void OnSdlCallbackEnd(uint32_t queue_depth_after_pop, bool underrun) {
  g_sdl_callbacks.fetch_add(1, std::memory_order_relaxed);
  if (underrun) {
    g_sdl_underruns.fetch_add(1, std::memory_order_relaxed);
  }
  uint32_t maxd = g_sdl_queue_depth_max.load(std::memory_order_relaxed);
  while (queue_depth_after_pop > maxd &&
         !g_sdl_queue_depth_max.compare_exchange_weak(maxd, queue_depth_after_pop,
                                                    std::memory_order_relaxed)) {
  }
}

void OnSdlQueueDepth(uint32_t depth) {
  uint32_t maxd = g_sdl_queue_depth_max.load(std::memory_order_relaxed);
  while (depth > maxd &&
         !g_sdl_queue_depth_max.compare_exchange_weak(maxd, depth, std::memory_order_relaxed)) {
  }
}

void OnAudioWorkerDispatchBegin() {}

void OnAudioWorkerDispatchEnd() {
  g_audio_dispatches.fetch_add(1, std::memory_order_relaxed);
}

void OnAudioWaitSuccess() {
  g_audio_wait_ok.fetch_add(1, std::memory_order_relaxed);
}

void OnAudioWaitTimeout() {
  g_audio_wait_to.fetch_add(1, std::memory_order_relaxed);
}

void OnVkRenderPassEnd(uint32_t reason) {
  if (reason < kRpEndReasonCount) {
    g_rp_end_reasons[reason].fetch_add(1, std::memory_order_relaxed);
  }
}

void OnShmUpload(uint64_t bytes) {
  g_shm_uploads.fetch_add(1, std::memory_order_relaxed);
  CumAdd(g_cum.shm_uploads, 1);
  CumAdd(g_cum.shm_upload_bytes, bytes);
  g_shm_upload_bytes.fetch_add(bytes, std::memory_order_relaxed);
}

void OnShaderTranslated(uint64_t micros) {
  CumAdd(g_cum.shader_translations, 1);
  CumAdd(g_cum.shader_translate_us, micros);
}

void OnTextureCreated() { CumAdd(g_cum.texture_creates, 1); }

void OnQueueLockWait(uint64_t micros) { CumAdd(g_cum.queue_lock_wait_us, micros); }

void OnFenceWait(uint64_t micros) {
  CumAdd(g_cum.fence_waits, 1);
  CumAdd(g_cum.fence_wait_us, micros);
}

void OnIssueDrawPhaseNanos(uint32_t phase, uint64_t nanos) {
  if (phase < 16) {
    CumAdd(g_cum.idraw_phase_ns[phase], nanos);
  }
}

void OnPresent(uint64_t lock_wait_micros, uint64_t present_micros) {
  CumAdd(g_cum.present_lock_wait_us, lock_wait_micros);
  CumAdd(g_cum.present_us, present_micros);
}

void OnAcquireImage(uint64_t micros) { CumAdd(g_cum.acquire_image_us, micros); }

void OnWriteWatchFault(uint64_t micros) {
  CumAdd(g_cum.write_watch_faults, 1);
  CumAdd(g_cum.write_watch_fault_us, micros);
}

void OnTextureLoad() { CumAdd(g_cum.texture_loads, 1); }

void SetTextureHostMemory(uint64_t bytes) {
  g_cum.texture_host_bytes.store(bytes, std::memory_order_relaxed);
}

CumulativeCounters GetCumulative() {
  CumulativeCounters c;
  c.pso_creates = g_cum.pso_creates.load(std::memory_order_relaxed);
  c.pso_create_us = g_cum.pso_create_us.load(std::memory_order_relaxed);
  c.shader_translations = g_cum.shader_translations.load(std::memory_order_relaxed);
  c.shader_translate_us = g_cum.shader_translate_us.load(std::memory_order_relaxed);
  c.texture_creates = g_cum.texture_creates.load(std::memory_order_relaxed);
  c.texture_loads = g_cum.texture_loads.load(std::memory_order_relaxed);
  c.shm_uploads = g_cum.shm_uploads.load(std::memory_order_relaxed);
  c.shm_upload_bytes = g_cum.shm_upload_bytes.load(std::memory_order_relaxed);
  c.issue_draw_us = g_cum.issue_draw_us.load(std::memory_order_relaxed);
  c.draws = g_cum.draws.load(std::memory_order_relaxed);
  c.submissions = g_cum.submissions.load(std::memory_order_relaxed);
  c.submit_us = g_cum.submit_us.load(std::memory_order_relaxed);
  c.render_pass_begins = g_cum.render_pass_begins.load(std::memory_order_relaxed);
  c.queue_lock_wait_us = g_cum.queue_lock_wait_us.load(std::memory_order_relaxed);
  c.present_us = g_cum.present_us.load(std::memory_order_relaxed);
  c.present_lock_wait_us = g_cum.present_lock_wait_us.load(std::memory_order_relaxed);
  c.acquire_image_us = g_cum.acquire_image_us.load(std::memory_order_relaxed);
  c.write_watch_faults = g_cum.write_watch_faults.load(std::memory_order_relaxed);
  c.write_watch_fault_us = g_cum.write_watch_fault_us.load(std::memory_order_relaxed);
  c.texture_host_bytes = g_cum.texture_host_bytes.load(std::memory_order_relaxed);
  c.fence_waits = g_cum.fence_waits.load(std::memory_order_relaxed);
  c.fence_wait_us = g_cum.fence_wait_us.load(std::memory_order_relaxed);
  for (uint32_t i = 0; i < 16; ++i) {
    c.idraw_phase_ns[i] = g_cum.idraw_phase_ns[i].load(std::memory_order_relaxed);
  }
  return c;
}

void OnShmUploadHoisted() {
  g_shm_uploads_hoisted.fetch_add(1, std::memory_order_relaxed);
}

void OnSdlCallbackAddMicros(uint64_t micros) {
  g_sdl_callback_us.fetch_add(micros, std::memory_order_relaxed);
}

void OnAudioWorkerAddMicros(uint64_t micros) {
  g_audio_dispatch_us.fetch_add(micros, std::memory_order_relaxed);
}

DiagWindowSnapshot ExchangeWindowAndReset() {
  DiagWindowSnapshot s;
  s.present_frames = g_present_frames.exchange(0, std::memory_order_relaxed);
  s.draws = g_draws.exchange(0, std::memory_order_relaxed);
  s.draws_indexed = g_draws_indexed.exchange(0, std::memory_order_relaxed);
  s.vertices = g_vertices.exchange(0, std::memory_order_relaxed);
  s.indices = g_indices.exchange(0, std::memory_order_relaxed);
  s.pipeline_binds = g_pipeline_binds.exchange(0, std::memory_order_relaxed);
  s.render_pass_begins = g_render_pass_begins.exchange(0, std::memory_order_relaxed);
  s.rendering_begins = g_rendering_begins.exchange(0, std::memory_order_relaxed);
  s.barriers = g_barriers.exchange(0, std::memory_order_relaxed);
  s.descriptor_binds = g_descriptor_binds.exchange(0, std::memory_order_relaxed);
  s.draws_max_per_present = g_draws_max_present.exchange(0, std::memory_order_relaxed);
  s.draws_last_present = g_draws_last_present.exchange(0, std::memory_order_relaxed);
  s.vk_record_us = g_vk_record_us.exchange(0, std::memory_order_relaxed);
  s.vk_submit_us = g_vk_submit_us.exchange(0, std::memory_order_relaxed);
  s.vk_gpu_exec_us = g_vk_gpu_exec_us.exchange(0, std::memory_order_relaxed);
  s.vk_gpu_exec_samples = g_vk_gpu_exec_samples.exchange(0, std::memory_order_relaxed);
  s.pso_creates = g_pso_creates.exchange(0, std::memory_order_relaxed);
  s.pso_create_us_sum = g_pso_create_us.exchange(0, std::memory_order_relaxed);
  s.vk_issue_draw_us = g_vk_issue_draw_us.exchange(0, std::memory_order_relaxed);
  s.vk_issue_draw_calls = g_vk_issue_draw_calls.exchange(0, std::memory_order_relaxed);
  s.vk_descriptor_update_us = g_vk_descriptor_update_us.exchange(0, std::memory_order_relaxed);
  s.vk_descriptor_update_calls = g_vk_descriptor_update_calls.exchange(0, std::memory_order_relaxed);
  for (uint32_t i = 0; i < kVkExecCategoryCount; ++i) {
    switch (i) {
      case kVkExecPipeline:
        s.vk_exec_pipeline_us = g_vk_exec_category_us[i].exchange(0, std::memory_order_relaxed);
        s.vk_exec_pipeline_cmds = g_vk_exec_category_cmds[i].exchange(0, std::memory_order_relaxed);
        break;
      case kVkExecDescriptor:
        s.vk_exec_descriptor_us = g_vk_exec_category_us[i].exchange(0, std::memory_order_relaxed);
        s.vk_exec_descriptor_cmds = g_vk_exec_category_cmds[i].exchange(0, std::memory_order_relaxed);
        break;
      case kVkExecDraw:
        s.vk_exec_draw_us = g_vk_exec_category_us[i].exchange(0, std::memory_order_relaxed);
        s.vk_exec_draw_cmds = g_vk_exec_category_cmds[i].exchange(0, std::memory_order_relaxed);
        break;
      case kVkExecBarrier:
        s.vk_exec_barrier_us = g_vk_exec_category_us[i].exchange(0, std::memory_order_relaxed);
        s.vk_exec_barrier_cmds = g_vk_exec_category_cmds[i].exchange(0, std::memory_order_relaxed);
        break;
      case kVkExecOther:
        s.vk_exec_other_us = g_vk_exec_category_us[i].exchange(0, std::memory_order_relaxed);
        break;
      default:
        break;
    }
  }
  s.vk_bind_pipeline_skipped = g_vk_bind_pipeline_skipped.exchange(0, std::memory_order_relaxed);
  s.vk_bind_descriptor_skipped = g_vk_bind_descriptor_skipped.exchange(0, std::memory_order_relaxed);
  s.vk_texture_descriptor_reuse_hits =
      g_vk_texture_desc_reuse_hits.exchange(0, std::memory_order_relaxed);
  s.vk_texture_descriptor_reuse_misses =
      g_vk_texture_desc_reuse_misses.exchange(0, std::memory_order_relaxed);
  for (uint32_t i = 0; i < DiagWindowSnapshot::kIssueDrawPhaseCount; ++i) {
    s.issue_draw_phase_us[i] = g_issue_draw_phase_us[i].exchange(0, std::memory_order_relaxed);
    s.issue_draw_phase_calls[i] = g_issue_draw_phase_calls[i].exchange(0, std::memory_order_relaxed);
    s.issue_draw_phase_max_us[i] =
        g_issue_draw_phase_max_us[i].exchange(0, std::memory_order_relaxed);
  }
  s.vk_pipeline_description_cache_hits =
      g_pipeline_description_cache_hits.exchange(0, std::memory_order_relaxed);
  s.vk_pipeline_description_cache_misses =
      g_pipeline_description_cache_misses.exchange(0, std::memory_order_relaxed);
  s.sdl_callbacks = g_sdl_callbacks.exchange(0, std::memory_order_relaxed);
  s.sdl_underruns = g_sdl_underruns.exchange(0, std::memory_order_relaxed);
  s.sdl_callback_us = g_sdl_callback_us.exchange(0, std::memory_order_relaxed);
  s.sdl_queue_depth_max = g_sdl_queue_depth_max.exchange(0, std::memory_order_relaxed);
  s.audio_worker_dispatches = g_audio_dispatches.exchange(0, std::memory_order_relaxed);
  s.audio_worker_dispatch_us = g_audio_dispatch_us.exchange(0, std::memory_order_relaxed);
  s.audio_wait_success = g_audio_wait_ok.exchange(0, std::memory_order_relaxed);
  s.audio_wait_timeout = g_audio_wait_to.exchange(0, std::memory_order_relaxed);
  for (uint32_t i = 0; i < kRpEndReasonCount; ++i) {
    s.rp_end_reasons[i] = g_rp_end_reasons[i].exchange(0, std::memory_order_relaxed);
  }
  s.shm_uploads = g_shm_uploads.exchange(0, std::memory_order_relaxed);
  s.shm_upload_bytes = g_shm_upload_bytes.exchange(0, std::memory_order_relaxed);
  s.shm_uploads_hoisted = g_shm_uploads_hoisted.exchange(0, std::memory_order_relaxed);
  return s;
}

}  // namespace rex::diag

#endif  // REX_DIAG_PROFILE