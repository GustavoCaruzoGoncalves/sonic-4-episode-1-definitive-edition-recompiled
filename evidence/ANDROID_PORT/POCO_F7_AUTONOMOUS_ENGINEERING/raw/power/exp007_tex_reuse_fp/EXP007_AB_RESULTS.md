# Exp-007 texture descriptor reuse — gated A/B (POCO F7)

**Config matched for both arms:** `REX_RENDER_TELEMETRY=ON`, `REX_BATCH_DRAW_RESIDENCY=OFF`, Release arm64-v8a, first stage stationary, draws/present ≈189.

| Arm | Label | Valid | median `record_us_pf` | `tex_reuse_hit` (probe) | `tex_reuse_miss` |
|-----|--------|-------|----------------------|---------------------------|------------------|
| **ON** | `exp007_tex_reuse_fp` | 4/4 COMPARABLE | **5076.5 µs** | ~19.2k | ~31.8k |
| **OFF** | `exp007_tex_reuse_off` | 4/4 COMPARABLE | **6638 µs** | **0** | ~41.6k |

**Delta (OFF − ON):** ~1562 µs (~23.5% of OFF `record_us_pf`). Causal: OFF build shows zero reuse hits.

**Visual (ON capture):** `first_stage_screenshot.png` — Splash Hill, HUD/rings/score sane; no obvious texture corruption (see agent review).

**CMake toggle:** `-DREX_TEXTURE_DESCRIPTOR_REUSE=ON|OFF` (default ON). OFF script: `build-android-arm64-exp007-noreuse.bat`.

**Decision:** Keep optimization; do not treat ~7.2 ms historical baseline as control — use paired OFF median.