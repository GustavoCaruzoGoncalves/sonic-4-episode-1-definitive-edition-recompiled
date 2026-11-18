# Experimental OpenGL ES 3.2 backend (scaffold)

**Not linked in default builds.** Enable with `-DREXGLUE_USE_GLES=ON` and disable Vulkan only in isolated experiment trees.

## Status

- Architecture slot mirrors `d3d12/` and `vulkan/` (see `GLES_VS_VULKAN_FEASIBILITY.md` in repo evidence).
- No `IssueDraw`, presenter, or shader path implemented yet.

## Next implementation steps

1. `GLESGraphicsSystem` + `GLESCommandProcessor` skeleton
2. `ui/opengl` EGL presenter for Android (SDL3 GLES context)
3. SPIR-V → GLES 3.2 via spirv-cross (capability gating)
4. Phase 0 microbench APK (optional `tools/`)

Do not route ANGLE-on-Vulkan here; must use the system Adreno GLES driver.