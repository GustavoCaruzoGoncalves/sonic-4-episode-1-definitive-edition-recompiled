# Building for Windows (x64, D3D12)

The pilot supports Windows via ReXGlue’s **D3D12** backend. **Recent Vulkan/Android SDK patches have
not been regression-tested on Windows.**

## Prerequisites

- Visual Studio 2022 with C++ workload (project used Community 2022)
- LLVM / clang as configured in ReXGlue presets
- Ninja
- ReXGlue SDK built: `cmake --preset win-amd64` in `third_party/rexglue-sdk/`
- Local `game/extracted/` and `rexglue codegen` in `pilot/`

See historical notes in root `ARCHITECTURE.md` §4.

## Build pilot

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-pilot.ps1
```

Expects existing CMake configure directory: `pilot\out\build\win-amd64-release` (create via ReXGlue
`rexglue init` / CMake preset flow if not present).

## Timing hooks

`REX_TIMING_PROBES` defaults **ON** for Windows in `pilot/CMakeLists.txt` (high overhead). Android
uses retail hooks instead.

## Status

Treat Windows as **secondary** until a maintainer re-validates:

- Shared `graphics_system.cpp` vblank changes
- Pipeline storage seek fix (Exp-016) on MSVC CRT `fopen` behavior
- Present path differences vs Vulkan Android

Report results in issues with build SHA and GPU driver version.