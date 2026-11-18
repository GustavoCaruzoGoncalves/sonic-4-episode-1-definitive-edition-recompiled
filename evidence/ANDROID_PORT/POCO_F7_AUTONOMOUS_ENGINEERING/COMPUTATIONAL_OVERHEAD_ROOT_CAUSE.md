# Causa raiz do overhead computacional — uma instância Sonic 4 (POCO F7)

**Data:** 2026-11-17  
**Objectivo corrigido:** eficiência **por instância** (CPU/GPU/memória/sincronização por frame de trabalho útil), **não** “forçar 60 FPS”.  
**Cena de referência:** 1.ª fase, Sonic parado, ARM64 AOT, Vulkan FIFO (Exp-003a).  
**Dispositivo:** POCO F7, Snapdragon 8s Gen 4, Adreno 825.

---

## Resumo executivo

| Pergunta | Resposta (com evidência) |
|----------|---------------------------|
| Há interpretação PPC no CPU? | **Não** — ver `CPU_EXECUTION_ARCHITECTURE_AUDIT.md`. |
| Porque parece um AAA no telemóvel? | **Três pilares:** (1) guest ARM64 a ~**40–46 loops/s** com centenas de draws/frame mesmo parado; (2) **5–9 ms**/present de gravação Vulkan no CPU; (3) GPU **~70–74 %** ocupada a ~**644 MHz** por conteúdo animado + pipeline completo. |
| O gargalo é “frequência GPU”? | **Não como causa raiz** — `gpu_exec_us_avg` típico **0,8–2,6 ms** (diag); o SoC fica ocupado porque **volta a submeter trabalho** ~40×/s. |
| O que escala por instância? | `libmain.so` guest, threads `XThread*`, worker GPU, gravação Vulkan, EDRAM guest, áudio SDL — **cada APK = cópia quase completa do runtime**. |
| O que já está provado? | Remover `REX_DIAG_PROFILE` e `REX_TIMING_PROBES` no retail (~**+6–12 %** PACE medido historicamente, menos atomics). |

**Não provado (evitar como “facto”):** que FIFO, clear de swapchain, ou flags isoladas resolvam o footprint; que Sonic parado = 0 trabalho visual.

---

## 1. Orçamento por frame (trabalho computacional)

### 1.1 Taxa guest (simulação + apresentação lógica)

| Fonte | Métrica | Valor |
|-------|---------|-------|
| Exp-003a FIFO (AC) | PACE médio | **37,4 Hz**, mediana **38,75 Hz** |
| Exp-004 (bateria, `sonic4_052`) | PACE médio | **41,8 Hz**, mediana **42,5 Hz** |
| Exp-004 | `frame_us` médio | **~20,0 ms** (~**50 ms** período efectivo incluindo variância) |

O guest **não** entra em idle com Sonic parado: o main loop (`sub_8249E2F8` + pacing) continua a ~**40 Hz**.

### 1.2 Decomposição dinâmica (build diag, mesma classe de cena)

Fonte: `ROOT_CAUSE_PERFORMANCE_INVESTIGATION.md`, `perf_diag_gpu_90s.log`, `sonic4_033.log`.

| Componente | Cena “leve” | Cena pesada / 1.ª fase sustentada |
|------------|-------------|-----------------------------------|
| `present_hz` (Vulkan guest presents) | ~59,7 | **~27–42** |
| **`draws_per_present`** | ~102 | **~223–346** |
| **`record_us_avg`** (CPU Vulkan) | **~2,6 ms** | **~6,5–9,5 ms** |
| **`gpu_exec_us_avg`** | **~0,8 ms** | **~1,9–2,6 ms** |
| `issue_draw_us_pf` | ~1,3 ms | **~8–9 ms** |

**Interpretação:** o frame domina **CPU record + IssueDraw guest/host**, não saturação de GPU por tempo de execução longo. Isto explica consumo “de jogo pesado” sem FPS alto.

### 1.3 GPU sysfs (cena parada)

| Experimento | `gpubusy` (lifetime) | GPU MHz mediana |
|-------------|----------------------|-----------------|
| Exp-003a | **~70,6 %** | **644 MHz** |
| Exp-004 bateria | **~74,1 %** | **644 MHz** |

Frequência e ocupação reflectem **submissões contínuas**, não só “relógio alto”.

### 1.4 Energia (indicador, não alvo)

- Exp-003a com **AC**: `current_now` ~**−923 mA** — **não** é descarga líquida.
- Exp-004 **bateria**, `status=3`, `AC=false`: sysfs reportou correntes **positivas** ~**1,28–1,69 mA** (escala µA); **|I|×V ≈ 3,3–6,3 W** proxy. Sinal do POCO em descarga pode não seguir o sinal Android canónico — usar **gpubusy + simpleperf** como primário para A/B.

---

## 2. CPU — maiores fontes de trabalho desnecessário ou amplificado

### 2.1 Guest AOT (`libmain.so`) — não é JIT, mas não é “barato”

**simpleperf** (múltiplas capturas 60 s, 1.ª fase):

| Símbolo | Overhead | Papel |
|---------|----------|-------|
| **`sub_821C62C8`** | **7–11 %** | Loop de pacing/espera até tick 1/60 s (chama `821C4870` em busy-wait quando atrasado). |
| **`sub_821C4870`** | **~5–6 %** | Poll de tempo / estado de frame. |
| **`QueryGuestTickCount`** | **~2,6–3,3 %** | Host traduz tempo para guest. |
| `FunctionDispatcher::Execute` | **&lt; 0,1 %** | Overhead de dispatch **negligível** (AOT direct call). |

**Conclusão:** o custo CPU guest não é “emulação de opcodes”, é **código Xbox recompilado** + **loops de espera** que disparam com volume alto quando o frame está atrasado (~20 ms real vs 16,67 ms alvo).

### 2.2 Código ARM64 gerado — overhead estrutural

Fonte: `ARM64_CODEGEN_CORRELATION_REPORT.md` (8 funções quentes).

| Padrão | Proporção (amostra estática) |
|--------|------------------------------|
| Load + store | **~44 %** das instruções |
| **`rev` (bswap)** | **~7,3 %** |
| `stp`/`ldp` (prologue guest) | presente em todas as funções quentes |

Macros (`sonic4_pch.h`):

```text
REX_LOAD_U32(x) → volatile load + __builtin_bswap32
```

**Relação com instâncias:** cada instância duplica **12 864** funções guest com este padrão; **não** partilham código guest entre processos.

**Não medido ainda:** ciclos exactos perdidos em `rev` vs Vulkan; optimização de endian **exige** prova de equivalência.

### 2.3 Runtime host (ReXGlue)

| Área | Evidência | Custo |
|------|-----------|-------|
| Dispatch PPC | `FunctionDispatcher` ausente do top | **Baixo** |
| Relógio / pacing | `QueryGuestTickCount`, `PaceGuestMainLoopFrame` | **Médio** (milhares de chamadas/s) |
| MMIO / `CheckLoad` | aparece em perfis | **Médio** em paths quentes |
| Sched Android | `__schedule`, futex, mutex | **Sintoma** de contensão + polling |

### 2.4 Instrumentação que **era** overhead (corrigido)

| Build | Efeito |
|-------|--------|
| `REX_DIAG_PROFILE` no retail | atomics por draw/barrier — **provável +~5 % PACE** (histórico) |
| `REX_TIMING_PROBES` | hooks por dispatch ~1500/s — **provável +~4 % PACE** |

Retail actual: `sonic4_timing_hooks_retail.cpp` — só PACE + telemetria opcional.

---

## 3. GPU / Vulkan — trabalho por frame

### 3.1 Pipeline Xenos → Vulkan

Por present guest (diag, Sonic parado ~40 Hz):

- **~250–320 draws**
- **Milhares** de `rendering_begins` / `barriers` por segundo
- **IssueDraw** dominado por **`vertex_fetch`** (~38 % do tempo IssueDraw em cena pesada) e **`primitive`** (~25 %) — `ISSUE_DRAW_ROOT_CAUSE.md`

Ficheiros: `command_processor.cpp`, `deferred_command_buffer.cpp`, `pipeline_cache.cpp`, `vulkan_presenter.cpp`.

### 3.2 Porque “parado” não reduz draws

O título **continua** a actualizar cena (HUD, água, partículas, áudio). **Não** há prova de framebuffer idêntico entre presents consecutivos — logo **não** há base segura para skip global de render.

### 3.3 Experimentos superficiais (não resolveram footprint)

| Exp | Resultado |
|-----|-----------|
| **Exp-003a FIFO** | PACE/GPU busy **inalterados** materialmente |
| **Exp-004 opt1** (`present_render_pass_clear=false`) | impacto **não** validado; não substitui redução de draws |

---

## 4. Memória, sincronização, threads

| Factor | Comportamento |
|--------|----------------|
| EDRAM / shared memory | `RequestRange` em **vertex_fetch** — hot path CPU |
| Workers | Thread GPU `CommandProcessor` + guest `XThread*` + áudio SDL |
| Wakeups | Exp-002 reduziu spin em vsync/WAIT_REG_MEM — **scheduler**, não draws |
| Instâncias N× | N× guest RAM + N× Vulkan state + N× threads (sem pool partilhado hoje) |

---

## 5. Comparação arquitectural (pesquisa)

| Projeto | CPU jogo | Implicação |
|---------|----------|------------|
| Xenia / Xendroid+ | JIT PPC | Custo variável, optimizações JIT **não** transferíveis literalmente |
| **Este port** | **AOT** | Custo fixo por load/store/endian; ganhos em **codegen** e **menos trabalho por frame** |

Técnicas úteis **se adaptadas**: cache de pipeline/descrição (já parcial — hits ~73 % diag), dedup bind (já no deferred CB), redução de **barriers** só com prova de correctness.

Referência: `RESEARCH_REFERENCE_PROJECTS.md`, clones em `evidence/ANDROID_PORT/research_clones/`.

---

## 6. Optimizações **comprovadas** vs **em aberto**

### Comprovadas (mantidas)

1. `REX_DIAG_PROFILE=OFF` no APK perf.
2. `REX_TIMING_PROBES=OFF` + hooks retail no Android.
3. Cache de `PipelineDescription` (release; hits altos em diag).

### Em aberto / prioridade por evidência

| ID | Hipótese | Evidência que motiva | Risco |
|----|----------|----------------------|-------|
| **H1** | Reduzir custo **`vertex_fetch` / `RequestRange`** | 38 % IssueDraw pesado | **Implementado, performance INCONCLUSA** — Exp-005 não validado (cena/workload); ver `validate_first_stage_workload.ps1` |
| **H2** | Quebrar busy-wait em **`821C62C8`/`821C4870`** sem alterar semântica 60 Hz tick | 13 %+ simpleperf | **Alto** — timing gameplay |
| **H3** | Endian fast-path para região guest já nativa-endian em RAM principal | 7 % `rev` estático | Alto —MMIO fronteiras |
| **H4** | Menos barriers redundantes no deferred CB | milhares/s | Alto |
| **H5** | Telemetria `RENDER_VK` fiável | bug: `libmain` sem `REX_FRAME_STATS` | **Baixo** — corrigido em `pilot/CMakeLists.txt` |

**Não fazer:** frame-skip global; reduzir qualidade visual; reintroduzir cache `PipelineDescription` incompleto; optimizar só “Hz” ou mAh sem trabalho/frame.

---

## 7. Métricas para o loop de engenharia (não FPS como alvo)

Prioridade de colecção (bateria, mesma cena):

1. **ms CPU / present:** `record_us_pf`, `issue_draw_us_pf` (diag ou telemetria corrigida).
2. **ms GPU / present:** `gpu_exec_us_avg`.
3. **Trabalho / present:** `draws_per_present`, `barriers/s`.
4. **Guest:** simpleperf top `sub_*` + `QueryGuestTickCount`.
5. **Indicadores energia:** `gpubusy`, |I|×V (com nota de sinal POCO).

Baselines preservados: `20261117_exp001_cntvct`, `exp002_*`, `exp003a_fifo`, `exp004_static_render`.  
**Novas iterações:** janela padrão **20 s** — `MEASUREMENT_POLICY.md`.

**Medição directa Exp-004 (telemetria fixa, 1.ª fase parado):** `draws_per_present ≈ 189`, `record_us_pf ≈ 7 ms` — ver `raw/power/exp004_telemetry_fixed/RESULTS.md`.

---

## 8. Bug de medição Exp-004 (correcção)

`PROBE RENDER_VK` a **zero** com PACE activo: `sonic4` compilava `REX_RENDER_TELEMETRY` mas **sem** `REX_FRAME_STATS` → `ExchangeWindowAndReset()` era **stub inline** em `libmain`, enquanto contadores viviam em `librexruntime.so` / `librexgpu-xenos.so`.

**Fix:** `REX_FRAME_STATS=1` em `sonic4` quando `REX_RENDER_TELEMETRY=ON` (implementação única em `librexruntime.so`).

Rebuild necessário antes da próxima coleta render.

---

## 9. Conclusão orientada a “~50 instâncias”

O título comporta-se como exigente porque **cada ~25 ms** ( ~40 Hz) paga:

1. **Simulação guest recompilada densa** (memória + bswap + waits).  
2. **~250+ draws** e **~6+ ms** de gravação Vulkan no CPU.  
3. **GPU ~70 %+** ocupada a frequências médias altas.

Nada disso é inevitável do hardware — parte é **fidelidade Xbox + pipeline host completo** por frame. O caminho para eficiência por instância passa por **reduzir trabalho comprovadamente redundante por frame** (H1, depois H2 com extrema cautela), não por mudar o contador de FPS ou o modo present.

**Próximo passo de engenharia:** rebuild Exp-004 com telemetria corrigida → confirmar `draws_per_present` em bateria → atacar **H1** (`vertex_fetch` / residency) com um patch isolado e A/B em trabalho/frame (não só PACE).

---

## Referências de ficheiros

| Documento / pasta |
|-------------------|
| `CPU_EXECUTION_ARCHITECTURE_AUDIT.md` |
| `ROOT_CAUSE_PERFORMANCE_INVESTIGATION.md` |
| `ISSUE_DRAW_ROOT_CAUSE.md` |
| `ARM64_CODEGEN_CORRELATION_REPORT.md` |
| `BATTERY_DRAIN_ROOT_CAUSE.md` |
| `raw/power/exp003a_fifo/`, `raw/power/exp004_static_render/` |
| `third_party/rexglue-sdk/src/graphics/vulkan/command_processor.cpp` |
| `pilot/generated/default/sonic4_pch.h` |