# Benchmark methodology

**Date:** 2026-11-17  
**Purpose:** make the Windows x64, POCO F7 ARM64, and failed-emulator work comparable and repeatable without changing retail behavior.

---

## 1. Test subjects

| Role | Binary / artifact | Notes |
| --- | --- | --- |
| Windows probe | `C:\S4EP1Recomp\pilot\out\build\win-amd64-release\sonic4.exe`, copied to `raw/windows/run_probe/sonic4.exe` | Desktop diagnostic/default build: `REX_TIMING_PROBES=ON`; emits `SCENEDISP` and `DIAG_VK` |
| Windows retail-matched | Built by `scripts/build-win-retail-benchmark.ps1`, copied to `raw/windows/bin/sonic4.exe` then `raw/windows/run_retail/sonic4.exe` | Android-equivalent hot path: `REX_DIAG_PROFILE=OFF`, `REX_TIMING_PROBES=OFF`; keeps only lightweight PACE telemetry |
| Android retail | `C:\S4EP1Recomp\android\dist\Sonic4-arm64-v8a.apk` | Already installed on POCO F7; `REX_DIAG_PROFILE=OFF`, `REX_TIMING_PROBES=OFF` |
| Emulator | `PerfARM64_API36` / `PerfX86_API36` in `C:\S4EP1Recomp\android\perf-sdk` | Validation only; no valid gameplay benchmark |

Hashes recorded earlier:

| Artifact | SHA-256 |
| --- | --- |
| APK | `7cfa543e3135f0c98b0edadbb4f5e000a79f4275f842a193a95d1a129c81ec4a` |
| APK `lib/main.arm64-v8a/libmain.so` | `2da2f5b0da9e6558819e9206cb934fedc266442cd2849782d7826a20784f0382` |

---

## 2. Save-data handling

POCO F7 installation and save data were preserved:

- No uninstall, no factory reset, no `adb shell rm` of app data.
- Installations used `adb install -r` earlier in the investigation.
- A snapshot was pulled to `raw/poco/user_from_device/user.tar` and extracted to `raw/poco/user_from_device/user/`.
- Before each Windows benchmark, `raw/windows/run_<variant>/user` is deleted and recreated from that snapshot; device logs from the snapshot are removed.
- Windows `sonic4.toml` was found unreliable in isolated run directories, so all runtime settings are passed as CLI arguments.

This gives both platforms the same title ID / save state (`58410A07`) at benchmark start.

---

## 3. Windows scenario

Driver: `scripts/run-win-scenario.ps1`.

CLI arguments:

```text
--game_data_root=C:\S4EP1Recomp\game\extracted
--user_data_root=<run_dir>\user
--gpu_plugin=xenos
--gpu_backend=vulkan
--license_mask=1
--log_file=<run_dir>\logs\<variant>_<label>.log
--mnk_mode=true
--fullscreen=false
--vsync=true
--window_width=2772
--window_height=1280
--video_mode_width=2772
--video_mode_height=1280
```

Sequence:

1. Start process.
2. Wait 18 s; capture main menu (`*_00_menu.png`).
3. Send `Space` (A); wait 6 s.
4. Send `Space` (A); wait 6 s.
5. Send `Space` (A); wait 12 s.
6. Capture gameplay start (`*_01_gameplay_start.png`).
7. Measure steady gameplay for 150 s.
8. Capture gameplay end (`*_02_gameplay_end.png`).
9. Close via Alt+F4; force-stop only as fallback.

Input map used by the game: A = `Space` or `Semicolon`, B = `Backspace`/`Quote`, Start = `Return`/`X`, D-pad = `Shift+Arrow`, left stick = `WASD`. `mnk_mode=true` is required for this input path.

The first `bench1` pair used the previous default `1920x1080` request and is retained as an unmatched-resolution baseline. `bench2` requests `2772x1280` to match the POCO landscape resolution; the log must contain:

```text
VulkanPresenter: Created 2772x1280 swapchain
```

---

## 4. POCO F7 scenario

Driver: `scripts/run-poco-scenario.ps1`.

Device/package:

```text
adb serial:       a63a397c
package:          com.s4ep1recomp.sonic4
activity:         .Sonic4Activity
adb executable:   C:\Users\Administrator\Downloads\platform-tools-latest-windows\platform-tools\adb.exe
```

Sequence:

1. `input keyevent KEYCODE_WAKEUP` (the screen had slept during earlier runs).
2. `am force-stop com.s4ep1recomp.sonic4`; wait 2 s.
3. `am start -n com.s4ep1recomp.sonic4/.Sonic4Activity`.
4. After the swapchain is detected, wait 10 s; capture main menu.
5. Tap A at `(2545,1015)` three times with 8 s gaps.
6. Wait 18 s; capture gameplay start and record battery/thermal/GPU/top state.
7. Measure steady gameplay for 150 s.
8. Capture gameplay end and record state again.
9. Dump logcat, pull the newest `files/user/logs/sonic4_*.log`, and summarize it.

A-button touch bounds from the in-game overlay are `[2432,902][2659,1129]`; `(2545,1015)` is the centre. Landscape resolution is `2772x1280`.

POCO battery was 16–17% and USB powered during earlier attempts, so wake and power state are recorded with each run.

---

## 5. Metrics

All logs are parsed by `tools/summarize_probe_log.py` and `tools/compare_benchmarks.py`.

| Metric | Regex / source | Meaning |
| --- | --- | --- |
| `PACE_821C0770/s` | 5 s telemetry windows | Guest main-loop pacing rate; target 60 Hz |
| `PROBE PACE frames/s` | same telemetry line | Cross-check of pacing rate |
| `frame_us` | same telemetry line | Observed frame/main-loop interval in microseconds |
| `PROBE SCENEDISP_821A8488/s` | 5 s probe windows, probe builds only | Scene-dispatch workload |
| `draws_per_present_avg` | `PROBE DIAG_VK`, diag/probe builds only | Vulkan draw workload |
| `record_us_avg` | `PROBE DIAG_VK`, diag/probe builds only | Vulkan command recording CPU time |
| `gpu_exec_us_avg` | GPU timestamp diag, where available | Adreno GPU execution time |
| Swapchain size | `VulkanPresenter: Created ... swapchain` | Confirms matched resolution on Windows |

Descriptive workload bins used in reports:

- **Heavy:** `PACE_821C0770 < 45 Hz`
- **Light:** `PACE_821C0770 >= 55 Hz`
- **Mid:** `45 <= PACE < 55 Hz`

These bins are post-hoc classifications of observed telemetry windows, **not** separately controlled “sky” vs “ground” scenes.

---

## 6. Equivalence limits

- Windows input is keyboard A (`Space`); Android input is a touch at the A overlay centre. They are semantically equivalent menu/game activation, not bit-identical input timing.
- The game may place the player in slightly different animation/camera states after menu navigation. Heavy/light bins are therefore used to compare workload regimes rather than claim identical moments.
- Windows uses an RTX 5060 with desktop NVIDIA Vulkan; POCO F7 uses Adreno 825 with mobile Vulkan 1.3.284. The Vulkan command-recording code is shared, but surface/driver paths differ.
- The Windows probe build has extra timing hooks; the Windows retail-matched and Android APK builds do not. Retail-to-retail PACE is the primary comparison; probe data is used only for workload decomposition.
- Thermal state and battery/USB power are recorded but not actively clamped.
- No emulator result is included because the ARM64 AVD cannot boot and the x86_64 path crashes under Berberis/ranchu Vulkan.

---

## 7. Reproduce

```powershell
# Windows matched-resolution pair
& .\evidence\ANDROID_PORT\scripts\run-win-scenario.ps1 -Variant probe  -SteadySeconds 150 -Label bench2
& .\evidence\ANDROID_PORT\scripts\run-win-scenario.ps1 -Variant retail -SteadySeconds 150 -Label bench2

# POCO F7
& .\evidence\ANDROID_PORT\scripts\run-poco-scenario.ps1 -SteadySeconds 150 -Label poco_bench2

# Tables
python .\evidence\ANDROID_PORT\tools\compare_benchmarks.py `
  'Win probe bench2=C:\S4EP1Recomp\evidence\ANDROID_PORT\raw\windows\run_probe\logs\probe_bench2.log' `
  'Win retail bench2=C:\S4EP1Recomp\evidence\ANDROID_PORT\raw\windows\run_retail\logs\retail_bench2.log' `
  'POCO poco_bench2=C:\S4EP1Recomp\evidence\ANDROID_PORT\raw\poco\poco_bench2_sonic4_XXX.log'
```

---

## 8. Scope control

No engine optimization, pipeline-cache reintroduction, renderer change, audio/touch/save change, or retail instrumentation change was made for these benchmarks. The only code changes were evidence scripts, benchmark drivers, and reports.
