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

#pragma once

#include <rex/system/xtypes.h>
#include <rex/system/xvideo.h>

namespace rex::runtime {
class ExportResolver;
}

namespace rex::system {
class KernelState;
}

namespace rex::kernel::xboxkrnl {

void VdQueryVideoMode(system::X_VIDEO_MODE* video_mode);

// Host-side frame pacing for guest main loops (call once at the start of each loop).
void PaceGuestMainLoopFrame();

// Wall-clock delta between the last two paced main-loop iterations (seconds).
float GuestGetPacedFrameDeltaSeconds();

// Clamp physics delta (seconds or frame ticks; see Clock cvars).
float GuestClampPhysicsDeltaSeconds(float delta_seconds);

// Scene/render update delta (sub_821C1C80 f1).
void GuestSanitizePrimaryFrameDelta(float& delta_seconds);

// Clamp f1 only when above guest_max_physics_dt (does not replace small deltas).
void GuestCapRunawayFrameDelta(float& delta_seconds);

// Overwrite guest float(s) with the current paced frame delta (title-specific VAs).
void GuestWriteFrameDeltaToGuestVAs(const uint32_t* guest_virtual_addresses, size_t count,
                                    float delta_seconds);
void GuestPublishPacedFrameDeltaTo(const uint32_t* guest_virtual_addresses, size_t count);

// Apply clamp to f1–f6 before sub_821B35C8; logs when timing_log_interval_sec > 0.
void GuestSanitizePhysicsStepInputs(float& f1, float& f2, float& f3, float& f4, float& f5,
                                    float& f6);

// Register video variable exports (VdGlobalDevice, VdHSIOCalibrationLock, etc.)
// Must be called during kernel initialization before XEX modules are loaded.
void RegisterVideoExports(rex::runtime::ExportResolver* export_resolver,
                          system::KernelState* kernel_state);

}  // namespace rex::kernel::xboxkrnl
