# Final evidence status — Android port performance investigation

**Date:** 2026-11-17  
**Workspace:** `C:\S4EP1Recomp`  
**Scope:** native Windows x64, POCO F7 ARM64, and emulator validation for Sonic 4 Episode I recomp performance.  
**Changes:** evidence scripts, reports, and tools only. No retail/engine optimization, pipeline-cache restoration, or gameplay/save behavior change was made.

---

## 1. Bottom line

- A matched-resolution, retail-to-retail comparison was completed at **2772x1280**.
- Windows x64 retail and POCO F7 ARM64 retail both sustain **59.2 Hz** in the measured light gameplay scenario.
- POCO heavy/mixed sessions measured **37.6 Hz** and **41.6 Hz** overall, with majorities of 5 s windows below 45 Hz.
- The heavy-scene gap is therefore **workload-dependent**, not a fixed ARM64 60 Hz ceiling.
- GPU execution is not the primary limiter in the available timestamp data; the remaining attribution is **guest ARM64 recomp execution + Xenos→Vulkan CPU work**, with driver/scheduling as an unquantified modifier.
- No emulator gameplay benchmark is valid in this environment.

---

## 2. Verified primary measurements

Re-generated with `tools/compare_benchmarks.py` from the retained raw logs:

| Run | Swap | PACE n | PACE avg | Heavy windows | `frame_us` avg/p95 | SCENEDISP avg/max |
| --- | --- | ---: | ---: | --- | ---: | ---: |
| Windows probe `bench2` | 2772x1280 | 39 | 59.5 Hz | 0 | 16318 / 16718 | 740.1 / 1350.8 |
| Windows retail `bench2` (clean) | 2772x1280 | 30 | 59.2 Hz | 0 | 16560 / 16627 | n/a |
| POCO `bench4` light gameplay | 2772x1280 | 41 | 59.2 Hz | 0 | 14804 / 15690 | n/a |
| POCO `sonic4_038` heavy/mixed | 2772x1280 | 83 | 37.6 Hz | 65 @ 33.3 Hz | 23524 / 30739 | n/a |
| POCO `sonic4_039` heavy/mixed | 2772x1280 | 51 | 41.6 Hz | 32 @ 36.2 Hz | 21391 / 28905 | n/a |

Heavy is a post-hoc telemetry bin (`PACE < 45 Hz`), not a controlled map region. Retail logs do not emit `SCENEDISP`, so exact scene identity is unavailable.

---

## 3. Artifact identity

| Artifact | SHA-256 |
| --- | --- |
| `android/dist/Sonic4-arm64-v8a.apk` | `7cfa543e3135f0c98b0edadbb4f5e000a79f4275f842a193a95d1a129c81ec4a` |
| Extracted `lib/main.arm64-v8a/libmain.so` | `2da2f5b0da9e6558819e9206cb934fedc266442cd2849782d7826a20784f0382` |

Both hashes were rechecked on 2026-11-17 and match the earlier records.

---

## 4. Report index

| Report | Purpose |
| --- | --- |
| `COMPARATIVE_BENCHMARK_REPORT.md` | Primary measured comparison and ranked performance attribution |
| `BENCHMARK_METHODOLOGY.md` | Repeatable Windows/POCO procedures, input map, metrics, equivalence limits |
| `EMULATOR_VALIDATION_REPORT.md` | Official SDK/AVD setup, ARM64 boot refusal, x86_64/Berberis crash |
| `ARM64_CODEGEN_CORRELATION_REPORT.md` | Static ARM64 disassembly correlated with generated guest-memory code |
| `ROOT_CAUSE_PERFORMANCE_INVESTIGATION.md` | Earlier diag/probe A/B, GPU timestamps, ranked causes |
| `VULKAN_CPU_RECORDING_INVESTIGATION.md` | `IssueDraw`, replay, descriptor-update decomposition |
| `ISSUE_DRAW_ROOT_CAUSE.md` | Vertex-fetch and primitive sub-cost analysis |
| `GRAPHICS_REGRESSION_INVESTIGATION.md` | Reverted `PipelineDescription` cache and validation limits |

---

## 5. Checklist against the original constraints

| Question / requirement | Status | Evidence |
| --- | --- | --- |
| Install/configure Android Studio, SDK, Platform Tools, Emulator, ARM64 image from official sources | Done | `EMULATOR_VALIDATION_REPORT.md`, `raw/sdk_setup/`, `raw/emulator/` |
| Do not assume x86_64 emulator runs ARM64 natively | Documented | ARM64 refusal + Berberis translation stack in emulator report |
| Preserve POCO installation/save data | Done | No uninstall/wipe; shared POCO save snapshot reused for Windows runs |
| Repeatable main-menu/level-load/gameplay benchmarks | Done with documented limits | `scripts/run-win-scenario.ps1`, `scripts/run-poco-scenario.ps1`, methodology |
| Investigate ARM64 codegen without blind rewrite | Done | Static disassembly/generated-source correlation report |
| Separate guest/CPU, Vulkan, GPU, and pacing | Partially quantified | Diag timestamps and code analysis; per-thread heavy-window trace remains unresolved |
| Keep diagnostics out of retail builds | Done | Primary binaries have `REX_DIAG_PROFILE=OFF`, `REX_TIMING_PROBES=OFF` |
| No optimization or pipeline-cache reintroduction | Done | Scope-control sections in all final reports |
| Emulator gameplay verified before claiming it works | Not claimed | Game never reached valid gameplay; explicitly excluded |

---

## 6. Remaining unresolved work

1. Paired diag A/B on Windows and POCO with the same save/scene, collecting `SCENEDISP`, `draws_per_present`, `record_us_avg`, and GPU timestamps in matching windows.
2. Per-thread POCO traces during heavy windows, separating Main/XThread, GPU Commands, Audio Worker, and SDLThread.
3. Only in a non-retail experiment branch, test ARM64 codegen memory/byteswap changes with measured A/B before considering any merge.
4. Optional native ARM64 AVD benchmark on an actual ARM64 host; the x86_64 fallback is invalid for this attribution.

These are follow-ups, not achieved measurements. No theoretical FPS value is inferred from them.
