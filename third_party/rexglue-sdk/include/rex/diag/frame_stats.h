/**
 * Lightweight per-window counters for Android diagnostic builds (REX_DIAG_PROFILE).
 * No logging here — sonic4_timing_hooks aggregates into PROBE DIAG_* lines.
 */
#pragma once

#include <chrono>
#include <cstdint>

namespace rex::diag {

enum VkExecCategory : uint32_t {
  kVkExecPipeline = 0,
  kVkExecDescriptor = 1,
  kVkExecDraw = 2,
  kVkExecBarrier = 3,
  kVkExecOther = 4,
  kVkExecCategoryCount = 5,
};

// Why an open render pass / dynamic rendering instance was ended (Exp-012).
enum RenderPassEndReason : uint32_t {
  kRpEndUnknown = 0,
  kRpEndShmUpload,      // SharedMemory upload (vkCmdCopyBuffer) of CPU-written guest data
  kRpEndTextureLoad,    // texture cache load / untile dispatch or copy
  kRpEndResolve,        // IssueCopy (EDRAM resolve / clear)
  kRpEndRtCache,        // render target cache transfers, host depth store, dumps
  kRpEndBarrier,        // other pending pipeline barriers before a draw
  kRpEndFbSwitch,       // different framebuffer / attachments for the next draw
  kRpEndSubmission,     // end of submission / swap
  kRpEndPrimitive,      // primitive processor builtin index upload
  kRpEndReasonCount,
};

// Monotonic totals (never reset) for per-frame spike attribution (Exp-015).
struct CumulativeCounters {
  uint64_t pso_creates = 0;
  uint64_t pso_create_us = 0;
  uint64_t shader_translations = 0;
  uint64_t shader_translate_us = 0;
  uint64_t texture_creates = 0;
  uint64_t texture_loads = 0;
  uint64_t shm_uploads = 0;
  uint64_t shm_upload_bytes = 0;
  uint64_t issue_draw_us = 0;
  uint64_t draws = 0;
  uint64_t submissions = 0;
  uint64_t submit_us = 0;
  uint64_t render_pass_begins = 0;
  uint64_t queue_lock_wait_us = 0;     // GPU thread waiting for the shared VkQueue lock
  uint64_t present_us = 0;             // UI thread: vkQueuePresentKHR (lock held)
  uint64_t present_lock_wait_us = 0;   // UI thread waiting for the VkQueue lock to present
  uint64_t acquire_image_us = 0;       // UI thread: vkAcquireNextImageKHR
  uint64_t write_watch_faults = 0;
  uint64_t write_watch_fault_us = 0;
  uint64_t texture_host_bytes = 0;  // gauge
  uint64_t fence_waits = 0;
  uint64_t fence_wait_us = 0;
  uint64_t idraw_phase_ns[16] = {};
};

struct DiagWindowSnapshot {
  uint32_t present_frames{0};
  uint32_t draws{0};
  uint32_t draws_indexed{0};
  uint64_t vertices{0};
  uint64_t indices{0};
  uint32_t pipeline_binds{0};
  uint32_t render_pass_begins{0};
  uint32_t rendering_begins{0};
  uint32_t barriers{0};
  uint32_t descriptor_binds{0};
  uint32_t draws_max_per_present{0};
  uint32_t draws_last_present{0};
  uint64_t vk_record_us{0};
  uint64_t vk_submit_us{0};
  uint64_t vk_gpu_exec_us{0};
  uint32_t vk_gpu_exec_samples{0};
  uint32_t pso_creates{0};
  uint64_t pso_create_us_sum{0};
  uint64_t vk_issue_draw_us{0};
  uint32_t vk_issue_draw_calls{0};
  uint64_t vk_descriptor_update_us{0};
  uint32_t vk_descriptor_update_calls{0};
  uint64_t vk_exec_pipeline_us{0};
  uint64_t vk_exec_descriptor_us{0};
  uint64_t vk_exec_draw_us{0};
  uint64_t vk_exec_barrier_us{0};
  uint64_t vk_exec_other_us{0};
  uint32_t vk_exec_pipeline_cmds{0};
  uint32_t vk_exec_descriptor_cmds{0};
  uint32_t vk_exec_draw_cmds{0};
  uint32_t vk_exec_barrier_cmds{0};
  uint32_t vk_bind_pipeline_skipped{0};
  uint32_t vk_bind_descriptor_skipped{0};
  uint32_t vk_texture_descriptor_reuse_hits{0};
  uint32_t vk_texture_descriptor_reuse_misses{0};
  static constexpr uint32_t kIssueDrawPhaseCount = 16;
  uint64_t issue_draw_phase_us[kIssueDrawPhaseCount]{};
  uint32_t issue_draw_phase_calls[kIssueDrawPhaseCount]{};
  uint64_t issue_draw_phase_max_us[kIssueDrawPhaseCount]{};
  uint32_t vk_pipeline_description_cache_hits{0};
  uint32_t vk_pipeline_description_cache_misses{0};
  uint32_t sdl_callbacks{0};
  uint32_t sdl_underruns{0};
  uint64_t sdl_callback_us{0};
  uint32_t sdl_queue_depth_max{0};
  uint32_t audio_worker_dispatches{0};
  uint64_t audio_worker_dispatch_us{0};
  uint32_t audio_wait_success{0};
  uint32_t audio_wait_timeout{0};
  uint32_t rp_end_reasons[kRpEndReasonCount]{};
  uint32_t shm_uploads{0};
  uint64_t shm_upload_bytes{0};
  uint32_t shm_uploads_hoisted{0};
};

#ifdef REX_FRAME_STATS

void OnVkDraw(uint32_t vertex_count, uint32_t index_count, bool indexed);
void OnVkBindPipeline();
void OnVkBeginRenderPass();
void OnVkBeginRendering();
void OnVkPipelineBarrier();
void OnVkBindDescriptorSets(uint32_t set_count);
void OnGuestPresentFrame();
void OnVkRecordMicros(uint64_t micros);
void OnVkSubmitMicros(uint64_t micros);
void OnVkGpuExecMicros(uint64_t micros);
void OnPsoCreateMicros(uint64_t micros);
void OnVkIssueDrawMicros(uint64_t micros);
void OnVkDescriptorUpdateMicros(uint64_t micros);
void OnVkDescriptorUpdateCall();
void OnVkExecCategoryMicros(uint32_t category, uint64_t micros);
void OnVkExecCategoryCommand(uint32_t category);
void OnVkBindPipelineSkipped();
void OnVkBindDescriptorSkipped();
void OnVkTextureDescriptorReuseHit();
void OnVkTextureDescriptorReuseMiss();
void OnIssueDrawPhaseMicros(uint32_t phase, uint64_t micros);
void OnPipelineDescriptionCacheHit();
void OnPipelineDescriptionCacheMiss();

void OnSdlCallbackBegin();
void OnSdlCallbackEnd(uint32_t queue_depth_after_pop, bool underrun);
void OnSdlCallbackAddMicros(uint64_t micros);
void OnSdlQueueDepth(uint32_t depth);
void OnAudioWorkerDispatchBegin();
void OnAudioWorkerDispatchEnd();
void OnAudioWorkerAddMicros(uint64_t micros);
void OnAudioWaitSuccess();
void OnAudioWaitTimeout();
void OnVkRenderPassEnd(uint32_t reason);
void OnShmUpload(uint64_t bytes);
void OnShmUploadHoisted();
void OnShaderTranslated(uint64_t micros);
void OnWriteWatchFault(uint64_t micros);
void OnFenceWait(uint64_t micros);
void OnIssueDrawPhaseNanos(uint32_t phase, uint64_t nanos);
void OnQueueLockWait(uint64_t micros);
void OnPresent(uint64_t lock_wait_micros, uint64_t present_micros);
void OnAcquireImage(uint64_t micros);
void OnTextureCreated();
void OnTextureLoad();
void SetTextureHostMemory(uint64_t bytes);
CumulativeCounters GetCumulative();

DiagWindowSnapshot ExchangeWindowAndReset();

#else

inline void OnVkDraw(uint32_t, uint32_t, bool) {}
inline void OnVkBindPipeline() {}
inline void OnVkBeginRenderPass() {}
inline void OnVkBeginRendering() {}
inline void OnVkPipelineBarrier() {}
inline void OnVkBindDescriptorSets(uint32_t) {}
inline void OnGuestPresentFrame() {}
inline void OnVkRecordMicros(uint64_t) {}
inline void OnVkSubmitMicros(uint64_t) {}
inline void OnVkGpuExecMicros(uint64_t) {}
inline void OnPsoCreateMicros(uint64_t) {}
inline void OnVkIssueDrawMicros(uint64_t) {}
inline void OnVkDescriptorUpdateMicros(uint64_t) {}
inline void OnVkDescriptorUpdateCall() {}
inline void OnVkExecCategoryMicros(uint32_t, uint64_t) {}
inline void OnVkExecCategoryCommand(uint32_t) {}
inline void OnVkBindPipelineSkipped() {}
inline void OnVkBindDescriptorSkipped() {}
inline void OnVkTextureDescriptorReuseHit() {}
inline void OnVkTextureDescriptorReuseMiss() {}
inline void OnIssueDrawPhaseMicros(uint32_t, uint64_t) {}
inline void OnPipelineDescriptionCacheHit() {}
inline void OnPipelineDescriptionCacheMiss() {}
inline void OnSdlCallbackBegin() {}
inline void OnSdlCallbackEnd(uint32_t, bool) {}
inline void OnSdlCallbackAddMicros(uint64_t) {}
inline void OnSdlQueueDepth(uint32_t) {}
inline void OnAudioWorkerDispatchBegin() {}
inline void OnAudioWorkerDispatchEnd() {}
inline void OnAudioWorkerAddMicros(uint64_t) {}
inline void OnAudioWaitSuccess() {}
inline void OnAudioWaitTimeout() {}
inline void OnVkRenderPassEnd(uint32_t) {}
inline void OnShmUpload(uint64_t) {}
inline void OnShmUploadHoisted() {}
inline void OnShaderTranslated(uint64_t) {}
inline void OnWriteWatchFault(uint64_t) {}
inline void OnFenceWait(uint64_t) {}
inline void OnIssueDrawPhaseNanos(uint32_t, uint64_t) {}
inline void OnQueueLockWait(uint64_t) {}
inline void OnPresent(uint64_t, uint64_t) {}
inline void OnAcquireImage(uint64_t) {}
inline void OnTextureCreated() {}
inline void OnTextureLoad() {}
inline void SetTextureHostMemory(uint64_t) {}
inline CumulativeCounters GetCumulative() { return {}; }
inline DiagWindowSnapshot ExchangeWindowAndReset() { return {}; }

#endif  // REX_FRAME_STATS

enum IssueDrawPhase : uint32_t {
  kIssueDrawShaderPrep = 0,
  kIssueDrawPrimitive,
  kIssueDrawShaderSampler,
  kIssueDrawRequestTextures,
  kIssueDrawRenderTarget,
  kIssueDrawConfigurePipeline,
  kIssueDrawPipelineBind,
  kIssueDrawDynamicState,
  kIssueDrawSystemConstants,
  kIssueDrawUpdateBindings,
  kIssueDrawVertexFetch,
  kIssueDrawMemexportSync,
  kIssueDrawBarriersRenderPass,
  kIssueDrawDeferredDraw,
  kIssueDrawMemexportPost,
  kIssueDrawPhaseCount = 16,
};

#ifdef REX_DIAG_PROFILE

class IssueDrawPhaseScope {
 public:
  explicit IssueDrawPhaseScope(uint32_t phase)
      : phase_(phase), started_(std::chrono::steady_clock::now()) {}
  ~IssueDrawPhaseScope() {
    const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - started_)
                                .count();
    OnIssueDrawPhaseMicros(phase_, static_cast<uint64_t>(elapsed_us));
  }

 private:
  uint32_t phase_;
  std::chrono::steady_clock::time_point started_;
};

#define REX_IDRAW_PHASE2(line, phase) \
  ::rex::diag::IssueDrawPhaseScope _rex_idraw_phase_##line(phase)
#define REX_IDRAW_PHASE(phase) REX_IDRAW_PHASE2(__LINE__, phase)

#elif defined(REX_FRAME_STATS)

// Exp-018: cumulative per-phase time for spike attribution (telemetry builds).
class IssueDrawPhaseCumulativeScope {
 public:
  explicit IssueDrawPhaseCumulativeScope(uint32_t phase)
      : phase_(phase), started_(std::chrono::steady_clock::now()) {}
  ~IssueDrawPhaseCumulativeScope() {
    OnIssueDrawPhaseNanos(phase_, uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               std::chrono::steady_clock::now() - started_)
                                               .count()));
  }

 private:
  uint32_t phase_;
  std::chrono::steady_clock::time_point started_;
};

#define REX_IDRAW_PHASE2(line, phase) \
  ::rex::diag::IssueDrawPhaseCumulativeScope _rex_idraw_phase_##line(phase)
#define REX_IDRAW_PHASE(phase) REX_IDRAW_PHASE2(__LINE__, phase)

#else

#define REX_IDRAW_PHASE(phase)

#endif

}  // namespace rex::diag