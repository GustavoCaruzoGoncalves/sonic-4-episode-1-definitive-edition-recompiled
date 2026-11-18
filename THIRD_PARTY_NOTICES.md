# Third-party notices

This project builds on several open-source and research codebases. The tables below are a
**practical index** for contributors; they are not a substitute for reading each component's
full license text in-tree.

## Primary runtime and recompilation stack

| Component | Location | License (summary) | Notes |
|-----------|----------|-------------------|-------|
| **ReXGlue SDK** (vendored, locally modified) | `third_party/rexglue-sdk/` | BSD 3-Clause (ReXGlue); portions from Xenia | Xbox 360 HLE runtime, Vulkan/D3D12 GPU, `rexglue codegen`, audio, kernel. **This tree includes Android performance patches (Exp-007–018).** |
| **Xenia** (via ReXGlue heritage) | within ReXGlue | BSD 3-Clause | GPU, kernel, and base code lineage; see ReXGlue `LICENSE`. |
| **360tools** | `third_party/360tools/` | MIT | STFS extraction (`extract_stfs.py`); reference tooling. |
| **XenonRecomp** | `third_party/XenonRecomp/` | See upstream | Reference only; not linked at build time. |
| **XenosRecomp** | `third_party/XenosRecomp/` | See upstream | Reference only. |

Full ReXGlue license: `third_party/rexglue-sdk/LICENSE`.

## ReXGlue `thirdparty/` bundle

ReXGlue vendors many libraries under `third_party/rexglue-sdk/thirdparty/`, including but not
limited to SDL3, FFmpeg, fmt, spdlog, glslang, SPIRV-Tools, VMA, imgui, Tracy, simde, xxHash,
CLI11, libmspack, and others. Each subfolder contains its own `LICENSE` file.

When redistributing binaries you build, comply with the attribution and source-offer requirements
of **every** linked dependency (notably FFmpeg if you ship audio support).

## Sonic 4 project code

Original project-specific sources in this repository (for example `pilot/src/`, `android/`,
root build scripts, and `docs/`) are offered under the BSD 3-Clause License in the repository
root `LICENSE`, **except** where a file header states otherwise.

## Relationship to hedge-dev projects

This project is **independent** of [Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp)
(GPL-3.0). We study their public documentation and engineering practices; we do not ship or
combine their source code.

## Upstream ReXGlue

Local changes in `third_party/rexglue-sdk/` are based on upstream
[rexglue/rexglue-sdk](https://github.com/rexglue/rexglue-sdk) (release **v0.10.0**, commit `c94f5eb`
at import time). A consolidated change log of performance-oriented patches is described in
`docs/performance/CURRENT_STATUS.md` and `docs/experiments/`.