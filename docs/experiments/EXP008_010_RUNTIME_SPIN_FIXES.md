# Exp-008 / 009 / 010 — runtime threads burning cores while the game is idle-ish

**Date:** 2026-11-17 · **Device:** POCO F7 (`192.168.70.7:45147`) · **Scene:** first stage, Sonic stationary

## Discovery (baseline, old build, telemetry ON)

`simpleperf record -p <pid> -f 4000 --call-graph fp` 20 s + `/proc/<pid>/task/*/schedstat`:

| Thread | Share of process samples | What it was doing |
|---|---:|---|
| `XThreadE61F2F80` (game) | 19.4 % | guest spin in `sub_821C62C8`/`sub_821C4870` waiting for GPU ring read pointer |
| `GPU Commands` | 16.5 % | PM4 → Vulkan translation (the real bottleneck) |
| `GPU VSync` | 15.8 % | **vblank interrupt storm** (Exp-009) |
| `Audio Worker` | 14.8 % | **busy-spinning `WaitMultiple`** (Exp-010) |
| `SDLThread` (UI/present) | 14.7 % | **~90 % inside Adreno `libllvm-qgl.so` recompiling the present pipeline every frame** (Exp-008) |
| `Main XThread` | 14.0 % | guest + futex contention on the global critical region |

Process total ≈ **5 busy cores** for a stationary 2D-ish scene.

## Root causes

### Exp-008 — presenter recompiles its swapchain pipeline on every paint
`VulkanPresenter::PaintAndPresentImpl` compares `GuestOutputPaintPipeline::swapchain_format`
against the render-pass format, but the field is **never assigned** (same latent bug in upstream
Xenia). Every paint therefore: awaited the previous paint submission (CPU↔GPU sync), destroyed the
pipeline, and called `vkCreateGraphicsPipelines` → full Adreno LLVM compile.
Fix: assign the format after successful creation. Cvar `present_pso_reuse` (default true).

### Exp-009 — vblank interrupt storm
`PaceGuestMainLoopFrame()` calls `Clock::ResetGuestClockHostBaseline()` every guest frame; it
moves only `anchor_host`, so the guest timebase jumps **back** to a constant. The VSync worker's
`while (current - last >= interval)` then underflows (uint64) and calls `MarkVblank()` back to
back for ~2^64/interval iterations, i.e. forever. Each call runs the guest ISR under
`ExecuteInterrupt`, which holds the **process-global critical region** — contended by every
other thread (futex storms, ~3.8k voluntary cs/s on the VSync thread).
Fix: time vblanks with the monotonic host clock at the video refresh rate, bounded catch-up
(≤3 back-to-back). Cvar `vblank_host_clock` (default true). This is also the hardware-faithful
semantic (vblank is a real-time display signal).

### Exp-010 — `PosixConditionBase::WaitMultiple` busy-spins
Alertable multi-object waits are sliced into 1 ms waits; inside the polling loop
`duration_cast<milliseconds>(remaining)` truncates to 0 → `sleep_for(0)` → spin (with a heap
allocation per iteration). Fix: blocking wait on a process-wide generation counter + condvar
bumped by every signal path (Event/Semaphore/Mutant/Timer/thread exit); exact deadline; alertable
slices rounded up. Cvar `wait_multi_block` (default true).

### A/B mechanism
`adb shell setprop debug.rex.cvars "name=value,..."` is read at startup on Android
(`rex_app.cpp` → `cvar::ApplyOverrideList`). Restart the app after changing it.

### Follow-up: vblank rate (`vblank_host_hz`)
First ON run used a display-faithful 60 Hz vblank: storm gone, but the guest became
vsync-quantized to **~29–30 Hz** (frames >16.7 ms wait two vblanks). The legacy storm meant
"flips never wait"; the project's 60 Hz limit actually comes from `PaceGuestMainLoopFrame`.
`vblank_host_hz` (default **1000**, like Xenia's no-vsync vblank interval) keeps that observable
pacing at ~1000 cheap ISRs/s; `0` = video-mode refresh, for when frames fit in 16.7 ms.

## Results (same build, `REX_RENDER_TELEMETRY=ON`, batch residency ON, tex reuse ON)

Matched workload: first stage, stationary, 189 draws/present. 20 s windows
(`scripts/measure_threads.sh`, raw in `raw/threads/<label>/`).

| Metric | OFF `exp008_010_off` (setprop all false) | ON `exp008_010_on_b` |
|---|---:|---:|
| **Process CPU (cores)** | **4.30** | **2.57** (−40 %) |
| GPU VSync thread | 0.674 | 0.032 |
| SDLThread (present) | 0.641 | 0.076 |
| Audio Worker | 0.595 | 0.040 |
| GPU Commands | 0.676 | 0.668 |
| Vblank interrupts/s | **~1.3 million** | 1000 |
| PACE (Hz) | 42.8–43.0 | 44.2–45.2 |
| `issue_draw_us_pf` | ~7.0 ms | ~4.9 ms (−30 %, less global-lock contention) |
| `record_us_pf` | ~7.0 ms | ~7.4 ms (within noise / DVFS) |

Controlled: yes (same APK, same scene, same draws/present, consecutive runs, ~2 min apart).
Runtime correctness: game boots, user reached the first stage in every run, guest frames and
presents keep flowing (PACE/RENDER_VK). Audio worker keeps dispatching (1.6k waits/s) but audible
output and visual output were **not** independently verified in this pass.

**Verdict:** SUCCESS for CPU efficiency (−1.7 cores of pure waste). Frame rate essentially
unchanged — the frame is limited by the GPU Commands thread / render-pass churn (next item).

Remaining risks: guest clock is still non-monotonic (`ResetGuestClockHostBaseline` moves only
`anchor_host`); anything else timed off the guest timebase may misbehave. Not changed here.
