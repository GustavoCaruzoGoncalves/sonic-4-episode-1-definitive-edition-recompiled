# Exp-011 — classic VkRenderPass instead of dynamic rendering on Adreno

**Date:** 2026-11-17 · **Device:** POCO F7 (`192.168.70.7:36067`, ADB port changed mid-session)
**Build:** telemetry ON, batch residency ON, tex reuse ON, Exp-008..010 ON (same APK both arms)

## Hypothesis
GPU Commands thread profile (DWARF, 10 s, after Exp-008..010): `vkCmdBeginRendering` inside
`vulkan.adreno.so` ≈ **29 %** of the thread; the driver `calloc`s large blocks per begin
(scudo secondary → `mmap`/`munmap` + page faults ≈ 14–20 %), `vkResetCommandPool` ≈ 17 %.
The guest frame has ~85 render-target begins per present (~4.8k/s). Classic render pass
objects (pre-built `VkRenderPass` + `VkFramebuffer`, already supported by the render target
cache) should avoid the per-begin driver setup.

## Change
`vulkan_dynamic_rendering` default → **false on Android** (`command_processor.cpp`). Desktop
unchanged. Runtime A/B via `setprop debug.rex.cvars vulkan_dynamic_rendering=true|false`.

## Result — controlled, same session, ~2 min apart, both 189.0 draws/present, stationary

| Metric | Dynamic (`exp011_dynamic_rp_still`) | Classic (`exp011_classic_rp_still`) |
|---|---:|---:|
| Process CPU | 2.49 cores | **1.68 cores** (−32 %) |
| GPU Commands thread | 0.660 | 0.419 |
| Guest GPU-fence spin thread | 0.782 | 0.255 |
| `record_us_pf` | 5475–5507 µs | **1431–1505 µs** (−73 %) |
| `issue_draw_us_pf` | 3319–3490 µs | 2704–2754 µs |
| `submit_us_pf` | ~1.9 ms | 2.1–2.5 ms |
| PACE | 57.9–58.0 Hz | **59.7 Hz** |
| GPU busy / freq (spot) | 74 % @ 873 MHz | 78 % @ 937 MHz |

Visual: `raw/threads/exp011_classic_rp_still/screen.png` — Splash Hill start, HUD, foliage, sky,
Sonic idle pose all correct. Earlier uncontrolled run (`exp011_classic_rp`, player moving) also
showed no corruption.

**Verdict:** SUCCESS (CPU). GPU busy did not drop (slightly higher because the game now presents
~60 Hz); the render-pass *count* is unchanged, so GPU-side tile load/store cost of ~85 passes
per frame remains — next target is eliminating unnecessary render-pass breaks.

**Session note:** the dynamic arm ran at 58 Hz here vs ~44 Hz one hour earlier (Exp-008..010
runs) — device thermal/DVFS state differs between sessions; only same-session pairs compare.

Risks: classic path depends on render-pass/framebuffer cache invalidation in
`VulkanRenderTargetCache`; only first stage was exercised.
