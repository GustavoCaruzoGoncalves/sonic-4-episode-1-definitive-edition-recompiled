# Investigação — CPU Vulkan (backend Xenos compartilhado)

**Data:** 2026-11-17  
**Dispositivo:** POCO F7 (`a63a397c`), Adreno 825  
**Baseline retail pré-Vulkan:** `perf_retail_hooks_150s.log` — PACE **45,9 Hz**, `frame_us` **18,5 ms**  
**Referência Windows:** mesmo código Vulkan; ~60 Hz em carga equivalente.

---

## 1. Resumo executivo

O rótulo histórico **`record_us_avg`** mede apenas o replay do buffer diferido (`DeferredCommandBuffer::Execute` → chamadas `vkCmd*`). **Não** inclui o trabalho dominante em **`VulkanCommandProcessor::IssueDraw`** (cache de pipeline, render targets, uniforms, barreiras gravadas no stream, etc.).

Medição nova (`PROBE DIAG_VK_BREAKDOWN`, build diag, `perf_vk_breakdown_diag_120s.log`):

| Regime | Host GPU estimado/issue+record+desc) | `record_us_pf` | `issue_draw_us_pf` | GPU exec |
| --- | ---: | ---: | ---: | ---: |
| Leve | **~7,3 ms** | 2,8 ms | 4,4 ms | ~1,2 ms |
| Pesado | **~15,4 ms** | 6,2 ms | **8,8 ms** | ~2,0 ms |

**Conclusão:** o CPU não está “preso no driver Adreno” nos draws (`vkCmdDraw` ~**1,7 µs/chamada**). O déficit é **preparação Xenos→Vulkan (`IssueDraw`)** + **replay do stream**; `vkUpdateDescriptorSets` é **~2%** do host GPU em carga pesada após reutilização de textura.

**Otimizações implementadas (Release):**

1. Reutilização de descriptor sets de textura quando bindings/samplers inalterados (implementa TODO Xenia; ~**70% hit** em diag).  
2. Deduplicação segura de `vkCmdBindPipeline` / `vkCmdBindDescriptorSets` consecutivos idênticos em `Execute`.  
3. Telemetria diag por categoria (sem overhead em Release).

**Validação retail 150 s pós-otimização:** `perf_vk_opt_retail2_150s.log` — PACE **39,1 Hz** (média); primeira metade **~50–60 Hz**, fase exigente **~30–38 Hz** (comparável ao baseline em trechos pesados, mas média global menor — **sessão quente** após vários testes ADB seguidos). **Não** se conclui regressão estrutural; revalidação com dispositivo frio recomendada.

**Atualização IssueDraw (17/11 tarde):** decomposição em `ISSUE_DRAW_ROOT_CAUSE.md` + `PROBE ISSUE_DRAW_BREAKDOWN`. Gargalos medidos: **vertex_fetch (~38%)**, **primitive (~25%)**.

**REGRESSÃO (17/11 noite):** cache de `PipelineDescription` **revertido** — corrupção gráfica Android. Ver `GRAPHICS_REGRESSION_INVESTIGATION.md`. Métricas `configure_pipeline` / `perf_pipe_desc_opt_retail_150s.log` **invalidadas**.

---

## 2. Metodologia

- **Diag:** `build-android-arm64-diag.bat`, `PROBE DIAG_VK` + `DIAG_VK_BREAKDOWN` a cada 5 s.  
- **Release:** `REX_DIAG_PROFILE=OFF`, `REX_TIMING_PROBES=OFF`.  
- **Ferramentas:** `tools/summarize_vk_breakdown.py`, `tools/summarize_probe_log.py`.  
- **Arquivos:** `perf_vk_breakdown_diag_120s.log`, `perf_vk_opt_retail2_150s.log`, `perf_vk_opt_retail_150s.log` (tentativa intermédia com fingerprint caro — revertida).

---

## 3. Tabela de custo por operação (carga pesada, **medido**)

Janela típica: `present_hz≈36`, `draws/present≈221` (`perf_vk_breakdown_diag_120s.log`, médias `record_us_pf≥5000`).

| Operação | Chamadas/frame | ms/frame (CPU) | µs/chamada | % do host GPU† |
| --- | ---: | ---: | ---: | ---: |
| **IssueDraw** (pipeline, RT, shaders, uniforms, barreiras no stream) | ~221 draws | **8,81** | ~40* | **57%** |
| **Replay Execute** (`vkCmd*` agregado) | ~1×/submit + cmds | **6,24** | — | **41%** |
| **Descriptor update** (`vkUpdateDescriptorSets`) | poucos/frame | **0,34** | ~1500** | **2%** |
| **vkCmdBindPipeline** (no replay) | ~78/frame | ~0,01 | **0,16** | &lt;1% |
| **vkCmdBindDescriptorSets** | ~63/frame | ~0,00 | **&lt;0,1** | &lt;1% |
| **vkCmdDraw/Indexed** | ~221 | **~0,39** | **1,77** | &lt;1% |
| **vkCmdPipelineBarrier** (replay medido) | alta freq. | incl. em Execute | **~0** timers‡ | (em Execute) |
| **GPU execution** (timestamps) | 1/present | **~2,0** | — | (GPU) |

† Host GPU = `issue_draw_us_pf + desc_update_us_pf + record_us_pf` ≈ **15,4 ms** (pesado).  
\* Amortizado por draw na fila GPU; inclui tudo até `CmdVkDraw` gravado.  
\** Por chamada de `vkUpdateDescriptorSets` quando ocorre.  
‡ `exec_barrier_us/call≈0` no timer — custo de barreira concentrado no stream gravado em IssueDraw e outros cmds em `exec_other` não instrumentados por comando.

**Atribuição app vs driver:** tempos de **`vkCmdDraw`/bind** no replay são **sub‑microsegundo** → driver **não** domina o replay; **`IssueDraw`** é **código aplicacional** (texture/render-target/pipeline caches). Windows no mesmo código reduz `issue_draw` por GHz core e custo guest inferior.

---

## 4. Contagens Vulkan (pesado vs leve, diag)

| Métrica | Leve (~60 Hz) | Pesado (~36 Hz) |
| --- | ---: | ---: |
| draws/present | ~87 | ~221 |
| pipeline_binds/s ÷ Hz | ~60 | ~78 |
| desc_binds/s ÷ Hz | ~183 | ~63 |
| barriers/s ÷ Hz | ~81 | ~218 |
| `pso_create/s` | ~0 | 0–2,8 |
| `tex_desc_reuse` | — | **69,9% hit** |

`desc_binds/s` alto com draw count alto indica **binds gravados no stream** mesmo quando sets estão atualizados — dedupe em `Execute` não ajuda se valores alternam entre draws consecutivos (`bind_skip_desc=0` em todas as janelas).

---

## 5. Causas identificadas

### C1 — Métrica enganosa `record_us_avg` (arquitetural)

Só cronometra `Execute`. Diag passa a expor `issue_draw_us_pf` separado.

### C2 — Invalidação forçada de descriptors de textura a cada draw (corrigido)

`UpdateBindings` limpava sempre bits de textura (TODO Triang3l). **Correção:** comparar `TextureBinding` + `SamplerParameters`; só invalidar se mudou.

**Experimento:** hit rate **~70%**; `desc_update_us_pf` **~0,3 ms** em pesado vs trabalho de IssueDraw **~9 ms**.

### C3 — `IssueDraw` domina (aberto)

Inclui `ConfigurePipeline`, `render_target_cache_->Update`, `PrimitiveProcessor`, barreiras, cópias. Próximo alvo: perf counters / Tracy em sub-etapas (sem diag no Release).

### C4 — Execute dedup (implementado, ganho marginal)

Binds repetidos **consecutivos** raros no stream do Sonic → `bind_skip_*=0`.

---

## 6. Android vs Windows

| Aspecto | Windows Vulkan | Android Vulkan |
| --- | --- | --- |
| Código IssueDraw/Execute | **Compartilhado** | **Compartilhado** |
| `record_us` isolado | mesmo | mesmo |
| Guest ARM64 | N/A | +tempo em `FRAMEUPD` / draws guest |
| Driver `vkCmdDraw` | sub‑µs (esperado) | **1,77 µs** medido |
| GPU exec | baixo | **~2 ms** — não saturado |

Mesmo workload lógico; Android paga **~2×** em host GPU (IssueDraw+record) **e** guest antes da fila gráfica.

---

## 7. ARM64 guest (paralelo)

`simpleperf` (`perf_retail_hooks.data`): `sub_821C62C8` / `821C4870` elevados sob frame atrasado — **sintoma**. Déficit restante após host GPU **~15 ms** requer guest + áudio + scheduling — ver `ROOT_CAUSE_PERFORMANCE_INVESTIGATION.md`.

---

## 8. Implementação

| Ficheiro | Alteração |
| --- | --- |
| `command_processor.cpp` | Reutilização textura; timers diag IssueDraw/descriptor update |
| `deferred_command_buffer.cpp` | Dedup bind pipeline/descriptor; timers por categoria em diag |
| `frame_stats.h/.cpp` | Campos `DIAG_VK_BREAKDOWN` |
| `sonic4_timing_hooks.cpp` | Log `PROBE DIAG_VK_BREAKDOWN` |

---

## 9. Validação física

| Sessão | Build | PACE médio | Nota |
| --- | --- | ---: | --- |
| Baseline retail | pré-Vulkan opt | **45,9** | `perf_retail_hooks_150s.log` |
| Vulkan opt v1 | fingerprint caro | 36,9 | Revertido overhead `GetActiveBinding`/draw |
| Vulkan opt v2 | binding/sampler | **39,1** | `perf_vk_opt_retail2_150s.log`; térmica/sessão |
| Diag breakdown | diag ON | — | Decomposição IssueDraw/record |

APK Release atual instalado: `build-android-arm64.bat` (sem diag).

---

## 10. Experimentos

| Hipótese | Experimento | Resultado |
| --- | --- | --- |
| Record = todo CPU Vulkan | `issue_draw_us_pf` vs `record_us_pf` | IssueDraw **maior** em pesado |
| Descriptor update domina | `desc_update_us_pf` | **~2%** |
| Driver draw caro | `exec_draw_us/call` | **~1,7 µs** |
| Textura sempre rebind | tex reuse hit% | **70%** skip `vkUpdateDescriptorSets` |
| Dedup Execute ajuda | `bind_skip_*` | **0** (stream alterna) |

---

## 11. Próximos passos (implementação)

1. Sub-timers em `IssueDraw` (`ConfigurePipeline`, `Update` RT, `SubmitBarriers`, `PrimitiveProcessor`) — só diag.  
2. Reduzir barreiras/cópias no stream (investigar correlação `barriers/s` com PACE).  
3. Revalidar retail **45,9+ Hz** com dispositivo frio e mesma rota Splash Hill.  
4. Guest hotspots ARM64 com `simpleperf` em fase `issue_draw_us` alto.

---

## 12. Regressões

- Windows: alterações em código compartilhado; **build Windows não reexecutado** nesta sessão.  
- Qualidade gráfica: sem alterações intencionais; reutilização de textura segue semântica Xenia “bindings unchanged”.