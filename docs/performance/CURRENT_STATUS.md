# Performance engineering — current status

**Last consolidated:** 2026-11-17  
**Primary device:** POCO F7 (Adreno 825-class), Vulkan, first-stage workloads unless noted.

This document summarizes **measured** experiments. Numbers are **not** universal guarantees; they
depend on scene, thermal state (DVFS), and build flags.

---

## Executive summary

The Android port moved from “multi-core idle waste” to “playable first stage with remaining spikes”
through a sequence of controlled changes in vendored ReXGlue:

1. **Exp-007** — texture descriptor reuse (~23.5% recording win).
2. **Exp-008–010** — presenter PSO reuse, host vblank timing, blocking `WaitMultiple` (~4.30 → ~2.57
   process cores stationary).
3. **Exp-011** — classic render passes on Android (~5.5 → ~1.45 ms record; ~2.49 → ~1.68 cores).
4. **Exp-015–017** — write-watch maps fix, pipeline storage persistence, dedicated present queue
   (major reduction in >33 ms frames during moving playtests).
5. **Exp-012 / Exp-014** — tried; **disabled by default** (no win or regression).

**Still open:** progressive stutter causes (Exp-018 points at in-flight fence / swapchain depth),
texture reload rate, power/thermals, guest clock semantics, upstream mmio fix (#441) alignment.

---

## Experiment log (production defaults)

| ID | Change | Default | Verdict |
|----|--------|---------|---------|
| 007 | `REX_TEXTURE_DESCRIPTOR_REUSE` | ON | **Keep** (~23.5% record ↓) |
| 008 | `present_pso_reuse` | ON | **Keep** |
| 009 | `vblank_host_clock` + `vblank_host_hz` | ON / 1000 | **Keep** |
| 010 | `wait_multi_block` | ON | **Keep** |
| 011 | `vulkan_dynamic_rendering=false` on Android | ON (classic) | **Keep** |
| 012 | `shm_upload_hoist` | OFF | Failed A/B |
| 013 | GPU timestamp hooks | Telemetry | Diagnostic |
| 014 | `render_area_tight` | OFF | No GPU win |
| 015 | `write_watch_skip_maps_query` | ON | **Keep** |
| 016 | pipeline storage seek + cache save | ON | **Keep** |
| 017 | `vulkan_dedicated_present_queue` | ON | **Keep** |
| 018 | IssueDraw phase + fence timer | Telemetry | In progress |

Full write-ups: [../experiments/](../experiments/).

---

## Key numbers (with conditions)

### Exp-007 — stationary, 189 draws/present, paired builds

- ON `record_us_pf` median **5077 µs** vs OFF **6638 µs** (~23.5% relative to OFF).

### Exp-008–010 — same APK, setprop A/B, stationary

- Process CPU **4.30 → 2.57** cores; vblank interrupts **~1.3M/s → 1000/s**.

### Exp-011 — same session, consecutive runs, stationary

- `record_us_pf` **~5.5 ms → ~1.45 ms**; process CPU **2.49 → 1.68** cores; PACE ~58 → ~60 Hz.

### Exp-012 — hoist ON vs OFF

- Fewer render passes but **higher** process CPU (1.82 vs 1.63 cores) → default OFF.

### Exp-013 — stationary GPU attribution

- ~**12.3 ms** GPU/frame; **~8.1 ms** inside passes; **~3.8 ms** compute between passes
  (resolves/untile).

### Exp-015–017 — moving play ~4–8.5 min

- Write-watch handler **150 ms/s → 9 ms/s**; pipeline compiles during play **58 → 2** (legacy vs fixed).
- Dedicated present queue: frames **>33 ms** **2.40% → 0.73%** (controlled A/B).

### Exp-018 — remaining spikes

- **75** gameplay spikes ~35–43 ms in one run; **20–30 ms `vkWaitForFences`** each; IssueDraw phases
  **< 6 ms** combined → bottleneck is **frame throttle / queue depth / presentation coupling**, not draw
  processing CPU.

---

## Known problems (contributor-facing)

1. **Resource usage & heat** — sustained play still stresses device (not fully characterized).
2. **Progressive stutter** — improved but not eliminated when advancing through Splash Hill.
3. **Host graphics CPU** — IssueDraw and submission paths remain hot.
4. **Texture streaming** — ~200 reloads/s; render-to-texture untile cost ~3.8 ms GPU/frame (Exp-013).
5. **Guest clock** — `ResetGuestClockHostBaseline` semantics may affect non-vblank timers.
6. **Shutdown** — SIGABRT on terminate path.
7. **Windows** — D3D12 not re-tested with all Android-side SDK edits.

---

## Telemetry builds

Release scripts set `REX_RENDER_TELEMETRY=OFF`. For experiments, use `build-android-arm64-telemetry-on.bat`
or enable `REX_RENDER_TELEMETRY` in CMake. Runtime cvars on Android: `debug.rex.cvars` property.

Analysis scripts: `evidence/ANDROID_PORT/POCO_F7_AUTONOMOUS_ENGINEERING/scripts/`.