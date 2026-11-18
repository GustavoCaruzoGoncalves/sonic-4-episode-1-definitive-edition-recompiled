# Comparative performance report — Windows x64 vs POCO F7 ARM64

**Date:** 2026-11-17  
**Resolution:** 2772x1280 on both native platforms for primary comparisons  
**Builds:** retail telemetry on both sides (`REX_DIAG_PROFILE=OFF`, `REX_TIMING_PROBES=OFF`); Windows probe used only for workload decomposition

---

## 1. Executive summary

The same retail APK reaches **59.2 Hz** on POCO F7 in a light, stable gameplay scene — identical to the matched Windows x64 retail result of **59.2 Hz**. Earlier native POCO sessions with heavier gameplay sustained only **37.6–41.6 Hz**, with most 5-second windows below 45 Hz. Windows showed no such windows.

Therefore:

- There is **no fixed ARM64 “tax”** that makes every frame slow: POCO can hit the 60 Hz target with the shipping ARM64 APK.
- The deficit is **workload-dependent**. It appears when scene/object dispatch and Vulkan draw recording grow.
- The emulator cannot isolate the cause: native ARM64 AVD refuses to boot on this x86_64 host, and the x86_64 fallback crashes under Berberis/ranchu Vulkan.
- The remaining attribution is **guest ARM64 recomp execution + Vulkan CPU recording**, with driver/scheduling as an unquantified modifier. GPU execution is not the primary limiter based on the earlier timestamp data.

No engine or retail optimization was introduced in this work.

---

## 2. Test configuration

| Item | Windows x64 | POCO F7 |
| --- | --- | --- |
| CPU | AMD Ryzen 7 5700X | Snapdragon 8s Gen 4 |
| GPU / Vulkan | NVIDIA RTX 5060, Vulkan 1.4.357, driver `0x9B438000` | Adreno 825, Vulkan 1.3.284, driver `0x80320047` |
| OS | Windows 10.0.19045 | Android 17 |
| Resolution | 2772x1280 window/video, swapchain confirmed | 2772x1280 landscape |
| Backend | `xenos` → Vulkan | native Vulkan |
| Save snapshot | reset from POCO `user_from_device/user` before each run | existing app data preserved |
| Retail telemetry | `REX_DIAG_PROFILE=OFF`, `REX_TIMING_PROBES=OFF` | same |
| Input | keyboard A = `Space` ×3 | touch A at `(2545,1015)` ×3 |
| Battery / thermal | mains powered | 13–16%, USB powered, thermal status 0 |

Detailed procedures are in [`BENCHMARK_METHODOLOGY.md`](BENCHMARK_METHODOLOGY.md).

---

## 3. Measured results

### 3.1 Primary table

| Run | Swap | PACE n | PACE avg | PACE min | Heavy windows | `frame_us` avg / p95 | SCENEDISP avg / max |
| --- | --- | ---: | ---: | ---: | --- | ---: | ---: |
| Windows probe `bench1` (unmatched 1920x1080) | 1920x1080 | 39 | 59.7 | 57.4 | 0 | 16205 / 16713 | 1934.9 / 3960.4 |
| Windows retail `bench1` (unmatched 1920x1080) | 1920x1080 | 39 | 59.5 | 58.3 | 0 | 15544 / 16484 | n/a |
| **Windows probe `bench2`** | **2772x1280** | 39 | **59.5** | 59.0 | 0 | 16318 / 16718 | 740.1 / 1350.8 |
| **Windows retail `bench2` (clean)** | **2772x1280** | 30 | **59.2** | 59.0 | 0 | 16560 / 16627 | n/a |
| **POCO `poco_bench4` (light scene)** | **2772x1280** | 41 | **59.2** | 49.4 | 0 | 14804 / 15690 | n/a |
| POCO `sonic4_038` (heavy/mixed) | 2772x1280 | 83 | 37.6 | 26.7 | 65 @ 33.3 Hz | 23524 / 30739 | n/a |
| POCO `sonic4_039` (heavy/mixed) | 2772x1280 | 51 | 41.6 | 28.5 | 32 @ 36.2 Hz | 21391 / 28905 | n/a |

“Heavy” is descriptive: a 5-second PACE window below 45 Hz. It is not a separately scripted map region.

### 3.2 Key comparisons

| Comparison | Result |
| --- | --- |
| Matched retail light: Windows `bench2` vs POCO `bench4` | **59.2 Hz vs 59.2 Hz**. POCO `frame_us` is actually lower (14.8 ms vs 16.6 ms). |
| Heavy POCO vs matched Windows retail | 37.6–41.6 Hz vs 59.2 Hz: deficit of **17.6–21.6 Hz** and **4.8–7.0 ms** per frame. |
| POCO light windows in heavy sessions | ~58.8–58.9 Hz, essentially matching Windows. |
| Windows across scene loads | PACE stayed 59.5–59.7 even when `SCENEDISP` averaged 740/s in `bench2` vs 1935/s in `bench1`. |

The Windows probe workload varied substantially between nominally identical runs, yet PACE did not move. Scene workload is therefore not perfectly repeatable, but the Windows path has enough headroom across the observed range.

---

## 4. Scenario validity

### 4.1 POCO `poco_bench4` — light/stable gameplay

- Swapchain created at `14:39:03.545`.
- First PACE at `14:39:10.695` (49.4 Hz during warm-up), then ~59.6–59.7 Hz for the long steady tail.
- Screenshot comparison:
  - menu vs gameplay-start mean RGB difference: ~`139/115/59` — the taps changed the scene substantially.
  - gameplay-start vs gameplay-end mean RGB difference: ~`18/13/17` — the steady scene was stable.
- Retail build does not log `SCENEDISP`, so the exact zone/object count cannot be named.

This is a valid measured **light-scenario** benchmark, not proof that heavy gameplay is fixed.

### 4.2 POCO `sonic4_038` / `sonic4_039` — heavy/mixed gameplay

These are earlier native-device sessions with traversal and many heavy windows. They are the best available measured evidence for the slow regime:

- `sonic4_038`: 65/83 windows <45 Hz, average 33.3 Hz in those windows.
- `sonic4_039`: 32/51 windows <45 Hz, average 36.2 Hz in those windows.

They were not produced by the final fixed script, so they should be labelled as historical/heavy device measurements rather than a clean repeat of `poco_bench4`.

### 4.3 Script failures that were diagnosed

- Windows retail `bench2` first attempt: prior probe's Alt+F4 focus leaked into the newly launched window. Log preserved as `retail_bench2_failed_focus_race.log`; input helper now retries activation and adds inter-run delays.
- POCO `bench2`/`bench3`: `run-as` commands were passed as one argv string, so log discovery failed and the driver aborted. `sonic4_040`/`041` still contained data proving the app was alive. Driver now passes `ls`, `-1t`, `path` and `cat`, `path` as separate argv entries.

These were harness bugs, not game regressions.

---

## 5. Cause separation

### 5.1 Diag instrumentation and timing probes — excluded for retail

Both primary comparison binaries have diagnostic profiling and heavy timing hooks disabled. The heavy deficit persists in that configuration. The earlier A/B work already measured ~+4.7 Hz from removing Vulkan diag and ~+1.6 Hz from removing pilot timing hooks; those gains do not close the 17–22 Hz heavy gap.

### 5.2 GPU execution — not the primary limiter

Earlier diagnostic runs with GPU timestamps measured:

| Regime | `gpu_exec_us_avg` | `record_us_avg` | draws/present |
| --- | ---: | ---: | ---: |
| Light | ~819 µs | ~2637 µs | ~102 |
| Heavy | ~1920–2556 µs | ~6537–9504 µs | 223–346 |

GPU execution stays in the 0.8–2.6 ms range while Vulkan CPU recording grows to 6.5–9.5 ms in heavy scenes. The matched light-scenario parity on POCO also argues against a fixed Adreno bottleneck.

### 5.3 Guest ARM64 recompilation — plausible and workload-scaled

Static evidence from [`ARM64_CODEGEN_CORRELATION_REPORT.md`](ARM64_CODEGEN_CORRELATION_REPORT.md):

- 2084 sampled hot ARM64 instructions: **44.15% loads/stores**, **7.34% byteswaps**.
- Generated functions are dense in `REX_LOAD_*` / `REX_STORE_*` macros.
- `sub_821C62C8` has 16 load macros, 7 store macros, 8 CR compares and a timebase query; its disassembly has 219 instructions with 61 stores, 44 loads and 20 byteswaps.
- Prior simpleperf put `sub_821C62C8` at ~10.8% and the clock poll `sub_821C4870` at ~3.5% in heavy gameplay.

The light-scenario result is important: the same ARM64 code can sustain 60 Hz when workload is low. The static codegen cost becomes limiting only when scene/object dispatch and Vulkan recording scale up.

### 5.4 Vulkan CPU recording — scales with draws and is shared code

The Xenos→Vulkan command-recording core is shared between Windows and Android. On Windows it handles several thousand scene dispatches per second without PACE loss. On Android, heavy scenes historically show `draws_per_present` 223–346 and `record_us_avg` 6.5–9.5 ms. The deficit is therefore consistent with **guest CPU + Vulkan CPU recording competing inside a ~16.7 ms budget**, not a separate Android backend bug.

### 5.5 Driver / scheduling — real but unquantified

Adreno vs NVIDIA driver behavior, thread affinity and synchronization remain plausible modifiers. The emulator cannot isolate them:

- ARM64 image cannot boot on this x86_64 host.
- x86_64 + Berberis + gfxstream/ranchu adds translation layers and crashes in `vk_common_SetDebugUtilsObjectNameEXT`.

No percentage is assigned to driver/scheduling from these runs.

---

## 6. Emulator status

No emulator benchmark is included. See [`EMULATOR_VALIDATION_REPORT.md`](EMULATOR_VALIDATION_REPORT.md) for installation, AVD configuration, exact ARM64 refusal, and the x86_64 SIGSEGV stack. The x86_64 path would conflate Berberis translation with gfxstream Vulkan and is invalid as a proxy for native ARM64 performance even if it launched.

---

## 7. Ranked conclusion

For the heavy-scene deficit:

1. **Primary:** workload-scaled guest ARM64 recomp execution plus Vulkan CPU command recording.
2. **Modifier:** Adreno driver / thread scheduling; not quantified here.
3. **Not primary:** GPU execution, diagnostic instrumentation, `-O3`/LTO absence, or the reverted `PipelineDescription` cache.
4. **Unavailable:** emulator-based isolation on this host.

The light-scenario result proves the ARM64 port is not intrinsically locked below 60 Hz. The unsolved problem is keeping the heavy regime inside the frame budget.

---

## 8. Unresolved questions and next measurements

1. Run a paired diag build on Windows and POCO with the **same** save/scene and collect `SCENEDISP`, `draws_per_present`, `record_us_avg` and GPU timestamps in the same window.
2. Capture per-thread traces on POCO during a heavy window and separate Main/XThread, GPU Commands, Audio Worker and SDLThread.
3. In a non-retail experiment branch, test whether reducing generated memory traffic/byte swaps changes heavy PACE; do not merge without A/B evidence.
4. If an ARM64 host becomes available, boot the native ARM64 AVD there for optional architecture comparison; do not use the x86_64 fallback.

---

## 9. Scope and regressions

- No engine optimization introduced.
- `PipelineDescription` cache remains reverted.
- No D3D12, Vulkan, Android, audio, touch or save behavior changed.
- POCO installation/save data preserved; no uninstall or wipe.
- Only benchmark drivers, evidence tools, and reports were added/changed.

---

## 10. Artifacts

| Path | Meaning |
| --- | --- |
| `FINAL_STATUS.md` | Consolidated status, verification, and unresolved-work index |
| `raw/comparative_summary.csv` | Machine-readable primary comparison table |
| `raw/windows/run_probe/logs/probe_bench2.log` | Matched Windows probe with `SCENEDISP` |
| `raw/windows/run_retail/logs/retail_bench2_clean.log` | Primary matched Windows retail telemetry |
| `raw/windows/run_retail/logs/retail_bench2_failed_focus_race.log` | Diagnosed harness failure |
| `raw/poco/poco_bench4_sonic4_042.log` | Matched POCO light-scenario retail telemetry |
| `raw/poco/sonic4_038.log`, `raw/poco/sonic4_039.log` | Heavy/mixed native POCO telemetry |
| `raw/poco/poco_bench4_start.txt`, `poco_bench4_end.txt` | Battery/thermal/GPU/top snapshots |
| `raw/arm64_static/disasm_hot.txt` | ARM64 hot-function disassembly sample |
| `raw/arm64_static/codegen_hot_functions.csv` | Generated-source correlation |
| `raw/emulator/` | Emulator installation and failure evidence |
| `scripts/run-win-scenario.ps1`, `scripts/run-poco-scenario.ps1` | Repeatable benchmark drivers |
| `tools/compare_benchmarks.py`, `tools/summarize_probe_log.py` | Structured summarizers |
