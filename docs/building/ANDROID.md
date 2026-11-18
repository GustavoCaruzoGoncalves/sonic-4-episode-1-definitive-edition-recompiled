# Building for Android (ARM64)

Primary target: **arm64-v8a**, API **28+**, **Vulkan** graphics.

## Prerequisites

| Tool | Notes |
|------|-------|
| Windows host | Current scripts assume Windows paths |
| CMake ≥ 3.25, Ninja | Used by all presets |
| Android SDK | `adb`, `build-tools`, `platforms` (see `android/package-arm64.ps1`) |
| Android NDK | `build-android-arm64.bat` uses `%LOCALAPPDATA%\Android\Sdk\ndk\28.2.13676358` — adjust if needed |
| Java `javac` 17+ | APK compile step |
| Python 3 | STFS extraction |
| ReXGlue built for **win-amd64** + **android-aarch64** | See below |

## One-time: build ReXGlue SDK

From `third_party/rexglue-sdk/` (see upstream docs):

1. Initialize git submodules under `thirdparty/` if you use a fresh clone (vendored tree in this repo
   already contains sources).
2. Configure Windows host: `cmake --preset win-amd64` → build → install (produces `rexglue.exe`).
3. Configure Android: Android NDK preset (e.g. `android-aarch64`) → `librexruntime.so` under
   `out/android-aarch64/`.

Host `rexglue.exe` is required because Windows cross-build runs **codegen on the host** (`pilot/CMakeLists.txt`).

## Generate pilot code (local)

With `game/extracted/default.xex` in place:

```powershell
cd pilot
rexglue codegen
```

This creates `pilot/generated/` (gitignored).

## Release APK (default options)

From repo root:

```powershell
.\build-android-arm64.bat
```

CMake flags in that script (verified):

- `REX_TEXTURE_DESCRIPTOR_REUSE=ON`
- `REX_BATCH_DRAW_RESIDENCY=ON`
- `REX_RENDER_TELEMETRY=OFF`
- `REX_DIAG_PROFILE=OFF`
- Release build, Ninja parallel

Output APK: `android\dist\Sonic4-arm64-v8a.apk`

## Install

```powershell
adb install -r android\dist\Sonic4-arm64-v8a.apk
```

Push game data according to your runtime/VFS configuration (typically packaged or mounted on device
— **project-specific deployment of assets is still manual**; confirm paths in your local setup).

## Variant build scripts

| Script | Purpose |
|--------|---------|
| `build-android-arm64-telemetry-on.bat` | Frame stats / `PROBE` logging |
| `build-android-arm64-exp007-noreuse.bat` | A/B without texture descriptor reuse |
| `build-android-arm64-exp004*.bat` | Historical experiments |
| `build-android-x86_64.bat` | Emulator ABI |

## Runtime tuning (debug)

```powershell
adb shell setprop debug.rex.cvars "vulkan_dynamic_rendering=false,present_pso_reuse=true"
```

Restart the app after changing properties.

## Troubleshooting

- **Missing `librexruntime.so`**: build Android preset of ReXGlue first; path is hardcoded in
  `package-arm64.ps1`.
- **Codegen fails on Android cross-build**: ensure `REXSDK_DIR` points to `third_party/rexglue-sdk` and
  host `rexglue.exe` exists.
- **NDK path**: edit `build-android-arm64.bat` if NDK revision differs.