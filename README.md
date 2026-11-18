# Sonic 4 Episode 1 — Definitive Edition Recompiled

A **community-driven, experimental** native **ARM64** recompilation of the **Xbox 360** version of
*Sonic the Hedgehog 4: Episode I*, targeting **Android** (primary) with an in-progress **Windows
D3D12** path.

> **This is not a finished product.** The project boots on Android, reaches menus and first-stage
> gameplay, and has measurable performance wins—but optimization, correctness, compatibility, and
> validation remain **active engineering work**.

> **This repository does not include game assets or the original XEX.** You must supply data from a
> **legally obtained copy** of the game on your own machine. See
> [docs/legal/DISTRIBUTION_POLICY.md](docs/legal/DISTRIBUTION_POLICY.md).

---

## Project status (honest snapshot)

| Area | State |
|------|--------|
| PPC → C++ AOT via ReXGlue | Working for this title (pilot project) |
| Android ARM64 APK | Builds; primary dev device **POCO F7** (Snapdragon, Adreno) |
| Vulkan Xenos translation | Primary Android graphics path; heavy local tuning |
| Windows x64 / D3D12 | Present in tree; **not re-validated** after all recent shared SDK changes |
| Performance | Major CPU wins documented (see below); heating, stutter, GPU/CPU balance **open** |
| Legal / distribution | Source-only; no SEGA assets in git |

---

## Features (verified vs experimental)

### Implemented and exercised on Android (POCO F7)

- Static recompilation (PowerPC guest → native ARM64 AOT)
- ReXGlue runtime: kernel HLE, VFS, audio (SDL/FFmpeg path), SDL3 window/input
- Vulkan-backed Xenos GPU command processor and presenter
- Touch-driven virtual gamepad (`pilot/src/sonic4_android_pad.cpp`)
- Menus and **Splash Hill Zone** (first stage) gameplay sessions used for benchmarks
- Build/packaging flow: CMake/Ninja → `package-arm64.ps1` → signed debug APK

### Performance / runtime patches (vendored in `third_party/rexglue-sdk/`)

| ID | Topic | Default (Android) | Measured outcome |
|----|--------|-------------------|------------------|
| **Exp-007** | Texture descriptor reuse (`REX_TEXTURE_DESCRIPTOR_REUSE`) | **ON** | ~**23.5%** lower Vulkan command **recording** time (paired A/B, stationary first stage) |
| **Exp-008** | Presenter pipeline reuse (`present_pso_reuse`) | **ON** | Stops per-frame Adreno pipeline recompile on paint |
| **Exp-009** | Host-clock vblank (`vblank_host_clock`, `vblank_host_hz`) | **ON** | Fixes vblank interrupt storm (~millions/s → ~1000/s) |
| **Exp-010** | Blocking multi-wait (`wait_multi_block`) | **ON** | Removes audio/worker busy-spin |
| **Exp-008–010 combined** | — | — | Process CPU **~4.30 → ~2.57** cores (same scene, controlled) |
| **Exp-011** | Classic `VkRenderPass` vs dynamic rendering | **OFF** on Android (`vulkan_dynamic_rendering=false`) | `record_us_pf` **~5.5 → ~1.45 ms**; process CPU **~2.49 → ~1.68** cores |
| **Exp-012** | Shared-memory upload hoisting (`shm_upload_hoist`) | **OFF** | Tested; **net CPU regression** after Exp-011; kept for research |
| **Exp-013** | GPU timestamp attribution | Telemetry only | ~**12.3 ms** GPU/frame stationary; resolves/untile **~3.8 ms** between passes |
| **Exp-014** | Tight render areas (`render_area_tight`) | **OFF** | No GPU time win on Adreno 825 |
| **Exp-015** | Skip `/proc/self/maps` on write-watch faults | **ON** | Fault handler **~886 µs → ~56 µs** |
| **Exp-016** | Pipeline/shader storage seek + driver cache save | **ON** | Mid-level pipeline compile hitches largely removed |
| **Exp-017** | Dedicated present queue | **ON** | Queue lock wait **~208 ms/s → ~1 ms/s**; fewer >33 ms frames |
| **Exp-018** | IssueDraw phase timers | Diagnosis | Spikes correlate with **`vkWaitForFences`** (in-flight frames), not IssueDraw |

Details, raw logs, and limitations: [docs/performance/CURRENT_STATUS.md](docs/performance/CURRENT_STATUS.md) and
[docs/experiments/](docs/experiments/).

### Experimental / not production-ready

- GLES backend feasibility notes only ([docs/experiments/GLES_VS_VULKAN_FEASIBILITY.md](docs/experiments/GLES_VS_VULKAN_FEASIBILITY.md))
- Progressive **gameplay stutter** while moving through the first stage (partially diagnosed, not fully fixed)
- Device **heat and power** under sustained play
- Shutdown crash on app exit (`KernelState::Terminate` path)
- Windows build parity with latest SDK patches

---

## Known limitations

See [docs/performance/CURRENT_STATUS.md](docs/performance/CURRENT_STATUS.md). Summary:

1. Host CPU and GPU scheduling overhead remain high versus a native game port.
2. **Stutter increases** as Sonic progresses through the first stage (~0.5% of frames >33 ms after Exp-017, with remaining fence waits).
3. **~200 texture reloads/s** (render-to-texture path) — not addressed.
4. Guest clock / timebase interactions still need review (`ResetGuestClockHostBaseline`).
5. Windows D3D12 not regression-tested on latest shared code.

---

## Supported platforms

| Platform | Status |
|----------|--------|
| **Android ARM64** (API 28+) | Primary; tested on **POCO F7** |
| **Android x86_64** | Build scripts exist; secondary |
| **Windows x64 D3D12** | CMake presets + `scripts/build-pilot.ps1`; validation **out of date** |

---

## Requirements

- **Legal game data**: Xbox 360 STFS/GOD package for Title ID `0x58410A07` — extract locally (see [docs/building/GAME_DATA.md](docs/building/GAME_DATA.md)).
- **Windows host** for Android cross-compile (project has been developed on Windows).
- **Android NDK** (r28c used in `build-android-arm64.bat`, e.g. `28.2.13676358`).
- **Android SDK** build-tools + platform (for `package-arm64.ps1`).
- **CMake ≥ 3.25**, **Ninja**, **Python 3** (STFS extract).
- **Pre-built host `rexglue.exe`** and Android `librexruntime.so` from building ReXGlue SDK once on Windows (`third_party/rexglue-sdk`).

Full steps: [docs/building/ANDROID.md](docs/building/ANDROID.md), [docs/building/WINDOWS.md](docs/building/WINDOWS.md).

---

## Quick start (Android ARM64)

```powershell
git clone https://github.com/GustavoCaruzoGoncalves/sonic-4-episode-1-definitive-edition-recompiled.git
cd sonic-4-episode-1-definitive-edition-recompiled

# 1) Provide game files locally (never commit them):
#    see docs/building/GAME_DATA.md

# 2) Build ReXGlue SDK (host + Android), then the pilot — details in docs/building/ANDROID.md

# 3) Typical release APK build (from repo root):
.\build-android-arm64.bat

# 4) Install (example):
adb install -r android\dist\Sonic4-arm64-v8a.apk
```

On-device runtime toggles (debug builds): `adb shell setprop debug.rex.cvars "name=value,..."` then restart the app.

---

## Repository layout

| Path | Purpose |
|------|---------|
| `pilot/` | ReXGlue recompilation project (`rexglue codegen` output in `pilot/generated/`, not in git) |
| `android/` | APK packaging, Java shell, on-screen controls |
| `third_party/rexglue-sdk/` | **Vendored, modified** ReXGlue runtime and tools |
| `docs/` | Architecture, building, performance, legal |
| `evidence/` | Benchmark reports and scripts (large binaries/screenshots gitignored) |
| `scripts/` | Windows pilot build helpers |

---

## Where we need help

We published this tree so engineers with deeper Xbox 360, Xenos, Vulkan, or ARM experience can
challenge our assumptions. Highest-impact areas:

1. Reducing **unnecessary host CPU** work (guest spins, threading, MMIO).
2. Diagnosing **progressive stutter** during first-stage gameplay.
3. Improving **Xenos → Vulkan** efficiency (submissions, resolves, render-pass structure).
4. **GPU resource streaming** / render-to-texture reload behavior.
5. **ARM64 AOT** code quality and hot paths in generated guest code.
6. **Synchronization and memory** (fences, in-flight frames, swapchain depth).
7. **Windows D3D12** regression and parity with Vulkan changes.
8. **Profiling reproducibility** (shared harnesses, clearer workloads).

See [CONTRIBUTING.md](CONTRIBUTING.md).

---

## Legal

*Sonic the Hedgehog* and *Sonic the Hedgehog 4: Episode I* are trademarks and copyrighted works of
their respective owners (including **SEGA**). This project is **unofficial**, **not affiliated** with
SEGA, and distributed **free of charge** for preservation and engineering research.

[docs/legal/COPYRIGHT.md](docs/legal/COPYRIGHT.md) · [docs/legal/DISTRIBUTION_POLICY.md](docs/legal/DISTRIBUTION_POLICY.md)

---

## Documentation index

- [Architecture overview](docs/architecture/OVERVIEW.md)
- [Performance status](docs/performance/CURRENT_STATUS.md)
- [Roadmap](docs/development/ROADMAP.md)
- [Legacy Portuguese planning docs](docs/development/LEGACY_PLANNING.md) (historical context)

---

## Credits

Built on [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) and the [Xenia](https://github.com/xenia-project/xenia)
lineage. Inspired by community recompilation efforts such as
[Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp) (engineering practices only;
no code sharing).