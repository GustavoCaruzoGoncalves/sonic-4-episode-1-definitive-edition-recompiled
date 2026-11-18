# GPU, Vulkan e áudio — análise combinada

**Baseline log:** `perf_post_initfix_150s.log` / `VULKAN_AUDIO_PROFILING.md` (diag ON).  
**Data:** 2026-11-17.

---

## 1. Vulkan — CPU vs GPU

### 1.1 CPU (medido)

| Regime | present_hz | draws/present | desc_binds/s | barriers/s | record_us_avg | submit_us_avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 60 Hz | 59,7 | 180,9 | 23569 | 7083 | **2665** | 550 |
| 26 Hz | 25,9 | **306,2** | 16799 | 6848 | **8370** | 626 |

- **~45–50 µs/draw** implícito em record (8370/306 ≈ 27 µs só se linear; inclui estado/barriers).
- `submit_us_avg` < 1 ms — **não** é o gargalo principal.
- **20k+ descriptor binds/s** em carga — candidato a deduplicação / cache de layouts (compare com D3D12/Vulkan Windows).

### 1.2 GPU (antes e depois 17/11)

| Estado | Evidência |
| --- | --- |
| Antes | `gpu_exec_ns=na` — proibido concluir bound GPU |
| ADB `gpu_busy` 66–77%, 644 MHz | Indício de uso, não tempo de frame |
| **17/11 diag** | `VK_QUERY_TYPE_TIMESTAMP` start/end do command buffer de present → `gpu_exec_us_avg` em `PROBE DIAG_VK` |

**Interpretação pendente:** capturar no POCO F7 com `build-android-arm64-diag.bat`. Se `gpu_exec_us_avg` << `record_us_avg`, otimizar Vulkan CPU; se >>, shader/ROB.

---

## 2. Por que draws/present dispara quando PACE cai

Hipótese suportada pelo log:

1. Frame guest **atrasado** → acumula trabalho gráfico do intervalo (mais draws por apresentação).  
2. Mesmo título no Windows mantém 60 Hz com **mais** `SCENEDISP/s` — o host Android demora a **consumir** o frame budget.

Não confundir com “overdraw do jogo”: é **throughput de pipeline** host.

---

## 3. Áudio

### 3.1 Medições

| Métrica | Valor típico (gameplay) | Leitura |
| --- | ---: | --- |
| `sdl_cb/s` | ~187 | Taxa fixa buffer SDL |
| `cb_us_avg` | 11–20 µs | Mix host barato |
| `underrun/s` | **0** | Fila SDL não esvazia cronicamente |
| `worker_disp_us_avg` | **1300–1660 µs** | Guest audio path caro |
| `worker_disp/s` | ~187 | Acoplado ao relógio guest |

### 3.2 Mecanismo do stutter audível (sem underrun)

1. PACE guest < 60 → menos períodos de áudio “a tempo”.  
2. Worker compete com XThread (~80%+ CPU).  
3. Samples chegam **irregularmente** à SDL mesmo com `queue_depth_max=8` — micro-gaps perceptíveis.

**Não** priorizar aumento cego de buffer (mascara, não corrige).

### 3.3 Próximas medições áudio

| Diagnóstico | Implementação sugerida |
| --- | --- |
| Starvation **upstream** da SDL | Contador samples produzidos vs consumidos por período guest (diag) |
| Drift guest vs host | Correlacionar `PROBE GUESTCLK` com `worker_disp/s` |
| Android backend | Log AAudio vs OpenSL policy (SDL build) |

---

## 4. Tabela experimental (template A/B)

Preencher após validação POCO F7:

| Métrica | Baseline diag ON (`perf_post_initfix`) | Perf OFF (17/11) | Δ |
| --- | ---: | ---: | ---: |
| Guest Hz médio | 39,6 | TBD | |
| P95 frame_us | TBD | TBD | |
| record_us_avg (pesado) | 8370 | TBD | |
| gpu_exec_us_avg | na → TBD diag | TBD | |
| underrun/s | 0 | TBD | |
| worker_disp_us_avg | 1500 | TBD | |

---

## 5. Comparativo Windows

- Windows Vulkan: `FRAMEUPD dur_us` **1,7–6,5 ms** típico com `SCENEDISP` até **1895/s**.  
- Android degradado: **34 ms** `FRAMEUPD` com menos dispatch e **menos** draws totais/s.

**Conclusão:** o backend Vulkan Android compartilha código com desktop, mas roda em **orçamento CPU guest pior** e possivelmente com **maior draws/present** sob atraso — dupla penalidade.