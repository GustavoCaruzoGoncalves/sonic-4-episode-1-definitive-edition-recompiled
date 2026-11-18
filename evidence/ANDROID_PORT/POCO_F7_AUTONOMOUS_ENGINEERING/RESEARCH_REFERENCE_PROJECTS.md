# Referências — projetos Android / Xbox 360

**Dispositivo alvo:** POCO F7 (Snapdragon 8s Gen 4, Adreno 825, Android 17, ARM64, Vulkan).

## Repositórios estudados

| Projeto | Modelo | Relevância para Sonic4 Recomp |
| --- | --- | --- |
| [Xendroid+](https://github.com/phforner0/Xendroid-Plus) | JIT PowerPC + Xenia GPU | **Medições publicadas no mesmo POCO F7**; otimizações Adreno/Turnip; redução de spin em workers; apresentação FIFO no HyperOS |
| [xenia-project/xenia](https://github.com/xenia-project/xenia) | Emulador completo | Subsistemas GPU (command processor, shaders), referência de comportamento Xenos |
| [xenia-android/xenia-android](https://github.com/xenia-android/xenia-android) | Port Android | Integração host Vulkan/SDL |
| [DEGROUCHO/xendroid-mali](https://github.com/DEGROUCHO/xendroid-mali) | Mali | Menos aplicável ao Adreno 825; útil para padrões Vulkan mobile |
| [chuckstation/xeo](https://github.com/chuckstation/xeo) | Variante Xenia | Histórico de mudanças mobile |
| Pinyon-Shift (clone local) | Recomp estática | Paralelo arquitetural: guest memory 4K, critical region, lock-free clock |

## Xendroid+ — medições POCO F7 (out/2026, [doc oficial](https://phforner0.github.io/Xendroid-Plus/docs/performance/))

- Sonic Unleashed gameplay: **50–52 fps** (título comparável em escala).
- GOW3 / Crysis 3: cap **30 fps** com otimizações de cópia de superfície e **sleep em workers** em vez de spin CPU.
- Need for Speed MW: **~21 fps**; passes de profundidade MSAA→1x diretos (**~+7%** medido).
- Build 51: **FIFO presentation** evita congelamento da imagem após minutos no HyperOS; resolves direto em texturas.

## Técnicas transferíveis vs não transferíveis

| Técnica Xendroid/Xenia | Aplica ao ReXGlue? |
| --- | --- |
| JIT PPC / blocos traduzidos dinamicamente | Não — usamos código **estático** em `libmain.so` |
| Spin → sleep nos job workers do **guest** | Parcial — hotspots `sub_821C62C8` / `sub_821C4870` (recomp) |
| Otimizações de resolve / MSAA no pipeline Vulkan | Parcial — mesmo backend Xenos→Vulkan, título-específico |
| Turnip + AdrenoTools | Opcional diagnóstico; build retail usa driver do sistema |
| Contador de tempo ARM64 de baixo custo | **Sim** — `cntvct_el0` no host (`clock_posix.cpp`) |
| Comparação A/B por sessão (≥30 s, A B B A) | Sim — metodologia adotada nos experimentos |

## Próximas linhas de pesquisa

1. `simpleperf` retail em cenas leves vs pesadas (`sub_821C62C8`, `IssueDraw`, `librexgpu-xenos.so`).
2. Comparar apresentação Vulkan/swapchain com recomendação FIFO (HyperOS).
3. Revisar `primitive_processor` / `vertex_fetch` com dados de `ISSUE_DRAW_ROOT_CAUSE.md` (build diag separado).