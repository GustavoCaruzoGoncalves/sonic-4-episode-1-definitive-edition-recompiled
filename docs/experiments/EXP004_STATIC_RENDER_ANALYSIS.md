# Exp-004 — Análise de renderização em cena “estática”

**Data:** 2026-11-17  
**Referência:** Exp-003a FIFO (`raw/power/exp003a_fifo/`)  
**Experimento em curso:** Exp-004 (`build-android-arm64-exp004.bat`)

---

## 1. Causa raiz (suportada por evidência)

### 1.1 O jogo **não** está visualmente estático

Sonic parado na 1.ª fase **não** implica framebuffer inalterado. Medições históricas com `REX_DIAG_PROFILE` no POCO (`evidence/ANDROID_PORT/raw/poco/sonic4_033.log`) mostram, em gameplay sustentado:

| Métrica | Valor típico (1.ª fase / parado) |
|---------|----------------------------------|
| `present_hz` (guest) | ~40–42 Hz (alinhado a PACE Exp-003a) |
| `draws_per_present_avg` | **~236–318** |
| `record_us_pf` (Vulkan CPU) | **~5,4–7,5 ms**/present |
| `gpu_exec_us_avg` | **~1,5–1,9 ms** |
| `barriers/s` | **~8k–13k** |
| `rendering_begins/s` | **~4k–5k** |

**Conclusão:** o guest continua a emitir **centenas de draws por frame** — animações (água, HUD, timers, partículas), actualizações de cena e áudio/timing seguem activos. **Não é seguro** aplicar frame-skip global só porque o jogador não se move.

### 1.2 Onde o trabalho se divide (Exp-003a)

| Subsistema | Evidência Exp-003a | Peso |
|------------|-------------------|------|
| **Guest main loop** | PACE **37,4 Hz** médio; `sub_821C62C8` **7,27 %**, `sub_821C4870` **5,74 %** (simpleperf) | CPU: simulação + polling a ~40 Hz |
| **Vulkan record/submit** | `gpubusy ~70,6 %`; GPU **644 MHz** mediana | CPU+GPU: preparação e execução por present guest |
| **Apresentação FIFO** | Modo 2 confirmado; **não** reduziu draws nem gpubusy vs MAILBOX | Sincronização swapchain apenas |
| **Presenter swapchain** | Pass final com `LOAD_OP_CLEAR` por defeito em todas as plataformas | Clear GPU extra no blit para o ecrã (candidato opt seguro) |

Ficheiros críticos:

- Guest loop / PACE: `pilot/src/sonic4_timing_hooks_retail.cpp` (`sub_8249E2F8`)
- IssueSwap / gamma-FXAA-resolver: `third_party/rexglue-sdk/src/graphics/vulkan/command_processor.cpp` (`IssueSwap`, `OnGuestPresentFrame`)
- Draw path: `VulkanCommandProcessor::IssueDraw` (ver `ISSUE_DRAW_ROOT_CAUSE.md`)
- Execução deferred CB + dedup bind: `deferred_command_buffer.cpp`
- Swapchain UI pass: `third_party/rexglue-sdk/src/ui/vulkan/vulkan_presenter.cpp`

### 1.3 O que **não** é o problema principal

- Modo Vulkan present (Exp-003a).
- Interpretador PPC (AOT confirmado).
- Falta de dedup de pipeline/descriptor no deferred CB (já activo; `bind_skip_*` contável em diag).

---

## 2. Exp-004 — diagnóstico controlado

### Build

`build-android-arm64-exp004.bat`:

- `REX_RENDER_TELEMETRY=ON` — `REX_FRAME_STATS` no SDK **sem** scopes `REX_IDRAW_PHASE`.
- `REX_TIMING_PROBES=OFF` — hooks retail (baixo overhead vs diag completo).
- Log: `PROBE RENDER_VK` junto com `PROBE PACE` (~5 s).

### O que medir

| Métrica | Fonte |
|---------|--------|
| Simulação guest | `PACE_821C0770`, `frame_us` |
| Carga render | `draws_per_present`, `record_us_pf`, `barriers/s` |
| GPU | `kgsl` freq, `gpubusy`, `gpu_exec_us_avg` no log |
| Energia | `current_now` / `voltage_now` — **só com AC desligado** |

Artefactos: `raw/power/exp004_static_render/`

---

## 3. Exp-004-opt1 (implementado)

**Mudança:** `present_render_pass_clear` default **`false` no Android** (`vulkan_presenter.cpp`). O quad de composição cobre o swapchain; `DONT_CARE` evita clear redundante na pass de apresentação.

**Não é:** skip de frames guest, pausa de simulação, nem modo diagnóstico.

**Validação visual:** obrigatória após install; regressões gráficas → reverter default.

---

## 4. Comparação esperada vs Exp-003a

| | Exp-003a | Exp-004 (alvo) |
|---|----------|----------------|
| Presentation | FIFO | FIFO (inalterado) |
| PACE | ~37–40 Hz | ~igual (sem throttle artificial) |
| `draws_per_present` | ~n/a (retail) | **log directo** |
| gpubusy / corrente | AC ligado | **bateria** para A/B sério |

---

## 5. Limitações

- Exp-003a medido **com AC** — não comparar corrente directamente.
- Telemetria Exp-004 adiciona atomics por draw (~overhead menor que diag completo, não zero).
- Cena “parada” ainda contém **centenas de draws/frame** — optimizações futuras têm de atacar **workload guest** ou **IssueDraw hot path** (`vertex_fetch`, `primitive`), não só presenter.

---

## 6. Próximos experimentos (prioridade por evidência)

1. **Medir Exp-004** na 1.ª fase parada, **AC off**, 120 s vs Exp-003a (gpubusy, PACE, `RENDER_VK`).
2. Se opt1 não mover gpubusy: **reverter** clear default; candidato seguinte = redução segura em `vertex_fetch` / residency (`ISSUE_DRAW_ROOT_CAUSE.md`), **um patch de cada vez**.
3. **Não** implementar skip de present até existir prova de output guest byte-estável (hash/captura) **e** preservação de áudio/timers.

---

*Exp-003a build preservado via `build-android-arm64.bat` (sem telemetria, clear presenter default anterior em non-Android; Android exp003 usava clear=true).*