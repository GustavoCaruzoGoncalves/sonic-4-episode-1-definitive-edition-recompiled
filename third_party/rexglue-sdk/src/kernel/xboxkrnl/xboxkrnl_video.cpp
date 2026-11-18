/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

// Disable warnings about unused parameters for kernel functions
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <string>

#include <rex/chrono/clock.h>
#include <rex/cvar.h>
#include <rex/thread.h>
#include <rex/graphics/pipeline/texture/info.h>
#include <rex/graphics/register_file.h>
#include <rex/graphics/video_mode_util.h>
#include <rex/graphics/xenos.h>
#include <rex/kernel/xboxkrnl/private.h>
#include <rex/kernel/xboxkrnl/rtl.h>
#include <rex/kernel/xboxkrnl/video.h>
#include <rex/memory/utils.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <rex/logging.h>
#include <rex/hook.h>
#include <rex/types.h>
#include <rex/runtime.h>
#include <rex/system/export_resolver.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xtypes.h>
#include <rex/ui/flags.h>

namespace {
// Display gamma type: 0 - linear, 1 - sRGB (CRT), 2 - BT.709 (HDTV), 3 - power
constexpr uint32_t kDisplayGammaType = 2;
// Display gamma power (used with gamma type 3)
constexpr double kDisplayGammaPower = 2.22222233;

uint32_t GetConfiguredVideoModeWidth() {
  int32_t configured_width = REXCVAR_GET(video_mode_width);
  if (!rex::cvar::HasNonDefaultValue("video_mode_width")) {
    if (rex::cvar::HasNonDefaultValue("window_width") && REXCVAR_GET(window_width) > 0) {
      configured_width = REXCVAR_GET(window_width);
    } else {
      int32_t preset_width = 0;
      int32_t preset_height = 0;
      if (rex::graphics::video_mode_util::TryGetResolutionPresetFromCVar(preset_width,
                                                                         preset_height)) {
        configured_width = preset_width;
      }
    }
  }
  return uint32_t(std::clamp(configured_width, 640, 0x0FFF));
}

uint32_t GetConfiguredVideoModeHeight() {
  int32_t configured_height = REXCVAR_GET(video_mode_height);
  if (!rex::cvar::HasNonDefaultValue("video_mode_height")) {
    if (rex::cvar::HasNonDefaultValue("window_height") && REXCVAR_GET(window_height) > 0) {
      configured_height = REXCVAR_GET(window_height);
    } else {
      int32_t preset_width = 0;
      int32_t preset_height = 0;
      if (rex::graphics::video_mode_util::TryGetResolutionPresetFromCVar(preset_width,
                                                                         preset_height)) {
        configured_height = preset_height;
      }
    }
  }
  return uint32_t(std::clamp(configured_height, 480, 0x0FFF));
}

float GetConfiguredVideoModeRefreshRate() {
  double refresh_rate_hz = std::clamp(REXCVAR_GET(video_mode_refresh_rate), 24.0, 240.0);
  return float(refresh_rate_hz);
}

void WarnNoGpuEmulation(const char* export_name, std::atomic<bool>& warned) {
  if (!warned.exchange(true)) {
    REXKRNL_WARN("{}: no GPU emulation loaded (gpu_plugin not set); call ignored", export_name);
  }
}

}  // namespace

namespace rex::kernel::xboxkrnl {
using namespace rex::system;

REXCVAR_DEFINE_BOOL(guest_frame_pacing, true, "Clock",
                     "Pace the guest main loop to the configured video refresh rate");

REXCVAR_DEFINE_DOUBLE(guest_max_physics_dt, 1.0 / 60.0, "Clock",
                        "Maximum physics delta in seconds when values look like seconds");

REXCVAR_DEFINE_DOUBLE(guest_max_physics_dt_frames, 1.0, "Clock",
                        "Maximum physics delta in frame ticks when values look like frames "
                        "(|dt| > 0.25)");

REXCVAR_DEFINE_DOUBLE(guest_physics_dt_scale, 1.0, "Clock",
                        "Multiplier applied to physics delta time before clamping");

REXCVAR_DEFINE_BOOL(guest_use_fixed_physics_dt, false, "Clock",
                     "Force physics delta to exactly 1/60 s on every sub_821B35C8 call "
                     "(diagnostic; ignores measured dt)");

REXCVAR_DEFINE_BOOL(guest_use_paced_frame_dt, false, "Clock",
                     "Use wall-clock paced main-loop delta for the first sub_821B35C8 call "
                     "each frame (extras still use clamped guest dt)");

REXCVAR_DEFINE_BOOL(guest_use_paced_render_dt, false, "Clock",
                     "Replace scene/update f1 with paced wall delta (diagnostic; can break render)");

REXCVAR_DEFINE_DOUBLE(guest_physics_extra_step_scale, 1.0, "Clock",
                        "Multiplier on paced frame dt for 2+ sub_821B35C8 calls within one "
                        "main-loop frame when guest_use_paced_frame_dt is on");

REXCVAR_DEFINE_BOOL(guest_physics_frame_dt_budget, false, "Clock",
                     "Limit total sub_821B35C8 delta per main-loop frame (off: breaks splash)");

REXCVAR_DEFINE_BOOL(guest_clamp_runaway_frame_dt, false, "Clock",
                     "Clamp scene f1 above guest_max_physics_dt (off: breaks splash)");

REXCVAR_DEFINE_INT32(timing_log_interval_sec, 0, "Clock",
                     "Log guest frame pacing stats every N seconds (0 = disabled)");

namespace {
std::atomic<uint64_t> g_last_guest_frame_host_ticks{0};
std::atomic<uint64_t> g_pacing_window_start_ticks{0};
std::atomic<uint32_t> g_pacing_window_count{0};

std::atomic<uint64_t> g_physics_window_start_ticks{0};
std::atomic<uint32_t> g_physics_window_count{0};
std::atomic<uint32_t> g_physics_window_main_loop_count{0};
std::atomic<uint32_t> g_physics_window_extra_steps{0};
std::atomic<double> g_physics_window_f1_sum{0.0};
std::atomic<float> g_physics_window_f1_max{0.0f};

std::atomic<double> g_paced_frame_delta_sec{1.0 / 60.0};
std::atomic<float> g_physics_dt_budget_remaining{0.0f};
std::atomic<uint32_t> g_physics_steps_this_frame{0};

constexpr double kPhysicsDtFrameHeuristic = 0.25;
constexpr double kMaxPacedFrameDeltaSec = 0.05;
}  // namespace

float GuestGetPacedFrameDeltaSeconds() {
  return static_cast<float>(g_paced_frame_delta_sec.load(std::memory_order_relaxed));
}

void GuestSanitizePrimaryFrameDelta(float& delta_seconds) {
  if (REXCVAR_GET(guest_frame_pacing) && REXCVAR_GET(guest_use_paced_render_dt)) {
    delta_seconds = GuestGetPacedFrameDeltaSeconds();
    return;
  }
  delta_seconds = GuestClampPhysicsDeltaSeconds(delta_seconds);
}

void GuestCapRunawayFrameDelta(float& delta_seconds) {
  if (!REXCVAR_GET(guest_clamp_runaway_frame_dt)) {
    return;
  }
  const float abs_in = std::abs(delta_seconds);
  const double max_dt = abs_in > kPhysicsDtFrameHeuristic
                            ? REXCVAR_GET(guest_max_physics_dt_frames)
                            : REXCVAR_GET(guest_max_physics_dt);
  if (abs_in > static_cast<float>(max_dt)) {
    delta_seconds = GuestClampPhysicsDeltaSeconds(delta_seconds);
  }
}

void GuestWriteFrameDeltaToGuestVAs(const uint32_t* guest_virtual_addresses, size_t count,
                                    float delta_seconds) {
  if (!guest_virtual_addresses || count == 0) {
    return;
  }
  auto* kernel_state = REX_KERNEL_STATE();
  if (!kernel_state) {
    return;
  }
  auto* memory = kernel_state->memory();
  if (!memory) {
    return;
  }
  constexpr uint32_t kWriteProtectMask =
      memory::kMemoryProtectWrite | memory::kMemoryProtectWriteCombine;

  for (size_t i = 0; i < count; ++i) {
    const uint32_t va = guest_virtual_addresses[i];
    if (!va) {
      continue;
    }
    rex::memory::BaseHeap* heap = memory->LookupHeap(va);
    if (!heap) {
      continue;
    }
    uint32_t protect = 0;
    if (!heap->QueryProtect(va, &protect) || !(protect & kWriteProtectMask)) {
      continue;
    }
    if (auto* dst = memory->TranslateVirtual<float*>(va)) {
      memory::store_and_swap(dst, delta_seconds);
    }
  }
}

void GuestPublishPacedFrameDeltaTo(const uint32_t* guest_virtual_addresses, size_t count) {
  if (!REXCVAR_GET(guest_frame_pacing) || !guest_virtual_addresses || count == 0) {
    return;
  }
  GuestWriteFrameDeltaToGuestVAs(guest_virtual_addresses, count,
                                 GuestGetPacedFrameDeltaSeconds());
}

float GuestClampPhysicsDeltaSeconds(float delta_seconds) {
  if (REXCVAR_GET(guest_use_fixed_physics_dt)) {
    return static_cast<float>(1.0 / 60.0);
  }

  const double scale = REXCVAR_GET(guest_physics_dt_scale);
  const double abs_in = std::abs(static_cast<double>(delta_seconds));
  const double max_dt = abs_in > kPhysicsDtFrameHeuristic
                            ? REXCVAR_GET(guest_max_physics_dt_frames)
                            : REXCVAR_GET(guest_max_physics_dt);

  double scaled = static_cast<double>(delta_seconds) * scale;
  if (scaled < 0.0) {
    scaled = 0.0;
  } else if (scaled > max_dt) {
    scaled = max_dt;
  } else if (-scaled > max_dt) {
    scaled = -max_dt;
  }
  return static_cast<float>(scaled);
}

void GuestSanitizePhysicsStepInputs(float& f1, float& f2, float& f3, float& f4, float& f5,
                                    float& f6) {
  if (!REXCVAR_GET(guest_use_fixed_physics_dt) && !REXCVAR_GET(guest_use_paced_frame_dt)) {
    const auto negligible = [](float v) { return std::abs(v) <= 1e-8f; };
    if (negligible(f1) && negligible(f2) && negligible(f3) && negligible(f4) &&
        negligible(f5) && negligible(f6)) {
      return;
    }
  }

  const uint32_t step_index =
      g_physics_steps_this_frame.fetch_add(1, std::memory_order_relaxed);
  if (step_index > 0) {
    g_physics_window_extra_steps.fetch_add(1, std::memory_order_relaxed);
  }

  if (REXCVAR_GET(guest_use_fixed_physics_dt)) {
    const float dt = static_cast<float>(1.0 / 60.0);
    f1 = f2 = f3 = f4 = f5 = f6 = dt;
  } else if (REXCVAR_GET(guest_physics_frame_dt_budget) &&
             !REXCVAR_GET(guest_use_paced_frame_dt)) {
    float budget = g_physics_dt_budget_remaining.load(std::memory_order_relaxed);
    float peak = 0.0f;
    float* vals[] = {&f1, &f2, &f3, &f4, &f5, &f6};
    for (float* v : vals) {
      *v = GuestClampPhysicsDeltaSeconds(*v);
      peak = std::max(peak, std::abs(*v));
    }
    if (peak > 0.0f && budget > 0.0f) {
      const float use = std::min(peak, budget);
      const float ratio = use / peak;
      for (float* v : vals) {
        *v *= ratio;
      }
      g_physics_dt_budget_remaining.store(budget - use, std::memory_order_relaxed);
    }
  } else if (REXCVAR_GET(guest_use_paced_frame_dt) && step_index == 0) {
    const double scale = REXCVAR_GET(guest_physics_dt_scale);
    double frame_dt = g_paced_frame_delta_sec.load(std::memory_order_relaxed) * scale;
    if (frame_dt < 0.0) {
      frame_dt = 0.0;
    } else if (frame_dt > kMaxPacedFrameDeltaSec) {
      frame_dt = kMaxPacedFrameDeltaSec;
    }
    const float dt = static_cast<float>(frame_dt);
    f1 = f2 = f3 = f4 = f5 = f6 = dt;
  } else if (REXCVAR_GET(guest_use_paced_frame_dt)) {
    const double scale = REXCVAR_GET(guest_physics_dt_scale);
    const double extra_scale = REXCVAR_GET(guest_physics_extra_step_scale);
    double frame_dt = g_paced_frame_delta_sec.load(std::memory_order_relaxed) * scale * extra_scale;
    if (frame_dt < 0.0) {
      frame_dt = 0.0;
    } else if (frame_dt > kMaxPacedFrameDeltaSec) {
      frame_dt = kMaxPacedFrameDeltaSec;
    }
    const float paced_extra = static_cast<float>(frame_dt);
    f1 = GuestClampPhysicsDeltaSeconds(f1);
    f2 = GuestClampPhysicsDeltaSeconds(f2);
    f3 = GuestClampPhysicsDeltaSeconds(f3);
    f4 = GuestClampPhysicsDeltaSeconds(f4);
    f5 = GuestClampPhysicsDeltaSeconds(f5);
    f6 = GuestClampPhysicsDeltaSeconds(f6);
    if (extra_scale > 0.0) {
      const auto min_abs = [](float a, float b) {
        return std::abs(a) <= std::abs(b) ? a : b;
      };
      f1 = min_abs(f1, paced_extra);
      f2 = min_abs(f2, paced_extra);
      f3 = min_abs(f3, paced_extra);
      f4 = min_abs(f4, paced_extra);
      f5 = min_abs(f5, paced_extra);
      f6 = min_abs(f6, paced_extra);
    }
  } else {
    f1 = GuestClampPhysicsDeltaSeconds(f1);
    f2 = GuestClampPhysicsDeltaSeconds(f2);
    f3 = GuestClampPhysicsDeltaSeconds(f3);
    f4 = GuestClampPhysicsDeltaSeconds(f4);
    f5 = GuestClampPhysicsDeltaSeconds(f5);
    f6 = GuestClampPhysicsDeltaSeconds(f6);
  }

  const int32_t log_interval_sec = REXCVAR_GET(timing_log_interval_sec);
  if (log_interval_sec <= 0) {
    return;
  }

  const uint64_t host_freq = chrono::Clock::QueryHostTickFrequency();
  const uint64_t now = chrono::Clock::QueryHostTickCount();

  g_physics_window_f1_sum.fetch_add(static_cast<double>(f1), std::memory_order_relaxed);
  float prev_max = g_physics_window_f1_max.load(std::memory_order_relaxed);
  while (f1 > prev_max &&
         !g_physics_window_f1_max.compare_exchange_weak(prev_max, f1, std::memory_order_relaxed)) {
  }

  uint64_t window_start = g_physics_window_start_ticks.load(std::memory_order_relaxed);
  if (!window_start) {
    g_physics_window_start_ticks.store(now, std::memory_order_relaxed);
    g_physics_window_count.store(1, std::memory_order_relaxed);
    return;
  }

  const uint32_t steps_in_window =
      g_physics_window_count.fetch_add(1, std::memory_order_relaxed) + 1;
  const uint64_t window_elapsed = now - window_start;
  const uint64_t log_interval_ticks = static_cast<uint64_t>(log_interval_sec) * host_freq;
  if (window_elapsed < log_interval_ticks) {
    return;
  }

  const double window_sec = static_cast<double>(window_elapsed) / host_freq;
  const double steps_per_sec = window_sec > 0.0 ? steps_in_window / window_sec : 0.0;
  const double avg_f1 = steps_in_window > 0
                            ? g_physics_window_f1_sum.load(std::memory_order_relaxed) /
                                  steps_in_window
                            : 0.0;
  const float max_f1 = g_physics_window_f1_max.load(std::memory_order_relaxed);
  const uint32_t main_loops = g_physics_window_main_loop_count.exchange(0, std::memory_order_relaxed);
  const uint32_t extra_steps = g_physics_window_extra_steps.exchange(0, std::memory_order_relaxed);

  REXKRNL_INFO(
      "Physics steps: {:.1f}/s (main loop {:.1f}/s, +{} extra steps), avg f1={:.5f} max "
      "f1={:.5f}, paced_dt={} extra_scale={} frame_dt={:.5f}s",
      steps_per_sec, window_sec > 0.0 ? main_loops / window_sec : 0.0, extra_steps, avg_f1,
      max_f1, REXCVAR_GET(guest_use_paced_frame_dt), REXCVAR_GET(guest_physics_extra_step_scale),
      GuestGetPacedFrameDeltaSeconds());

  g_physics_window_start_ticks.store(now, std::memory_order_relaxed);
  g_physics_window_count.store(0, std::memory_order_relaxed);
  g_physics_window_f1_sum.store(0.0, std::memory_order_relaxed);
  g_physics_window_f1_max.store(0.0f, std::memory_order_relaxed);
}

void PaceGuestMainLoopFrame() {
  if (!REXCVAR_GET(guest_frame_pacing)) {
    return;
  }

  X_VIDEO_MODE mode;
  VdQueryVideoMode(&mode);
  const double refresh_hz = std::max(1.0, static_cast<double>(mode.refresh_rate));
  const uint64_t host_freq = chrono::Clock::QueryHostTickFrequency();
  const uint64_t frame_interval_ticks =
      std::max<uint64_t>(1, static_cast<uint64_t>(host_freq / refresh_hz));

  const uint64_t now = chrono::Clock::QueryHostTickCount();
  const uint64_t last = g_last_guest_frame_host_ticks.load(std::memory_order_relaxed);
  if (last != 0 && now > last) {
    const uint64_t elapsed = now - last;
    if (elapsed < frame_interval_ticks) {
      const uint64_t remain_ticks = frame_interval_ticks - elapsed;
      const uint64_t remain_us = remain_ticks * 1000000 / host_freq;
      if (remain_us > 0) {
        thread::Sleep(std::chrono::microseconds(remain_us));
      }
    }
  }
  const uint64_t paced_now = chrono::Clock::QueryHostTickCount();
  if (last != 0) {
    const double delta_sec = static_cast<double>(paced_now - last) / host_freq;
    g_paced_frame_delta_sec.store(std::clamp(delta_sec, 0.0, kMaxPacedFrameDeltaSec),
                                  std::memory_order_relaxed);
  } else {
    g_paced_frame_delta_sec.store(1.0 / refresh_hz, std::memory_order_relaxed);
  }
  g_last_guest_frame_host_ticks.store(paced_now, std::memory_order_relaxed);
  chrono::Clock::ResetGuestClockHostBaseline();
  const float frame_budget = static_cast<float>(
      g_paced_frame_delta_sec.load(std::memory_order_relaxed) *
      REXCVAR_GET(guest_physics_dt_scale));
  g_physics_dt_budget_remaining.store(std::max(0.0f, frame_budget),
                                      std::memory_order_relaxed);
  g_physics_steps_this_frame.store(0, std::memory_order_relaxed);
  g_physics_window_main_loop_count.fetch_add(1, std::memory_order_relaxed);

  const int32_t log_interval_sec = REXCVAR_GET(timing_log_interval_sec);
  if (log_interval_sec > 0) {
    uint64_t window_start = g_pacing_window_start_ticks.load(std::memory_order_relaxed);
    if (!window_start) {
      g_pacing_window_start_ticks.store(paced_now, std::memory_order_relaxed);
      g_pacing_window_count.store(1, std::memory_order_relaxed);
    } else {
      const uint32_t frames_in_window =
          g_pacing_window_count.fetch_add(1, std::memory_order_relaxed) + 1;
      const uint64_t window_elapsed = paced_now - window_start;
      const uint64_t log_interval_ticks = static_cast<uint64_t>(log_interval_sec) * host_freq;
      if (window_elapsed >= log_interval_ticks) {
        const double window_sec = static_cast<double>(window_elapsed) / host_freq;
        const double measured_fps = window_sec > 0.0 ? frames_in_window / window_sec : 0.0;
        REXKRNL_INFO(
            "Guest frame pacing: {:.1f} loops/s (target {:.1f} Hz), guest refresh {:.2f} Hz, "
            "guest_frame_pacing={}",
            measured_fps, refresh_hz, static_cast<float>(mode.refresh_rate),
            REXCVAR_GET(guest_frame_pacing));
        g_pacing_window_start_ticks.store(paced_now, std::memory_order_relaxed);
        g_pacing_window_count.store(0, std::memory_order_relaxed);
      }
    }
  }
}

REX_EXPORT_STUB(__imp__VdBlockUntilGUIIdle);
REX_EXPORT_STUB(__imp__VdDisplayFatalError);
REX_EXPORT_STUB(__imp__VdEnableClosedCaption);
REX_EXPORT_STUB(__imp__VdEnableDisablePowerSavingMode);
REX_EXPORT_STUB(__imp__VdGenerateGPUCSCCoefficients);
REX_EXPORT_STUB(__imp__VdGetClosedCaptionReadyStatus);
REX_EXPORT_STUB(__imp__VdGetDisplayModeOverride);
REX_EXPORT_STUB(__imp__VdInitializeScaler);
REX_EXPORT_STUB(__imp__VdQuerySystemCommandBuffer);
REX_EXPORT_STUB(__imp__VdReadDVERegisterUlong);
REX_EXPORT_STUB(__imp__VdReadWriteHSIOCalibrationFlag);
REX_EXPORT_STUB(__imp__VdRegisterGraphicsNotification);
REX_EXPORT_STUB(__imp__VdRegisterXamGraphicsNotification);
REX_EXPORT_STUB(__imp__VdSendClosedCaptionData);
REX_EXPORT_STUB(__imp__VdSetCGMSOption);
REX_EXPORT_STUB(__imp__VdSetColorProfileAdjustment);
REX_EXPORT_STUB(__imp__VdSetCscMatricesOverride);
REX_EXPORT_STUB(__imp__VdSetHDCPOption);
REX_EXPORT_STUB(__imp__VdSetMacrovisionOption);
REX_EXPORT_STUB(__imp__VdSetSystemCommandBuffer);
REX_EXPORT_STUB(__imp__VdSetWSSData);
REX_EXPORT_STUB(__imp__VdSetWSSOption);
REX_EXPORT_STUB(__imp__VdTurnDisplayOff);
REX_EXPORT_STUB(__imp__VdTurnDisplayOn);
REX_EXPORT_STUB(__imp__VdWriteDVERegisterUlong);
REX_EXPORT_STUB(__imp__VdInitializeEDRAM);
REX_EXPORT_STUB(__imp__VdReadEEDIDBlock);
REX_EXPORT_STUB(__imp__VdEnumerateVideoModes);
REX_EXPORT_STUB(__imp__VdEnableHDCP);
REX_EXPORT_STUB(__imp__VdRegisterHDCPNotification);
REX_EXPORT_STUB(__imp__VdGetDisplayDiscoveryData);
REX_EXPORT_STUB(__imp__VdStartDisplayDiscovery);
REX_EXPORT_STUB(__imp__VdSetHDCPRevocationList);
REX_EXPORT_STUB(__imp__VdEnableWMAProOverHDMI);
REX_EXPORT_STUB(__imp__VdQueryRealVideoMode);
REX_EXPORT_STUB(__imp__VdSetCGMSState);
REX_EXPORT_STUB(__imp__VdSetSCMSState);
REX_EXPORT_STUB(__imp__VdGetOption);
REX_EXPORT_STUB(__imp__VdSetOption);
REX_EXPORT_STUB(__imp__VdQueryVideoCapabilities);
REX_EXPORT_STUB(__imp__VdGet3dVideoFormat);
REX_EXPORT_STUB(__imp__VdGetWSS2Data);
REX_EXPORT_STUB(__imp__VdSet3dVideoFormat);
REX_EXPORT_STUB(__imp__VdSetWSS2Data);
REX_EXPORT_STUB(__imp__VdSetStudioRGBMode);

// https://web.archive.org/web/20150805074003/https://www.tweakoz.com/orkid/
// http://www.tweakoz.com/orkid/dox/d3/d52/xb360init_8cpp_source.html
// https://github.com/Free60Project/xenosfb/
// https://github.com/Free60Project/xenosfb/blob/master/src/xe.h
// https://github.com/gligli/libxemit
// https://web.archive.org/web/20090428095215/https://msdn.microsoft.com/en-us/library/bb313877.aspx
// https://web.archive.org/web/20100423054747/https://msdn.microsoft.com/en-us/library/bb313961.aspx
// https://web.archive.org/web/20100423054747/https://msdn.microsoft.com/en-us/library/bb313878.aspx
// https://web.archive.org/web/20090510235238/https://msdn.microsoft.com/en-us/library/bb313942.aspx
// https://svn.dd-wrt.com/browser/src/linux/universal/linux-3.8/drivers/gpu/drm/radeon/radeon_ring.c?rev=21595
// https://www.microsoft.com/en-za/download/details.aspx?id=5313 -- "Stripped
// Down Direct3D: Xbox 360 Command Buffer and Resource Management"

void VdGetCurrentDisplayGamma_entry(mapped_u32 type_ptr, mapped_f32 power_ptr) {
  // 1 - sRGB.
  // 2 - TV (BT.709).
  // 3 - use the power written to *power_ptr.
  // Anything else - linear.
  // Used in D3D SetGammaRamp/SetPWLGamma to adjust the ramp for the display.
  *type_ptr = kDisplayGammaType;
  *power_ptr = float(kDisplayGammaPower);
}

struct X_D3DPRIVATE_RECT {
  rex::be<uint32_t> x1;  // 0x0
  rex::be<uint32_t> y1;  // 0x4
  rex::be<uint32_t> x2;  // 0x8
  rex::be<uint32_t> y2;  // 0xC
};
static_assert_size(X_D3DPRIVATE_RECT, 0x10);

struct X_D3DFILTER_PARAMETERS {
  rex::be<float> nyquist;         // 0x0
  rex::be<float> flicker_filter;  // 0x4
  rex::be<float> beta;            // 0x8
};
static_assert_size(X_D3DFILTER_PARAMETERS, 0xC);

struct X_D3DPRIVATE_SCALER_PARAMETERS {
  X_D3DPRIVATE_RECT scaler_source_rect;                 // 0x0
  rex::be<uint32_t> scaled_output_width;                // 0x10
  rex::be<uint32_t> scaled_output_height;               // 0x14
  rex::be<uint32_t> vertical_filter_type;               // 0x18
  X_D3DFILTER_PARAMETERS vertical_filter_parameters;    // 0x1C
  rex::be<uint32_t> horizontal_filter_type;             // 0x28
  X_D3DFILTER_PARAMETERS horizontal_filter_parameters;  // 0x2C
};
static_assert_size(X_D3DPRIVATE_SCALER_PARAMETERS, 0x38);

struct X_DISPLAY_INFO {
  rex::be<uint16_t> front_buffer_width;              // 0x0
  rex::be<uint16_t> front_buffer_height;             // 0x2
  uint8_t front_buffer_color_format;                 // 0x4
  uint8_t front_buffer_pixel_format;                 // 0x5
  X_D3DPRIVATE_SCALER_PARAMETERS scaler_parameters;  // 0x8
  rex::be<uint16_t> display_window_overscan_left;    // 0x40
  rex::be<uint16_t> display_window_overscan_top;     // 0x42
  rex::be<uint16_t> display_window_overscan_right;   // 0x44
  rex::be<uint16_t> display_window_overscan_bottom;  // 0x46
  rex::be<uint16_t> display_width;                   // 0x48
  rex::be<uint16_t> display_height;                  // 0x4A
  rex::be<float> display_refresh_rate;               // 0x4C
  rex::be<uint32_t> display_interlaced;              // 0x50
  uint8_t display_color_format;                      // 0x54
  rex::be<uint16_t> actual_display_width;            // 0x56
};
static_assert_size(X_DISPLAY_INFO, 0x58);

void VdGetCurrentDisplayInformation_entry(ppc_ptr_t<X_DISPLAY_INFO> display_info) {
  X_VIDEO_MODE mode;
  VdQueryVideoMode(&mode);
  display_info.Zero();
  display_info->front_buffer_width = (uint16_t)mode.display_width;
  display_info->front_buffer_height = (uint16_t)mode.display_height;

  display_info->scaler_parameters.scaler_source_rect.x2 = mode.display_width;
  display_info->scaler_parameters.scaler_source_rect.y2 = mode.display_height;
  display_info->scaler_parameters.scaled_output_width = mode.display_width;
  display_info->scaler_parameters.scaled_output_height = mode.display_height;
  display_info->scaler_parameters.horizontal_filter_type = 1;
  display_info->scaler_parameters.vertical_filter_type = 1;

  uint16_t overscan_x = uint16_t(uint32_t(mode.display_width) / 4);
  uint16_t overscan_y = uint16_t(uint32_t(mode.display_height) / 4);
  display_info->display_window_overscan_left = overscan_x;
  display_info->display_window_overscan_top = overscan_y;
  display_info->display_window_overscan_right = overscan_x;
  display_info->display_window_overscan_bottom = overscan_y;
  display_info->display_width = (uint16_t)mode.display_width;
  display_info->display_height = (uint16_t)mode.display_height;
  display_info->display_refresh_rate = mode.refresh_rate;
  display_info->actual_display_width = (uint16_t)mode.display_width;
}

void VdQueryVideoMode(X_VIDEO_MODE* video_mode) {
  // Exposed as CVARs so the guest can observe custom display settings.
  uint32_t display_width = GetConfiguredVideoModeWidth();
  uint32_t display_height = GetConfiguredVideoModeHeight();
  float refresh_rate_hz = GetConfiguredVideoModeRefreshRate();

  std::memset(video_mode, 0, sizeof(X_VIDEO_MODE));
  video_mode->display_width = display_width;
  video_mode->display_height = display_height;
  video_mode->is_interlaced = 0;
  video_mode->is_widescreen = display_width * 3 >= display_height * 4;
  video_mode->is_hi_def = display_width >= 1280 || display_height >= 720;
  video_mode->refresh_rate = refresh_rate_hz;
  video_mode->video_standard = 1;  // NTSC
  video_mode->unknown_0x8a = 0x4A;
  video_mode->unknown_0x01 = 0x01;
}

void VdQueryVideoMode_entry(ppc_ptr_t<X_VIDEO_MODE> video_mode) {
  VdQueryVideoMode(video_mode);
}

u32 VdQueryVideoFlags_entry() {
  X_VIDEO_MODE mode;
  VdQueryVideoMode(&mode);

  uint32_t flags = 0;
  flags |= mode.is_widescreen ? 1 : 0;
  flags |= mode.display_width >= 1024 ? 2 : 0;
  flags |= mode.display_width >= 1920 ? 4 : 0;

  return flags;
}

u32 VdSetDisplayMode_entry(u32 flags) {
  // Often 0x40000000.

  // 0?ccf000 00000000 00000000 000000r0

  // r: 0x00000002 |     1
  // f: 0x08000000 |    27
  // c: 0x30000000 | 28-29
  // ?: 0x40000000 |    30

  // r: 1 = Resolution is 720x480 or 720x576
  // f: 1 = Texture format is k_2_10_10_10 or k_2_10_10_10_AS_16_16_16_16
  // c: Color space (0 = RGB, 1 = ?, 2 = ?)
  // ?: (always set?)

  return 0;
}

u32 VdSetDisplayModeOverride_entry(u32 unk0, u32 unk1, f64 refresh_rate, u32 unk3, u32 unk4) {
  // refresh_rate = 0, 50, 59.9, etc.
  return 0;
}

u32 VdInitializeEngines_entry(u32 unk0, u32 callback, mapped_void arg, mapped_u32 pfp_ptr,
                              mapped_u32 me_ptr) {
  // r3 = 0x4F810000
  // r4 = function ptr (cleanup callback?)
  // r5 = function arg
  // r6 = PFP Microcode
  // r7 = ME Microcode
  return 1;
}

void VdShutdownEngines_entry() {
  // Ignored for now.
  // Games seem to call an Initialize/Shutdown pair to query info, then
  // re-initialize.
}

u32 VdGetGraphicsAsicID_entry() {
  // Games compare for < 0x10 and do VdInitializeEDRAM, else other
  // (retrain/etc).
  return 0x11;
}

u32 VdEnableDisableClockGating_entry(u32 enabled) {
  // Ignored, as it really doesn't matter.
  return 0;
}

void VdSetGraphicsInterruptCallback_entry(u32 callback, mapped_void user_data) {
  // callback takes 2 params
  // r3 = bool 0/1 - 0 is normal interrupt, 1 is some acquire/lock mumble
  // r4 = user_data (r4 of VdSetGraphicsInterruptCallback)
  auto* graphics_system = REX_KERNEL_STATE()->emulator()->graphics_system();
  if (!graphics_system) {
    static std::atomic<bool> warned{false};
    WarnNoGpuEmulation("VdSetGraphicsInterruptCallback", warned);
    return;
  }
  graphics_system->SetInterruptCallback(callback, user_data.guest_address());
}

void VdInitializeRingBuffer_entry(mapped_void ptr, i32 size_log2) {
  // r3 = result of MmGetPhysicalAddress
  // r4 = log2(size)
  // Buffer pointers are from MmAllocatePhysicalMemory with WRITE_COMBINE.
  auto* graphics_system = REX_KERNEL_STATE()->emulator()->graphics_system();
  if (!graphics_system) {
    static std::atomic<bool> warned{false};
    WarnNoGpuEmulation("VdInitializeRingBuffer", warned);
    return;
  }
  graphics_system->InitializeRingBuffer(ptr.guest_address(), size_log2);
}

void VdEnableRingBufferRPtrWriteBack_entry(mapped_void ptr, i32 block_size_log2) {
  // r4 = log2(block size), 6, usually --- <=19
  auto* graphics_system = REX_KERNEL_STATE()->emulator()->graphics_system();
  if (!graphics_system) {
    static std::atomic<bool> warned{false};
    WarnNoGpuEmulation("VdEnableRingBufferRPtrWriteBack", warned);
    return;
  }
  graphics_system->EnableReadPointerWriteBack(ptr.guest_address(), block_size_log2);
}

void VdGetSystemCommandBuffer_entry(mapped_void p0_ptr, mapped_void p1_ptr) {
  p0_ptr.Zero(0x94);
  memory::store_and_swap<uint32_t>(p0_ptr, 0xBEEF0000);
  memory::store_and_swap<uint32_t>(p1_ptr, 0xBEEF0001);
}

void VdSetSystemCommandBufferGpuIdentifierAddress_entry(mapped_void unk) {
  // r3 = 0x2B10(d3d?) + 8
}

// VdVerifyMEInitCommand
// r3
// r4 = 19
// no op?

u32 VdInitializeScalerCommandBuffer_entry(
    u32 scaler_source_xy,                                      // ((uint16_t)y << 16) | (uint16_t)x
    u32 scaler_source_wh,                                      // ((uint16_t)h << 16) | (uint16_t)w
    u32 scaled_output_xy,                                      // ((uint16_t)y << 16) | (uint16_t)x
    u32 scaled_output_wh,                                      // ((uint16_t)h << 16) | (uint16_t)w
    u32 front_buffer_wh,                                       // ((uint16_t)h << 16) | (uint16_t)w
    u32 vertical_filter_type,                                  // 7?
    ppc_ptr_t<X_D3DFILTER_PARAMETERS> vertical_filter_params,  //
    u32 horizontal_filter_type,                                // 7?
    ppc_ptr_t<X_D3DFILTER_PARAMETERS> horizontal_filter_params,  //
    mapped_void unk9,                                            //
    mapped_void dest_ptr,  // Points to the first 80000000h where the memcpy
                           // sources from.
    u32 dest_count         // Count in words.
) {
  // We could fake the commands here, but I'm not sure the game checks for
  // anything but success (non-zero ret).
  // For now, we just fill it with NOPs.
  auto dest = dest_ptr.as_array<uint32_t>();
  for (size_t i = 0; i < dest_count; ++i) {
    dest[i] = 0x80000000;
  }
  return (uint32_t)dest_count;
}

struct BufferScaling {
  rex::be<uint16_t> fb_width;
  rex::be<uint16_t> fb_height;
  rex::be<uint16_t> bb_width;
  rex::be<uint16_t> bb_height;
};
void AppendParam(string::StringBuffer* string_buffer, ppc_ptr_t<BufferScaling> param) {
  string_buffer->AppendFormat("{:08X}(scale {}x{} -> {}x{}))", param.guest_address(),
                              uint16_t(param->bb_width), uint16_t(param->bb_height),
                              uint16_t(param->fb_width), uint16_t(param->fb_height));
}

u32 VdCallGraphicsNotificationRoutines_entry(u32 unk0, ppc_ptr_t<BufferScaling> args_ptr) {
  assert_true(unk0 == 1);

  // TODO(benvanik): what does this mean, I forget:
  // callbacks get 0, r3, r4

  return 0;
}

u32 VdIsHSIOTrainingSucceeded_entry() {
  // BOOL return value
  return 1;
}

u32 VdPersistDisplay_entry(u32 unk0, mapped_u32 unk1_ptr) {
  // unk1_ptr needs to be populated with a pointer passed to
  // MmFreePhysicalMemory(1, *unk1_ptr).
  if (unk1_ptr) {
    auto heap = REX_KERNEL_MEMORY()->LookupHeapByType(true, 16 * 1024);
    uint32_t unk1_value;
    heap->Alloc(64, 32, memory::kMemoryAllocationReserve | memory::kMemoryAllocationCommit,
                memory::kMemoryProtectNoAccess, false, &unk1_value);
    *unk1_ptr = unk1_value;
  }

  return 1;
}

u32 VdRetrainEDRAMWorker_entry(u32 unk0) {
  return 0;
}

u32 VdRetrainEDRAM_entry(u32 unk0, u32 unk1, u32 unk2, u32 unk3, u32 unk4, u32 unk5) {
  return 0;
}

void VdSwap_entry(mapped_void buffer_ptr,      // ptr into primary ringbuffer
                  mapped_void fetch_ptr,       // frontbuffer Direct3D 9 texture header fetch
                  mapped_void unk2,            // system writeback ptr
                  mapped_void unk3,            // buffer from VdGetSystemCommandBuffer
                  mapped_void unk4,            // from VdGetSystemCommandBuffer (0xBEEF0001)
                  mapped_u32 frontbuffer_ptr,  // ptr to frontbuffer address
                  mapped_u32 texture_format_ptr, mapped_u32 color_space_ptr, mapped_u32 width,
                  mapped_u32 height) {
  // All of these parameters are REQUIRED.
  assert(buffer_ptr);
  assert(fetch_ptr);
  assert(frontbuffer_ptr);
  assert(texture_format_ptr);
  assert(width);
  assert(height);

  namespace xenos = rex::graphics::xenos;

  xenos::xe_gpu_texture_fetch_t gpu_fetch;
  memory::copy_and_swap_32_unaligned(&gpu_fetch,
                                     reinterpret_cast<uint32_t*>(fetch_ptr.host_address()), 6);

  // The fetch constant passed is not a true GPU fetch constant, but rather, the
  // fetch constant stored in the Direct3D 9 texture header, which contains the
  // address in one of the virtual mappings of the physical memory rather than
  // the physical address itself. We're emulating swapping in the GPU subsystem,
  // which works with GPU memory addresses (physical addresses directly) from
  // proper fetch constants like ones used to bind textures to shaders, not CPU
  // MMU addresses, so translation from virtual to physical is needed.
  uint32_t frontbuffer_virtual_address = gpu_fetch.base_address << 12;
  assert_true(*frontbuffer_ptr == frontbuffer_virtual_address);
  uint32_t frontbuffer_physical_address =
      REX_KERNEL_MEMORY()->GetPhysicalAddress(frontbuffer_virtual_address);
  assert_true(frontbuffer_physical_address != UINT32_MAX);
  if (frontbuffer_physical_address == UINT32_MAX) {
    // Xenia-specific safety check.
    REXKRNL_ERROR("VdSwap: Invalid front buffer virtual address 0x{:08X}",
                  frontbuffer_virtual_address);
    return;
  }
  gpu_fetch.base_address = frontbuffer_physical_address >> 12;

  auto texture_format = rex::graphics::xenos::TextureFormat(texture_format_ptr.value());
  auto color_space = *color_space_ptr;
  assert_true(texture_format == rex::graphics::xenos::TextureFormat::k_8_8_8_8 ||
              texture_format == rex::graphics::xenos::TextureFormat::k_2_10_10_10_AS_16_16_16_16);
  assert_true(color_space == 0);  // RGB(0)
  assert_true(*width == 1 + gpu_fetch.size_2d.width);
  assert_true(*height == 1 + gpu_fetch.size_2d.height);

  // The caller seems to reserve 64 words (256b) in the primary ringbuffer
  // for this method to do what it needs. We just zero them out and send a
  // token value. It'd be nice to figure out what this is really doing so
  // that we could simulate it, though due to TCR I bet all games need to
  // use this method.
  buffer_ptr.Zero(64 * 4);

  uint32_t offset = 0;
  auto dwords = buffer_ptr.as_array<uint32_t>();

  // Write in the GPU texture fetch.
  dwords[offset++] =
      xenos::MakePacketType0(rex::graphics::XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0, 6);
  dwords[offset++] = gpu_fetch.dword_0;
  dwords[offset++] = gpu_fetch.dword_1;
  dwords[offset++] = gpu_fetch.dword_2;
  dwords[offset++] = gpu_fetch.dword_3;
  dwords[offset++] = gpu_fetch.dword_4;
  dwords[offset++] = gpu_fetch.dword_5;

  dwords[offset++] = xenos::MakePacketType3(xenos::PM4_XE_SWAP, 4);
  dwords[offset++] = rex::graphics::xenos::kSwapSignature;
  dwords[offset++] = frontbuffer_physical_address;

  dwords[offset++] = *width;
  dwords[offset++] = *height;

  // Fill the rest of the buffer with NOP packets.
  for (uint32_t i = offset; i < 64; i++) {
    dwords[i] = xenos::MakePacketType2();
  }
}

void RegisterVideoExports(rex::runtime::ExportResolver* export_resolver,
                          KernelState* kernel_state) {
  auto memory = kernel_state->memory();

  // VdGlobalDevice (4b)
  // Pointer to a global D3D device. Games only seem to set this, so we don't
  // have to do anything. We may want to read it back later, though.
  uint32_t pVdGlobalDevice = memory->SystemHeapAlloc(4, 32, memory::kSystemHeapPhysical);
  export_resolver->SetVariableMapping("xboxkrnl.exe", 0x01BE, pVdGlobalDevice);
  memory::store_and_swap<uint32_t>(memory->TranslateVirtual(pVdGlobalDevice), 0);

  // VdGlobalXamDevice (4b)
  // Pointer to the XAM D3D device, which we don't have.
  uint32_t pVdGlobalXamDevice = memory->SystemHeapAlloc(4, 32, memory::kSystemHeapPhysical);
  export_resolver->SetVariableMapping("xboxkrnl.exe", 0x01BF, pVdGlobalXamDevice);
  memory::store_and_swap<uint32_t>(memory->TranslateVirtual(pVdGlobalXamDevice), 0);

  // VdGpuClockInMHz (4b)
  // GPU clock. Xenos is 500MHz. Hope nothing is relying on this timing...
  uint32_t pVdGpuClockInMHz = memory->SystemHeapAlloc(4, 32, memory::kSystemHeapPhysical);
  export_resolver->SetVariableMapping("xboxkrnl.exe", 0x01C0, pVdGpuClockInMHz);
  memory::store_and_swap<uint32_t>(memory->TranslateVirtual(pVdGpuClockInMHz), 500);

  // VdHSIOCalibrationLock (28b)
  // CriticalSection.
  uint32_t pVdHSIOCalibrationLock = memory->SystemHeapAlloc(28, 32, memory::kSystemHeapPhysical);
  export_resolver->SetVariableMapping("xboxkrnl.exe", 0x01C1, pVdHSIOCalibrationLock);
  auto hsio_lock = memory->TranslateVirtual<X_RTL_CRITICAL_SECTION*>(pVdHSIOCalibrationLock);
  xeRtlInitializeCriticalSectionAndSpinCount(hsio_lock, pVdHSIOCalibrationLock, 10000);
}

}  // namespace rex::kernel::xboxkrnl

REX_EXPORT(__imp__VdGetCurrentDisplayGamma, rex::kernel::xboxkrnl::VdGetCurrentDisplayGamma_entry)
REX_EXPORT(__imp__VdGetCurrentDisplayInformation,
           rex::kernel::xboxkrnl::VdGetCurrentDisplayInformation_entry)
REX_EXPORT(__imp__VdQueryVideoMode, rex::kernel::xboxkrnl::VdQueryVideoMode_entry)
REX_EXPORT(__imp__VdQueryVideoFlags, rex::kernel::xboxkrnl::VdQueryVideoFlags_entry)
REX_EXPORT(__imp__VdSetDisplayMode, rex::kernel::xboxkrnl::VdSetDisplayMode_entry)
REX_EXPORT(__imp__VdSetDisplayModeOverride, rex::kernel::xboxkrnl::VdSetDisplayModeOverride_entry)
REX_EXPORT(__imp__VdInitializeEngines, rex::kernel::xboxkrnl::VdInitializeEngines_entry)
REX_EXPORT(__imp__VdShutdownEngines, rex::kernel::xboxkrnl::VdShutdownEngines_entry)
REX_EXPORT(__imp__VdGetGraphicsAsicID, rex::kernel::xboxkrnl::VdGetGraphicsAsicID_entry)
REX_EXPORT(__imp__VdEnableDisableClockGating,
           rex::kernel::xboxkrnl::VdEnableDisableClockGating_entry)
REX_EXPORT(__imp__VdSetGraphicsInterruptCallback,
           rex::kernel::xboxkrnl::VdSetGraphicsInterruptCallback_entry)
REX_EXPORT(__imp__VdInitializeRingBuffer, rex::kernel::xboxkrnl::VdInitializeRingBuffer_entry)
REX_EXPORT(__imp__VdEnableRingBufferRPtrWriteBack,
           rex::kernel::xboxkrnl::VdEnableRingBufferRPtrWriteBack_entry)
REX_EXPORT(__imp__VdGetSystemCommandBuffer, rex::kernel::xboxkrnl::VdGetSystemCommandBuffer_entry)
REX_EXPORT(__imp__VdSetSystemCommandBufferGpuIdentifierAddress,
           rex::kernel::xboxkrnl::VdSetSystemCommandBufferGpuIdentifierAddress_entry)
REX_EXPORT(__imp__VdInitializeScalerCommandBuffer,
           rex::kernel::xboxkrnl::VdInitializeScalerCommandBuffer_entry)
REX_EXPORT(__imp__VdCallGraphicsNotificationRoutines,
           rex::kernel::xboxkrnl::VdCallGraphicsNotificationRoutines_entry)
REX_EXPORT(__imp__VdIsHSIOTrainingSucceeded, rex::kernel::xboxkrnl::VdIsHSIOTrainingSucceeded_entry)
REX_EXPORT(__imp__VdPersistDisplay, rex::kernel::xboxkrnl::VdPersistDisplay_entry)
REX_EXPORT(__imp__VdRetrainEDRAMWorker, rex::kernel::xboxkrnl::VdRetrainEDRAMWorker_entry)
REX_EXPORT(__imp__VdRetrainEDRAM, rex::kernel::xboxkrnl::VdRetrainEDRAM_entry)
REX_EXPORT(__imp__VdSwap, rex::kernel::xboxkrnl::VdSwap_entry)
