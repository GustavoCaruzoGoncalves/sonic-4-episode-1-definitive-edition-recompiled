# GLES 3.2 vs Vulkan — feasibility (POCO F7 / Adreno 825)

**Date:** 2026-11-17  
**Status:** Investigation only — **no migration**, **no GLES game build yet**  
**Device:** POCO F7, Adreno 825, driver `OpenGL ES 3.2 V@0800.71`, `ro.opengles.version=196610` (3.2)

---

## Executive summary

| Question | Answer |
|----------|--------|
| Is there an existing GLES backend? | **No** — ReXGlue/Xenos plugin exposes **Vulkan** (Android) and **D3D12** (Windows) only. |
| Is multi-backend architecturally possible? | **Yes** — same split as D3D12: shared `pipeline/*`, `command_processor.cpp` PM4; per-backend `*CommandProcessor`, caches, presenter. |
| Is Vulkan alone “fundamentally broken” on Adreno 825? | **Not proven.** Measured **guest + GPU path ~19 ms/frame (~41 Hz PACE)**; **~7.8 ms IssueDraw + ~6.8 ms record/present** on CPU — heavy, but **gpu_exec_us_avg ≈ 0** → not GPU-starved. GLES might trim **CPU record/bind/barrier** cost; it **cannot** remove **~200 draws/present**, **guest AOT (~19 ms loop)**, or **EDRAM/memexport** semantics. |
| Controlled GLES vs Vulkan benchmark today? | **Impossible** without a real GLES command processor — only Vulkan baseline below. |
| Recommended next step | **Phase 0:** Adreno microbench (GLES vs Vulkan draw/state loop, no Xenos). **Phase 1:** GLES scaffold + presenter + one `IssueDraw` path. **Phase 2:** Sonic 4 gated A/B (20 s first stage). |

**Do not claim GLES is faster** until Phase 2 completes with identical workload gates.

---

## 1. Architecture audit (ReXGlue `rexgpu-xenos`)

### 1.1 Plugin / factory

- `plugin_main.cpp`: `rex_gpu_create()` selects `d3d12` or `vulkan` via `GpuCreateInfo.backend` / cvar `gpu_backend`.
- Android build: `REXGLUE_USE_VULKAN=ON`, `REXGLUE_USE_D3D12=OFF` (CMake).

### 1.2 Shared (API-independent) — reuse for GLES

| Area | Location | Notes |
|------|----------|--------|
| PM4 / ring buffer | `command_processor.cpp` (~1646 lines) | Packet dispatch, guest MMIO |
| Register file | `register_file.cpp`, `registers.cpp` | Xenos state |
| Shader ucode analysis | `pipeline/shader/*` | ALU/fetch/memexport analysis |
| SPIR-V emission | `spirv*.cpp` | **Current IR for Vulkan** |
| Draw utils | `util/draw.cpp`, `draw_extent_estimator.cpp` | Viewport/scissor/math |
| Primitive semantics | `primitive_processor.cpp` (shared base) | Fan/quad/point policies |
| RT/texture logic (guest) | `pipeline/render_target/cache.cpp`, `pipeline/texture/cache.cpp` | Guest EDRAM routing |
| Packet disassembler | `packet_disassembler.cpp` | Debug |

### 1.3 Vulkan-specific (must reimplement for GLES)

| Component | Approx. size | Role |
|-----------|--------------|------|
| `vulkan/command_processor.cpp` | **7190** lines | IssueDraw, barriers, descriptors, submit |
| `vulkan/render_target_cache.cpp` | **6155** | Images, passes, resolves, FSI |
| `vulkan/pipeline_cache.cpp` | **3879** | PSO, layout, SPIR-V modules |
| `vulkan/texture_cache.cpp` | **3261** | Guest textures → VkImage |
| `vulkan/deferred_command_buffer.cpp` | **514** | Record then `Execute` → `vkCmd*` |
| `vulkan/shared_memory.cpp` | **364** | Guest RAM → VkBuffer + barriers |
| `vulkan/primitive_processor.cpp` | **234** | IB uploads |
| UI | `ui/vulkan/*` | **VulkanPresenter**, swapchain, surfaces |

**Total Vulkan-only rough order:** **~22k LOC** (excluding SPIR-V translator).

D3D12 parallel exists (`d3d12/*`, DXBC path) — **template for GLES folder layout**, not copy-paste.

### 1.4 Translation / shader path for GLES

Today:

- **Vulkan:** ucode → **SPIR-V** (`SpirvShaderTranslator`) → `vkCreateShaderModule`.
- **D3D12:** ucode → **DXBC**.

For GLES options:

1. **SPIR-V → GLSL ES** (spirv-cross or glslang SPIR-V backend) — reuses existing translator; risk of unsupported features on ES.
2. **New GLSL emitter** from ucode — huge effort; not justified initially.

**Prototype should use (1)** with capability checks per extension.

### 1.5 Presentation (Android)

- `VulkanGraphicsSystem::CreateProvider()` → `ui::vulkan::VulkanProvider` → **VulkanPresenter** only.
- **No** `ui/opengl` presenter in tree; SDL3 supports GLES on Android — new **EglPresenter** + `GLESProvider` required.

---

## 2. Adreno 825 GLES capabilities (device capture)

From `adb dumpsys SurfaceFlinger` on POCO F7:

- **GLES 3.2**, Adreno 825, Ganesh stack.
- Notable for Xenos parity:
  - `GL_EXT_geometry_shader`, `GL_EXT_tessellation_shader`
  - `GL_EXT_shader_framebuffer_fetch`, `GL_QCOM_shader_framebuffer_fetch_noncoherent`, `GL_QCOM_shader_framebuffer_fetch_rate`
  - `GL_EXT_color_buffer_float`, half-float, MRT (`GL_EXT_draw_buffers_indexed`)
  - `GL_OES_texture_3D`, ASTC/ETC/S3TC/BPTC compression ext family
  - `GL_EXT_disjoint_timer_query` (GPU timing for benchmarks)
  - **No** guarantee of D3D11-style **ROV**; EDRAM uses **fragment shader interlock** on Vulkan (`edram_fragment_shader_interlock` in CP) — GLES mapping is **non-trivial** (framebuffer fetch + QCOM extensions).

Vulkan on same device: loader reports **Vulkan 1.3** (`vulkanVersion = 4206592` in `dumpsys gpu`).

---

## 3. Is our Vulkan backend doing excessive work?

Evidence from **POCO F7, first-stage class scenes** (telemetry build, 2026-11-17):

| Metric | Typical | Interpretation |
|--------|---------|----------------|
| `PACE_821C0770` | **~39–42 Hz** | Guest main loop **~19–20 ms** — primary user-visible stall |
| `frame_us` | **~19–20 ms** | Includes guest + GPU thread work in loop hook |
| `draws_per_present` | **~189–220** | Guest still submits full scene |
| `issue_draw_us_pf` | **~7.5–9.3 ms** | **IssueDraw** (vertex residency, primitive, bindings, barriers) |
| `record_us_pf` | **~5.1–7.3 ms** | Deferred CB **record + Execute** per present |
| `submit_us_pf` | **~0.6–0.8 ms** | **Small** — driver submit not the monster |
| `gpu_exec_us_avg` | **~0** | Timestamp path sparse; GPU not long-running per sample |
| `barriers/s` | **~6–11k** | Explicit `vkCmdPipelineBarrier` batches |
| `bind_skip_pipe` / `bind_skip_desc` | **0** (record side) | Dedup happens at **Execute**, not in stats |
| Exp-007 texture reuse | **~1.5 ms** record (paired A/B) | Helps **descriptor writes**, not total frame |

**Conclusion:** Vulkan overhead on device is **real and large on the CPU**, dominated by **per-draw IssueDraw** and **deferred command recording**, not by `vkQueueSubmit`. Whether GLES is **more efficient for the same state changes** is **hypothesis**, not fact:

- **Pro-GLES:** immediate GL command stream; driver-side barrier elision; possibly cheaper texture/sampler binds for low slot counts.
- **Pro-Vulkan (stay):** same Adreno compiler; we already batch barriers in `pending_barriers_`; switching APIs does not cut **draw count** or **shader translation** cost; **EDRAM/FSI** path may be **more proven on Vulkan** in this codebase.

**Fundamental inefficiency (API-agnostic):** ~200 **`IssueDraw`** invocations/present with full pipeline/bind/texture/residency work each time, while guest simulation also runs at ~40 Hz.

---

## 4. GLES comparison — what could be faster, what cannot

| Workload | Vulkan today | GLES hypothesis | Correctness risk |
|----------|--------------|-----------------|------------------|
| Descriptor / texture bind | Transient VkDescriptorSet + `vkUpdateDescriptorSets` | `glBindTexture` / sampler objects / UBO `glBindBufferBase` | Medium — slot limits, FSI |
| Pipeline state | PSO cache + `CmdVkBindPipeline` | Program object + uniform state | Medium |
| Barriers | Explicit per-batch in deferred CB | Driver tracking | High for memexport / UAV-like |
| EDRAM + FSI | Fragment shader interlock (SPIR-V) | Framebuffer fetch / QCOM ext | **High** |
| Memexport | Shared memory + barriers | SSBO/image load-store + sync | **High** |
| Present | Vulkan swapchain | EGL + `eglSwapBuffers` | Low (new presenter) |
| Guest CPU | N/A | N/A | **Unchanged** |

---

## 5. Implementation effort (order-of-magnitude)

| Phase | Deliverable | Effort | Enables benchmark? |
|-------|-------------|--------|---------------------|
| **0** | Standalone NDK app: matched draw count, UBO+4 textures, GLES vs Vk timing on POCO | **2–5 days** | **Driver CPU hypothesis only** |
| **1** | `gles/` scaffold: GraphicsSystem, EGL provider, GLES CP stub, CMake `REXGLUE_USE_GLES` | **1–2 weeks** | No |
| **2** | MVP CP: shared memory buffer, single RT, triangle draws, SPIRV→GLSL for simple shaders | **4–8 weeks** | Partial |
| **3** | EDRAM/FSI, memexport, full RT cache parity | **3–6 months** | Sonic 4 complete |
| **4** | Sonic 4 gated A/B (20 s first stage, same gates as `validate_first_stage_workload.ps1`) | After Phase 3 | **Yes** |

**Full production GLES backend:** comparable to **initial Vulkan port (~20k+ LOC)** → **~6–12 engineer-months** with regression risk.

---

## 6. Experimental backend — scaffold (no Vulkan changes)

Added **opt-in** CMake hook only (default **OFF**):

- `REXGLUE_USE_GLES` — does not compile unless explicitly enabled **and** paired with build flags that keep Vulkan default APK unchanged.
- Stub types under `third_party/rexglue-sdk/src/graphics/gles/` — **not linked** in default Android build.

**Smallest meaningful prototype (recommended):**

1. Phase 0 microbench on device (no Sonic integration).
2. Phase 1 `GLESCommandProcessor::IssueDraw` for **one** shader pair + **no EDRAM** — still useless for Sonic 4 but validates record CPU delta.
3. Do **not** implement ANGLE/Vulkan-backed GL.

---

## 7. Benchmark plan (when GLES MVP exists)

Mirror existing policy (`MEASUREMENT_POLICY.md`):

1. Build **two APKs**: Vulkan retail/telemetry vs `REXGLUE_USE_GLES` experimental (different package id or suffix).
2. `adb install -r`, launch, first stage, Sonic stationary **before** 20 s window.
3. Gates: `draws_per_present` 175–205, draws/s, barriers/s.
4. Compare: `frame_us`, `PACE_821C0770`, backend CPU (`record_*`, `issue_draw_*`, GL timer queries), draws/present, visual screenshot.
5. **Abort GLES promotion** if any corruption or timing regression in gameplay logic.

**Current Vulkan baseline (reference, not GLES control):** `stack_h1on_first_stage` / `sonic4_070.log` — **PACE ~41.8 Hz**, **frame_us ~18692**, **issue_draw_us_pf ~7841**, **record_us_pf ~6821**, **draws_per_present ~200**.

---

## 8. Decision criteria

| Outcome | Action |
|---------|--------|
| Phase 0: GLES **≥15%** lower CPU on identical microbench | Fund Phase 1–2 |
| Phase 0: GLES ≈ Vulkan | **Do not** pursue GLES for CPU; optimize IssueDraw + guest work on Vulkan |
| Phase 2 Sonic: GLES faster **and** visually correct | Continue GLES toward parity |
| FSI/memexport failures | **Stay on Vulkan**; GLES as research only |

---

## 9. Compatibility risks (Sonic 4)

- EDRAM fragment shader interlock / resolve paths  
- Memexport + guest index DMA  
- 64-bit index, tessellation, gamma RT formats  
- Qualcomm texture format quirks (already hit on Vulkan in other ports)  
- Dual API maintenance + shader cache split (SPIR-V vs GL binary)