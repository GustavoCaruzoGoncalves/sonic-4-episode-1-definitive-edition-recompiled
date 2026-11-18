# Investigação de causa raiz — performance Android ARM64

**Data:** 2026-11-17  
**Dispositivo:** POCO F7 (`a63a397c`), Snapdragon 8s Gen 4, Adreno 825, Vulkan 1.3.284  
**APK perf:** `android/dist/Sonic4-arm64-v8a.apk` — `REX_DIAG_PROFILE=OFF`, `REX_TIMING_PROBES=OFF`  
**Referência Windows:** mesmo backend Vulkan (código compartilhado), ~60 Hz em cena equivalente.

---

## 1. Resumo executivo

O déficit de performance **não** é um único bug nem “hardware fraco”. São **três camadas acumuladas**, duas já parcialmente corrigidas:

| # | Causa (impacto) | Categoria | Estado |
| --- | --- | --- | --- |
| 1 | Instrumentação Vulkan/diag no APK Release (`REX_DIAG_PROFILE=1`) | B — backend host | **Corrigido** (~+4,7 Hz PACE) |
| 2 | Sondas `sonic4_timing_hooks.cpp` no hot path (~25 hooks, atomics/observe a cada dispatch) mesmo sem diag | A — runtime Android | **Corrigido** (~+1,6 Hz PACE; −2,5 ms `frame_us`) |
| 3 | Custo estrutural do loop de frame: simulação guest ARM64 + gravação Vulkan CPU escala com **draws/present** e `FRAMEUPD` | A + B (+ C driver) | **Aberto** — limita ~42–50 Hz sustentado |

**Meta 60 Hz:** não atingida. Após as duas correções arquiteturais, média sustentada **~45,9 Hz** (vs **~44,3 Hz** na linha de base perf anterior). Picos a 59 Hz existem em trechos leves; fase exigente permanece **37–45 Hz**.

---

## 2. Baseline verificada

| Execução | Log | Build | PACE médio | PACE &lt;45 Hz | `frame_us` médio | Notas |
| --- | --- | --- | ---: | ---: | ---: | --- |
| Diag ON (histórico) | `perf_post_initfix_150s.log` | `REX_DIAG_PROFILE=ON` + probes | 39,6 Hz | 34,5 Hz | 24,1 ms | `FRAMEUPD` 18,9 ms; `DIAG_VK` presente |
| Perf sem probes retail | `perf_nodiag_150s.log` | diag OFF, **hooks completos** | 44,3 Hz | 40,7 Hz | 21,0 ms | `FRAMEUPD` 16,8 ms |
| **Perf retail hooks (esta rodada)** | `perf_retail_hooks_150s.log` (`sonic4_025`) | diag OFF, **`REX_TIMING_PROBES=OFF`** | **45,9 Hz** | **40,4 Hz** | **18,5 ms** | Log mínimo a cada 5 s; sem `FRAMEUPD`/`SCENEDISP` |
| Termal pré-teste | — | bateria 55%, ~36,5 °C | — | — | — | Comparável a sessões anteriores |

**Procedimento:** `adb install -r` → 55 s load + 150 s gameplay (swipes) → `run-as … cat files/user/logs/sonic4_025.log`.

**Ferramentas:** `python evidence/ANDROID_PORT/tools/summarize_probe_log.py`; `simpleperf record` → `evidence/ANDROID_PORT/perf_retail_hooks.data` + `simpleperf_retail_hooks_report.txt`.

---

## 3. Causas raiz ranqueadas

### RC1 — Diagnóstico Vulkan no Release (P0, corrigido)

**Defeito:** `REX_DIAG_PROFILE` forçava atomics por draw/barrier/bind em todo o caminho de gravação Vulkan.

**Evidência:** A/B `perf_post_initfix` vs `perf_nodiag_150s` (tabela acima).

**Arquivos:** `third_party/rexglue-sdk/CMakeLists.txt`, targets `graphics`/`audio`/`system`, `build-android-arm64.bat`.

---

### RC2 — Sondas de timing no hot path guest (P0, corrigido 17/11)

**Defeito:** `pilot/src/sonic4_timing_hooks.cpp` intercepta `sub_821B35C8`, `sub_821C1C80`, `sub_8249CC00`, `sub_8249D308`, `sub_821C0E58`, `sub_821C0770`, `sub_82488958`, etc., com `Observe`, históricos de LR, atomics e `QueryHostTickCount` **em cada chamada** — ordem de **milhares/segundo** (`SCENEDISP` ~1500/s no Android). Isso corria no APK “perf” mesmo com `REX_DIAG_PROFILE=OFF`.

**Correção:**

- `CMake` opção `REX_TIMING_PROBES` (default **OFF** no Android; **ON** no Windows desktop; forçado **ON** com `REX_DIAG_PROFILE`).
- `pilot/src/sonic4_timing_hooks_retail.cpp`: passthrough para funções hookadas; mantém `PaceGuestMainLoopFrame` em `sub_8249E2F8` e stubs `Sonic4PaceRearm*` / `Sonic4NoteTaskCallback`; telemetria leve (PACE a cada 5 s, sem hooks por dispatch).

**Impacto medido:** +1,6 Hz PACE médio; −2,5 ms `frame_us`; menos janelas &lt;45 Hz (20 vs 27, mesma duração).

**Correção de lógica:** hooks **não** alteravam gameplay — só medição (`821C62C8`/`821C4870` já eram diag-only).

---

### RC3 — Orçamento de frame: guest ARM64 + Vulkan CPU (P0, aberto)

**Defeito:** Em carga pesada, o jogo não cabe em **16,67 ms** por frame.

**Evidência (build com probes, ainda válida para decomposição):**

| Regime | PACE | draws/present | `record_us_avg` | `FRAMEUPD` µs |
| --- | ---: | ---: | ---: | ---: |
| Leve | 59,7 | 181 | 2665 | 1615 |
| Pesado | 25,9–34 | 306+ | 8370 | 34157 |

Windows na mesma classe de cena: **~1895 `SCENEDISP`/s @ 60 Hz** (`win_worldselect_after_fix.log`) — backend Vulkan **compartilhado**; o Android paga fator extra no **guest ARM64** e/ou interação Adreno.

**Win vs Android (auditoria de código):**

- Backend Xenos→Vulkan: `command_processor.cpp`, `deferred_command_buffer.cpp` — **sem** ramo `#ifdef ANDROID` no núcleo de gravação.
- Diferenças de plataforma: surface/swapchain (`vulkan_presenter.cpp`), áudio SDL Android, `threading_posix.cpp` (afinidade).
- Conclusão obrigatória do brief: operações **baratas no Windows Vulkan** e **caras no Android** hoje apontam principalmente para **(A) execução PPC recompilada em ARM64** + **(C) driver/scheduling**, não para um segundo backend Vulkan Android distinto.

**simpleperf (25 s, build retail, gameplay):**

| Overhead | Símbolo | Interpretação |
| ---: | --- | --- |
| 10,8% | `sub_821C62C8` | Loop de espera guest (chama `821C4870` em busy-wait até tick 1/60 s) — **sintoma** de frame lento (Ghidra confirma `do { Function_821C4870 } while (...)`). |
| 3,5% | `__imp__sub_821C4870` | Poll de relógio — barato por chamada, volume alto quando atrasado. |
| 0,5% | `CommandProcessor::WriteRegister` (thread GPU Commands) | Vulkan host presente mas não domina amostra agregada; trabalho guest espalhado em muitos símbolos &lt;0,3%. |

**Categoria do déficit restante:** **A (recomp + runtime guest)** + **B (Vulkan record CPU)**. **C (GPU)** medido em 90 s diag (`perf_diag_gpu_90s.log`, `sonic4_026`):

| Regime | `present_hz` | `record_us_avg` | `gpu_exec_us_avg` | `draws_per_present` |
| --- | ---: | ---: | ---: | ---: |
| Leve | 59,7 | 2637 | **819** | 102 |
| Pesado | 27–28 | **6537–9504** | **1920–2556** | 223–346 |

A GPU Adreno executa o frame em **~0,8–2,6 ms** (timestamp queries OK); a gravação Vulkan no CPU chega a **6,5–9,5 ms** antes do custo guest. **Não** é gargalo GPU saturada — é **CPU record + simulação guest**.

---

## 4. Experimentos diferenciais

| Experimento | Hipótese | Medição | Resultado | Conclusão |
| --- | --- | --- | --- | --- |
| `REX_DIAG_PROFILE` OFF | Diag Vulkan infla `record_us` | PACE, `DIAG_VK` | +4,7 Hz; diag ausente | Confirma RC1 |
| `REX_TIMING_PROBES` OFF (retail) | Hooks pilot inflam CPU sem mudar jogo | PACE, `frame_us` | +1,6 Hz; −2,5 ms frame | Confirma RC2 |
| simpleperf vs poll alto | `821C4870` é causa | % CPU em `821C62C8`/`4870` | Alto % em espera | Poll é **efeito** |
| Win Vulkan referência | Backend Android é único gargalo | Comparar código | Núcleo compartilhado | Investigar guest ARM64 + driver |
| `android_thread_cores=auto` | Pinning recupera 60 Hz | Logs anteriores | Sem ganho F7 | Descartado como P0 |

---

## 5. Correções implementadas (esta investigação)

1. **`REX_TIMING_PROBES` + `sonic4_timing_hooks_retail.cpp`** — ver RC2.  
2. **`build-android-arm64.bat`:** `-DREX_TIMING_PROBES=OFF` explícito.  
3. **`build-android-arm64-diag.bat`:** `-DREX_TIMING_PROBES=ON` com diag.  
4. Artefatos: `perf_retail_hooks_150s.log`, `perf_retail_hooks.data`, `simpleperf_retail_hooks_report.txt`.

*(Correção RC1 já estava no tree antes desta rodada; revalidada na linha de base.)*

---

## 6. Vulkan / GPU

- **CPU record:** escala com **draws/present** nos logs diag (RC3).  
- **GPU:** timestamps implementados em build diag (`OnVkGpuExecMicros`); **não** medidos nesta sessão perf retail.  
- **Thread GPU Commands:** amostra dominada por spinlocks kernel + `WriteRegister` — sugere driver/sincronização, não prova saturação GPU sem timestamps.

---

## 7. Timing / pacing

- `821C62C8` / `821C4870`: caminho retail chama código gerado **sem** hook (correto).  
- Lock-free guest clock (histórico) mantido; não reverter.  
- `PaceGuestMainLoopFrame` permanece em `sub_8249E2F8` (retail).

---

## 8. Áudio / threads

- Audio Worker: visível em `simpleperf` (atomics libc); underruns baixos em sessões anteriores.  
- Contenção Main + Audio + GPU: candidato a medição com diag de threads; não endereçado nesta rodada.

---

## 9. Regressões

- **Windows:** `REX_TIMING_PROBES` default ON apenas `WIN32 && !ANDROID` — comportamento dev preservado.  
- **Android retail:** sem uninstall de saves; `install -r` OK (`sonic4_025.log`).  
- Builds Windows completos **não** reexecutados nesta sessão (sem script `build-win` no root do repo).

---

## 10. Vulkan CPU — decomposição (2026-11-17)

Ver **`VULKAN_CPU_RECORDING_INVESTIGATION.md`**. Finding principal: **`record_us_avg` ≠ custo Vulkan total**; em carga pesada **`issue_draw_us_pf` ~8,8 ms** + **`record_us_pf` ~6,2 ms** + **`desc_update` ~0,3 ms** vs **GPU ~2 ms**. Otimizações: reutilização de descriptors de textura (~70% hit), dedup de binds no `Execute`.

## 11. Gargalos restantes e próximos passos (implementáveis)

1. **Rodar APK diag 150 s** — preencher `gpu_exec_us_avg` vs `record_us_avg` (isolamento C).  
2. **simpleperf focado em `libmain.so`** com call graph durante fase **FRAMEUPD &gt;20 ms** (hooks diag só na janela de log).  
3. **Comparar custo por draw** Windows Vulkan vs Android (mesmo save, mesma zona) com contadores diag.  
4. **Codegen / memória guest:** `REX_LOAD`/`STORE` + bswap por acesso — P1 longo prazo (alinhado Pinyon).  
5. **Descriptor/bind redundancy:** auditar diferença de cache hit Android vs Win se GPU timestamps mostrarem CPU-bound record.

---

## 11. Tabela before/after (cadeia de fixes perf)

| Estágio | PACE médio | `frame_us` médio |
| --- | ---: | ---: |
| Release com diag + probes | 39,6 Hz | 24,1 ms |
| Diag OFF, probes ON | 44,3 Hz | 21,0 ms |
| **Diag OFF, retail hooks** | **45,9 Hz** | **18,5 ms** |
| Alvo | 60 Hz | ~16,7 ms |

**Ganho total verificado (diag→retail):** ~**6,3 Hz** médio; **~5,6 ms** no `frame_us` do main loop; ainda **~3–8 ms** acima do orçamento em carga pesada.

---

## 12. Referências de arquivo

| Área | Caminhos |
| --- | --- |
| Retail hooks | `pilot/src/sonic4_timing_hooks_retail.cpp`, `pilot/CMakeLists.txt` |
| Diag Vulkan | `third_party/rexglue-sdk/src/graphics/vulkan/command_processor.cpp`, `frame_stats.*` |
| Clock guest | `third_party/rexglue-sdk/src/core/clock.cpp` |
| Relatórios correlatos | `ANDROID_RUNTIME_ARCHITECTURE_AUDIT.md`, `CPU_HOTSPOT_ANALYSIS.md`, `GPU_AUDIO_ANALYSIS.md` |