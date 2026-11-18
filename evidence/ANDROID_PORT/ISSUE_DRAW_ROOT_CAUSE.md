# IssueDraw — causa raiz e otimização (POCO F7)

**Data:** 2026-11-17  
**Dispositivo:** POCO F7 `a63a397c`  
**Build diag:** `build-android-arm64-diag.bat` (`REX_DIAG_PROFILE=ON`)  
**Build Release:** `build-android-arm64.bat` (`REX_DIAG_PROFILE=OFF`, `REX_TIMING_PROBES=OFF`)

---

## 1. Resumo executivo

O custo agregado **`issue_draw_us_pf`** continua dominante (~**8,7 ms**/present em carga pesada), mas a decomposição por fase mostra que **não** é `ConfigurePipeline` / `GetCurrentStateDescription` o gargalo principal neste título após a otimização de cache de descrição de pipeline.

| Rank | Fase (exclusiva) | µs/present (pesado) | % de IssueDraw† |
| ---: | --- | ---: | ---: |
| 1 | `vertex_fetch` (residência VB + `GetVertexFetch` / `RequestRange`) | **3362** | **38,5%** |
| 2 | `primitive` (`PrimitiveProcessor::Process`) | **2209** | **25,3%** |
| 3 | `shader_prep` (análise ucode, memexport ranges) | **785** | **9,0%** |
| 4 | `bindings` (`UpdateBindings`) | **537** | **6,2%** |
| 5 | `textures` (`RequestTextures`) | **298** | **3,4%** |

† Percentagens sobre `issue_draw_us_pf` médio em janelas pesadas (`issue_draw_us_pf ≥ 7000`), log `perf_issue_draw_diag_120s.log` (`sonic4_033.log`).

**Correção implementada (Release):** cache seguro de `PipelineDescription` em `VulkanPipelineCache::ConfigurePipeline` — evita `GetCurrentStateDescription` quando shaders, primitive key, depth/mask, `render_pass_key` e fingerprint de registros de pipeline são idênticos. **Hit rate diag ≈ 73%**; fase `configure_pipeline_us_pf` cai para **~26 µs/present** (pesado).

**Release 150 s (mesma sessão, save preservado):** `perf_pipe_desc_opt_retail_150s.log` — PACE **46,2 Hz** médio (`frame_us` **18,5 ms**), trechos pesados **~39,8 Hz**. Baseline retail validado: **45,9 Hz** (`perf_retail_hooks_150s.log`). Diferença **+0,3 Hz** na média global — **não conclusivo** (dispositivo quente após diag; revalidação fria recomendada).

---

## 2. Grafo de chamadas — `VulkanCommandProcessor::IssueDraw`

Ordem real (simplificado):

```
IssueDraw
├── [shader_prep] shader/memexport prep, depth mask, interpolators
├── loop (sampler overflow ×2):
│   ├── BeginSubmission
│   ├── [primitive] PrimitiveProcessor::Process
│   └── [shader_sampler] modificação shader, tradução, UseSampler
├── [textures] TextureCache::RequestTextures
├── [render_target] RenderTargetCache::Update
├── [configure_pipeline] PipelineCache::ConfigurePipeline
│   ├── EnsureShadersTranslated
│   ├── GetCurrentStateDescription (omitido se cache de descrição hit)
│   ├── last_pipeline_ fast path / pipelines_.find / criação PSO
├── GetPipelineAndLayoutByHandle (async placeholder)
├── [pipeline_bind] CmdVkBindPipeline + layout/descriptor invalidation
├── GetHostViewportInfo
├── [dynamic_state] UpdateDynamicState
├── [system_constants] UpdateSystemConstantValues
├── [bindings] UpdateBindings (+ diag: vkUpdateDescriptorSets)
├── [vertex_fetch] loops vertex_fetch_bitmap → GetVertexFetch → SharedMemory::RequestRange
├── [memexport_sync] memexport ranges, guest DMA index copy (raros)
├── [barriers_rp] SharedMemory::Use + SubmitBarriersAndEnterRenderTargetCacheRenderPass
├── [deferred_draw] CmdVkDraw / CmdVkDrawIndexed (+ bind index)
└── [memexport_post] RangeWrittenByGpu, readback opcional
```

---

## 3. Metodologia de medição

- **Fases exclusivas:** `REX_IDRAW_PHASE` + `PROBE ISSUE_DRAW_BREAKDOWN` (só `REX_DIAG_PROFILE`).
- **Agregado IssueDraw:** scope total existente → `issue_draw_us_pf` em `PROBE DIAG_VK_BREAKDOWN`.
- **Ferramentas:** `tools/summarize_issue_draw_breakdown.py`, `tools/summarize_vk_breakdown.py`.
- **Sem double-count:** cada fase em bloco `{}` com RAII; soma das fases ≈ total (diferença residual = trabalho fora dos blocos ou retornos antecipados).

---

## 4. Tabela de custo por operação (diag, pesado)

Fonte: **28 janelas**, **23 pesadas** (`perf_issue_draw_diag_120s.log`).

| Fase | µs/present (médio pesado) | µs/present (leve) | Notas |
| --- | ---: | ---: | --- |
| **issue_draw (total)** | 8721 | 2756 | `DIAG_VK_BREAKDOWN` |
| shader_prep | 785 | 422 | `AnalyzeShaderUcode`, memexport |
| primitive | 2209 | 454 | guest index/VB |
| shader_sampler | 91 | 44 | tradução + `UseSampler` |
| textures | 298 | 144 | `RequestTextures` |
| render_target | 100 | 94 | `RenderTargetCache::Update` |
| **configure_pipeline** | **26** | 13 | após cache descrição |
| pipeline_bind | 0 | 0 | quase sempre cache hit interno |
| dynamic_state | 1 | 1 | |
| system_constants | 2 | 1 | |
| bindings | 537 | 164 | uniforms + tex descriptors |
| **vertex_fetch** | **3362** | 905 | **maior custo** |
| memexport_sync | 0 | 0 | |
| barriers_rp | 8 | 2 | |
| deferred_draw | 1 | 0 | só gravação no stream |
| memexport_post | 51 | 18 | |

**Replay / driver (inalterado neste patch):** `record_us_pf` ainda **~6–7 ms** em pesado na mesma sessão.

---

## 5. Causa raiz por gargalo

### 5.1 Vertex fetch (38%)

- Por draw: varre `vertex_fetch_bitmap`, `regs.GetVertexFetch`, e **`shared_memory_->RequestRange`** quando o bit de sync não está setado.
- Custo é **CPU aplicacional + acesso memória guest** (não `vkCmd*`).
- Windows no mesmo código paga menos por GHz e menor pressão no modelo de memória guest.

### 5.2 PrimitiveProcessor (25%)

- `Process()` por draw: conversão de índices/primítivos, possível guest DMA.
- Correlação com `draws_per_present` alto em carga pesada.

### 5.3 ConfigurePipeline (resolvido parcialmente)

- **Antes:** `GetCurrentStateDescription` a cada draw mesmo com `last_pipeline_` hit posterior.
- **Depois:** fingerprint + cópia da última `PipelineDescription` — **~73%** skips de rebuild.
- Restante custo em miss: lookup `pipelines_` / criação PSO (raro: `pso_create/s` baixo).

### 5.4 Bindings (6%)

- Já existe reuse de texture descriptors (~70% hit em janelas anteriores); uniform/CB paths permanecem.

---

## ⚠️ REGRESSÃO GRÁFICA (17/11) — cache revertido

A otimização de **cache de `PipelineDescription`** (secção 6 abaixo) causou corrupção grave no POCO F7 (geometria duplicada, logo distorcido, artefactos de RT). **Revertida** — ver `GRAPHICS_REGRESSION_INVESTIGATION.md`. **Não reintroduzir** sem fingerprint equivalente a `GetCurrentStateDescription` completo.

---

## 6. Otimização implementada (REVERTIDA — NÃO USAR)

**Arquivo:** `pipeline_cache.cpp` / `pipeline_cache.h`

- `ComputePipelineConfigurationRegisterHash()` — `PA_SU_SC_MODE_CNTL`, `PA_CL_CLIP_CNTL`, `RB_BLENDCONTROL` por RT ativo.
- Snapshot de inputs + `last_configured_description_`.
- Telemetria: `pipe_desc_cache_hit` / `pipe_desc_cache_miss` em `PROBE ISSUE_DRAW_BREAKDOWN`.
- **Release:** mesmo caminho sem contadores (`OnPipelineDescriptionCache*` → no-op).

**Invalidação:** clear do pipeline cache, mudança de hash de registros, shaders, `render_pass_key`, primitive key, depth/mask.

---

## 7. Antes / depois (medição)

| Métrica | Antes (vk breakdown legado) | Depois (esta sessão) |
| --- | ---: | ---: |
| `configure_pipeline` (fase exclusiva, pesado) | incluído no agregado ~8,8 ms, sem split | **~26 µs/pf** |
| `issue_draw_us_pf` (pesado) | **~8800** | **~8721** (sessão quente; variância térmica) |
| `GetCurrentStateDescription` / draw | ~100% | **~27%** (miss rate) |
| PACE Release 150 s | **45,9 Hz** (baseline frio) | **46,2 Hz** (sessão pós-diag) |

O ganho de **IssueDraw** na média agregada ainda é pequeno porque **vertex_fetch + primitive** dominam; o cache de pipeline remove trabalho redundante sem alterar o caminho crítico restante.

---

## 8. ARM64 / guest

- Hotspots de espera (`821C62C8`) continuam sintoma de frame atrasado, não causa primária de `IssueDraw`.
- `vertex_fetch` e `primitive` implicam **acesso frequente à memória guest** — alinhado com custo extra vs Windows no mesmo backend Vulkan.

---

## 9. Próximos alvos (prioridade medida)

1. **Vertex fetch:** reduzir `RequestRange` / batelar residência; revisar invalidação de `vertex_buffers_in_sync_`.
2. **PrimitiveProcessor:** cache de resultado quando draw/index inalterados (cuidado com semântica Xenos).
3. **Shader prep:** `AnalyzeShaderUcode` já curto-circuita se analisado — avaliar custo de `GetInterpolatorInputMask`.

**Evitar:** `GetActiveBinding` ou lookups caros **por draw** no hot path (lição do cache de descriptors).

---

## 10. Regressão / entregáveis

- Windows / Android Release: sem macros de diag em hot path.
- Logs: `perf_issue_draw_diag_120s.log`, `perf_pipe_desc_opt_retail_150s.log`.
- Relatórios atualizados: `VULKAN_CPU_RECORDING_INVESTIGATION.md` (referência cruzada).

---

## 11. Critério de sucesso

Meta **60 Hz** sustentado **não atingida**. Bottleneck remanescente quantificado: **~3,4 ms + ~2,2 ms** em vertex fetch + primitive só em IssueDraw (pesado), mais **~6 ms** de replay diferido fora de IssueDraw.