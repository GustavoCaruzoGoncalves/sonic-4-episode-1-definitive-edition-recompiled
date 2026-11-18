# Exp-013 / 014 — where the GPU time goes (Adreno 825)

**Date:** 2026-11-17 · first stage, stationary, 189 draws/present, ~59.7 Hz, classic render passes

## Exp-013 — GPU pass profiler (telemetry builds only)
No Perfetto GPU render-stage producer on this device (`perfetto --query`: only `android.gpu.memory`),
so timing is in-app: `DeferredCommandBuffer::Execute` takes an optional `ExecuteHook`; the
command processor's `GpuPassProfiler` writes `vkCmdWriteTimestamp(BOTTOM_OF_PIPE)` at the start,
at every render-pass begin/end and at the end of each submission (8 pools × 512 queries), reads
them without waiting after the fence completes, and logs `PROBE GPU_TS` every 5 s.
Segments: render pass (with ordinal within the frame, extent, draws) / between passes split by
content (compute dispatch, transfer, other).

### Result (`sonic4_086.log`, two 5 s windows)
| Per frame | Value |
|---|---:|
| GPU time (sum of segments) | **12.2–12.3 ms** (gpubusy 79 % @ 873 MHz) |
| inside render passes (~77) | **8.1 ms** |
| compute between passes (resolves / texture loads / EDRAM) | **3.8 ms** |
| transfers (uploads) | 0.3 ms |
| submissions | ~7.7 per frame (~460/s) |

Most expensive passes (ordinal within frame, extent, draws, µs/frame):
`#70 1920x1376 d≈68–77 1.7–1.9 ms`, `#10 d=3 1.2 ms`, `#19 d=1 1.1 ms`, then ≤0.33 ms each.
Two passes with 1–3 draws cost >1 ms each — the shape of full-screen work.

## Exp-014 — tight render area (cvar `render_area_tight`)
Hypothesis: every guest render pass used the full host framebuffer (1920×1376) as `renderArea`
(upstream TODO), so the tiler loads/stores tiles nothing draws to. Change: union of the draw
scissors recorded in the pass is patched into the deferred begin command at pass end (pixels
outside the render area are not affected by load/store ops → identical output).

Result (`sonic4_087.log`): areas shrank to 1920×536/544 but **GPU time unchanged** (12.33 ms/frame,
8.25 ms in passes, same top passes). Visual correct (`raw/threads/exp014_area_on/screen.png`).
**Verdict: FAILURE (no effect)** → default **off**, code kept behind the cvar.

Interpretation: the ~536-row bands and 12 resolves/frame match the game's own predicated tiling
(EDRAM bands), so pass cost is real per-band shading/bandwidth, not unused tiles.

## Next leads (by measured size)
1. Compute between passes, 3.8 ms/frame: split by origin (resolve copy, texture load/untile,
   EDRAM dump/transfer) — resolves of each tile band feed textures that are then re-loaded.
2. The single-draw passes at ~1.1 ms: identify the shader/format (gamma RT stored as RGBA16 UNORM,
   2× MSAA) — diagnostic A/B only, quality must not change in the shipped config.
3. ~7.7 submissions per frame (`vulkan_submit_on_primary_buffer_end`).
