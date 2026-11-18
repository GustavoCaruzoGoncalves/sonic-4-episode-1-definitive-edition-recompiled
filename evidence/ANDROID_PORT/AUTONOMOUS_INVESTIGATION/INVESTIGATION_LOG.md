# Autonomous investigation log

## 2026-11-17 — Exp001: x86_64 emulator black screen / boot hang

### Symptom
- Native x86_64 APK (`Sonic4-x86_64.apk`), `primaryCpuAbi=x86_64`, libs verified AMD64.
- Swapchain created; SDL touch overlay visible; 3D area black.
- App log stops at `Initializing shader storage for title 58410A07...` (94 lines, no progress).
- No `PACE_821C0770`, no `PSO_CREATE`.
- POCO ARM64 continues past same line within ~600 ms (texture cache, PSO creation, PACE).

### Process state (emulator PID 4101)
- Threads present: `SDLThread`, `GPU Commands` (state R), `GPU VSync`, `XMA Decoder`.
- Vulkan: gfxstream → host RTX 5060 (not Berberis).

### Working hypothesis
1. **Primary:** UI thread blocked in `GraphicsSystem::InitializeShaderStorage` → `CallInThread` + `Fence::Wait` while GPU worker does not run the queued callback (possible `pending_fns_` race / missing wake).
2. **Secondary:** GPU worker stuck inside `VulkanPipelineCache::InitializeShaderStorage` (pipeline preload / thread pool).

### Next action
- Fix `CommandProcessor::CallInThread` synchronization and wake GPU worker on enqueue.
- Rebuild x86_64 APK, retest boot on emulator.
- Document first log divergence vs POCO after fix.