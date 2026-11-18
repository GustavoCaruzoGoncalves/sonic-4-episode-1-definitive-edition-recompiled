# Cross-platform — Vulkan no Windows

## Arquitetura

O runtime carrega `rexgpu-xenos.dll` pelo cvar `gpu_plugin`. A fábrica `rex_gpu_create` recebe o backend: `any`, `d3d12` ou `vulkan`. Com os dois compilados, `any` continua escolhendo D3D12. A tradução do Xenos é compartilhada; D3D12 usa DXBC e Vulkan usa SPIR-V. A apresentação Vulkan já existia no SDK (`src/ui/vulkan`, `src/graphics/vulkan`) e ficava desligada no Windows (`REXGLUE_USE_VULKAN=OFF`).

## O que foi ligado

- Submódulos `glslang`, `vulkan-headers`, `vulkan-memory-allocator`, `spirv-headers` e `spirv-tools` inicializados nos commits já pinados pelo SDK.
- SDK reconfigurado com `REXGLUE_USE_D3D12=ON` e `REXGLUE_USE_VULKAN=ON`. O loader é o `vulkan-1.dll` do driver, não um SDK instalado à parte.
- Cvar `gpu_backend` em `src/ui/rex_app.cpp`, repassado a `LoadGpuPlugin`. O `sonic4.exe` compila essa fonte a partir do SDK instalado, então o executável foi religado. A correção de velocidade no código gerado não foi reescrita (`0 written`).
- DLLs anteriores guardadas em `evidence/CROSS_PLATFORM/dll_backup_before_vulkan`.

## Teste em 2026-11-16

`sonic4.exe --gpu_plugin=xenos --gpu_backend=vulkan --license_mask=1`

- Instância Vulkan 1.4.357, dispositivo NVIDIA GeForce RTX 5060.
- Swapchain 1920x1080, formato 44, modo de apresentação 0.
- Pipelines gráficos criados em seguida. `LICENSE flag160=0`.
- Nenhum `[error]`, `VK_ERROR` ou device lost nesse arranque.
- O processo permaneceu aberto para conferência visual. Entrar numa fase ainda depende de jogo manual.

D3D12 continua no mesmo plugin. Para voltar: `gpu_backend = "d3d12"` em `sonic4.toml`, ou `--gpu_backend=d3d12`.

## Fora deste marco

Linux: o CMake do SDK já desliga D3D12 e liga Vulkan, com superfície GNU/Linux e plugin `.so`. Não houve build nem execução aqui.

Android e Switch: o código recompilado desta árvore é x86-64. Vulkan no Windows não produz um binário ARM64. Um porte nativo precisa de um alvo de codegen AArch64, mais ciclo de vida de superfície no Android e um backend gráfico próprio no Switch. Isso não foi implementado.
