# Comparação de implementação — Recomp vs emuladores Android

| Dimensão | Sonic4 Recomp (ReXGlue) | Xendroid+ / Xenia |
| --- | --- | --- |
| **CPU guest** | AOT: `rexglue codegen` → `libmain.so` | JIT blocos PPC → ARM64 em runtime |
| **Entrada thread** | `GetFunction` + call direto | Dispatcher JIT + cache de código |
| **GPU** | Xenos → Vulkan (`librexgpu-xenos`) | Xenia GPU + patches Turnip |
| **Shader “interpreter”** | `ShaderInterpreter` (microcódigo GPU) | Similar (ucode Xenos) |
| **Medição POCO F7** | PACE 38–60 Hz (cenário-dependente) | Publicado 20–52 fps por título |
| **Transferível** | Relógio ARM64, Vulkan present, redução spin | **Não** copiar JIT |

Ver `CPU_EXECUTION_ARCHITECTURE_AUDIT.md` para evidência de perfil.