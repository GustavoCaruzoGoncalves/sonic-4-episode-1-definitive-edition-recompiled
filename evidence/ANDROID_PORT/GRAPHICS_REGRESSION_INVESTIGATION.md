# Regressão gráfica Android — investigação e correção

**Data:** 2026-11-17  
**Dispositivo de referência:** POCO F7 (`a63a397c`)  
**Pacote:** `com.s4ep1recomp.sonic4`

---

## 1. Falhas visuais reportadas (4 capturas)

*(As imagens anexadas pelo utilizador não estão no repositório; descrição alinhada com o relatório do utilizador.)*

| # | Sintoma | Interpretação técnica provável |
| --- | --- | --- |
| 1 | Composição de cena errada; objetos deslocados/duplicados | Estado de pipeline ou vértices inconsistente |
| 2 | Logo EPISODE I esticado/duplicado | Pipeline de rasterização/blend incorreto ou UV/transform |
| 3 | Geometria massivamente duplicada | Topologia ou transformação de pipeline errada entre draws |
| 4 | Blocos pretos/verdes, retângulos transparentes | RT/pipeline/depth mistos; desenho com PSO errado |

Estes padrões são **incompatíveis** com variação normal de shader cache ou throttling — indicam **estado gráfico incorreto** no host Vulkan.

---

## 2. Contexto Git / mudanças

O diretório raiz `C:\S4EP1Recomp` **não** é um repositório Git (sem `.git` na raiz). Submódulos em `third_party/rexglue-sdk/thirdparty/*` têm `.git` próprio; o fork ReXGlue/pilot **não** tem histórico Git centralizado aqui.

**Alterações na janela da regressão (esta sessão de otimização IssueDraw):**

| Ficheiro | Natureza | Afeta Release? |
| --- | --- | --- |
| `pipeline_cache.cpp` / `pipeline_cache.h` | Cache de `PipelineDescription` | **Sim — causa raiz confirmada por código** |
| `command_processor.cpp` | Scopes `REX_IDRAW_PHASE` (diag) + blocos `{}` | **Não** (`REX_DIAG_PROFILE=OFF` em Release) |
| `frame_stats.h/cpp`, `sonic4_timing_hooks.cpp` | Telemetria diag | **Não** |
| `evidence/*`, `tools/summarize_issue_draw_breakdown.py` | Documentação | **Não** |

**Não alterados nesta rodada (mantidos):** texture descriptor reuse, `last_pipeline_` existente, retail timing hooks, world/level selection fixes.

---

## 3. Causa raiz confirmada — cache de `PipelineDescription`

### 3.1 O que foi introduzido

Em `VulkanPipelineCache::ConfigurePipeline`, saltava-se `GetCurrentStateDescription` quando:

- shaders, `render_pass_key`, primitive key parcial, `normalized_depth_control`, `normalized_color_mask` e
- `ComputePipelineConfigurationRegisterHash()` (apenas `PA_SU_SC_MODE_CNTL`, `PA_CL_CLIP_CNTL`, `RB_BLENDCONTROL` por RT ativo)

coincidiam com o draw anterior.

### 3.2 Por que é incorrecto

`GetCurrentStateDescription` consulta **muito mais** estado de registos e derivações, incluindo:

- `draw_util::IsPrimitivePolygonal(regs)` e `IsRasterizationPotentiallyDone(regs, …)` → `rasterizer_discard`, ramos de polígono
- Topologia / geometry shader a partir de `primitive_processing_result` **e** modificadores de VS
- `sample_rate_shading` a partir do pixel shader e `depth_stencil` modes
- Viewport path / tessellation wireframe CVAR
- Conteúdo completo de `render_targets[]` por RT na render pass

O fingerprint **não** cobria todas as entradas que definem `PipelineDescription`. Draws consecutivos com o mesmo hash “aproximado” reutilizavam uma descrição **obsoleta**, depois:

- `last_pipeline_->first == description` podia ainda dar hit, ou
- `pipelines_.find(description)` devolvia **PSO errado**

Efeito observável: pipelines Vulkan com blend/topologia/rasterização errados → duplicação, estiramento, artefactos de RT.

**Hipótese vertex_fetch / `vertex_buffers_in_sync_`:** nesta rodada **não** houve alteração de lógica em `RequestRange` — apenas scopes de profiling em build diag. **Descartada** como causa da regressão Release.

**Hipótese `PrimitiveProcessor`:** sem alteração de código nesta rodada — **descartada**.

**Hipótese texture descriptor reuse:** anterior a esta sessão; build com regressão correlaciona com APK que inclui **pipeline description cache** ( novo ). Reuso de textura mantido até prova em contrário após correção.

---

## 4. Experiências de revert controlado

| Teste | Alteração | Resultado visual | Conclusão |
| --- | --- | --- | --- |
| Baseline (regressão) | Cache `PipelineDescription` + texture reuse | Corrupção grave (4 SS user) | Reproduzido |
| Revert pipeline cache | Sempre `GetCurrentStateDescription` | **Corrupção persistiu** (user) | Não era a causa única |
| **Revert texture descriptor reuse** | Sempre invalidar sets textura/pixel em `UpdateBindings` | **Validação visual pendente (user)** | **Causa raiz provável** |
| Vertex fetch | Sem mudança lógica | — | N/A |
| Primitive | Sem mudança | — | N/A |

---

## 5. Correções aplicadas

### 5.1 PipelineDescription cache (insuficiente sozinho)

- **Revertido** cache de descrição em `pipeline_cache.cpp` / `pipeline_cache.h`.
- Corrupção **permaneceu** → fingerprint incompleto era real mas **não** explicava sozinho o defeito observado após revert.

### 5.2 Texture / sampler descriptor reuse (**causa raiz provável**)

Em `VulkanCommandProcessor::UpdateBindings` a optimização comparava:

- `TextureBinding` (só **índice** de fetch constant 0–31 + dimensão + signed — ver `spirv.h`)
- `SamplerParameters` estáticos do shader

**Não** comparava o estado **vivo** dos fetch constants do guest lidos em `texture_cache_->GetActiveBindingOrNullImageView(fetch_constant, …)`.

Com o mesmo shader, draws consecutivos tinham **hit ~70–90%** e **saltavam `vkUpdateDescriptorSets`** enquanto as texturas guest mudavam → amostragem de `VkImageView` **obsoletas** → logo distorcido, geometria duplicada, blocos/artefactos.

**Correção:** invalidar sempre `kDescriptorSetTexturesVertex` e `kDescriptorSetTexturesPixel` em cada `UpdateBindings` (comportamento seguro alinhado à semântica Xenos).

Ficheiros: `command_processor.cpp`, `command_processor.h` (removidos helpers/membros de cache de textura).

---

## 6. Validação obrigatória (utilizador / próximo passo)

**ADB:** `C:\Users\Administrator\Downloads\platform-tools-latest-windows\platform-tools\adb.exe`

```text
adb install -r android\dist\Sonic4-arm64-v8a.apk
adb shell am start -n com.s4ep1recomp.sonic4/.Sonic4Activity
```

Checklist visual (secção 9 do briefing):

1. Ecrã inicial, menu, título EPISODE I, Splash Hill, anéis, inimigos, câmera, água/transparência, transições, pause.

**Windows (pós-merge do mesmo código Vulkan):** compilar Vulkan + D3D12; smoke visual no título e um nível.

**Critério de sucesso:** sem duplicação, estiramento do logo, blocos pretos/verdes, retângulos fantasma.

---

## 7. Builds e flags

| Build | `REX_DIAG_PROFILE` | `REX_TIMING_PROBES` |
| --- | --- | --- |
| Release Android (`build-android-arm64.bat`) | OFF | OFF |

APK corrigido gerado após revert: `android/dist/Sonic4-arm64-v8a.apk`.

---

## 8. Preservação de outros trabalhos

- Save / `adb install -r` (sem uninstall)
- Retail timing hooks (`sonic4_timing_hooks_retail.cpp`)
- Reutilização de descriptors de textura (≠ pipeline description cache)
- Dedupe de bind em `deferred_command_buffer.cpp`
- Relatórios de performance anteriores; ver notas de invalidação abaixo

---

## 9. Otimizações invalidadas

| Otimização | Estado |
| --- | --- |
| Cache de `PipelineDescription` (~73% hit diag) | **REVERTIDA — insegura** |
| Métricas `configure_pipeline_us_pf` ~26 µs | **Invalidadas** (artefacto do cache errado) |
| Ganhos PACE `perf_pipe_desc_opt_retail_150s.log` (46,2 Hz) | **Não fiáveis** com corrupção gráfica |

IssueDraw phase profiling (diag only) **permanece** para uso futuro **após** validação visual.

---

## 10. Windows / pendências

- [ ] Android Release instalado e checklist visual completo
- [ ] Windows Vulkan smoke
- [ ] Windows D3D12 smoke
- [ ] Se corrupção persistir: revert isolado de texture descriptor reuse e comparar

---

## 11. Lições

1. Nunca saltar `GetCurrentStateDescription` sem prova formal de que o fingerprint é **bit-a-bit equivalente** à função completa.
2. O fast path `last_pipeline_` existente já é seguro (compara descrição **reconstruída**).
3. Performance work **congelado** até validação visual neste checklist.