# Histórico de performance Android — Sonic 4 EP I (recomp)

**Dispositivo de referência:** POCO F7 (`25053PC47G`), Android 17, arm64-v8a, Adreno 825.  
**Árvore local:** `C:\S4EP1Recomp` (sem remoto git — datas dos logs fazem autoridade).  
**Revisão:** 2026-11-17 (reinvestigação “do zero”).

---

## Linha do tempo (cronológica)

| Data / ordem | Mudança | Motivo | Arquivos | Condição de teste | Medições | Veredito | Retido? |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 16/11 — baseline `-O1` | Compilação conservadora | Estabilidade pós-crash compilador | `rexglue.cmake` (histórico) | POCO F7, Splash Hill | `sonic4_003.log`: ~49–60 Hz início, depois ~30 Hz; CPU ~411% | Baseline fraca | Substituída |
| 16/11 | `-O2 -fno-inline` (retail estável) | Evitar crash `0x8218FB50` | `function_dispatcher.cpp` tail thunk + recomp flags | Mesmo device | `sonic4_011.log`: picos 60, sustentado **~30–35 Hz** | Estável, lento | Base de comparação |
| 16/11 | Fix tail thunk `0x8218FB50` | Crash seleção de mundo | `function_dispatcher.cpp` | Win + Android | Sem FATAL; PACE Android ainda ~39 Hz (`android_worldselect_*`) | **Funcional** | Sim |
| 16/11 | `-O3` | Performance guest | `pilot/generated/rexglue.cmake` | POCO F7 | `android_o3_perf.log`: **~41,5 Hz** média | Marginal (+2 Hz) | Sim |
| 16/11 | `-O3 -flto=thin` | Inline cross-TU | idem | POCO F7 | `android_lto_perf.log`: **~44,8 Hz** média, sustentado ~37–42 | Marginal (+5 Hz) | Sim |
| 16/11 | `REX_DIAG_PROFILE=1` em Release Android | Diagnóstico Vulkan/áudio | `pilot/CMakeLists.txt`, `src/{graphics,system,audio}/CMakeLists.txt` | APK “Release” com atomics por draw | `diag_sceneA_ground.log`, `sonic4_017` | **Erro de metodologia** — toda medição até 17/11 foi com overhead de instrumentação | Corrigido 17/11 |
| 16/11 | Sondas `PROBE DIAG_*` | Separar CPU record vs guest | `frame_stats.*`, `sonic4_timing_hooks.cpp` | Cena A/B automatizada | PACE 24–28 Hz pesado; `record_us_avg` 7–11 ms; `underrun/s≈0` | Diagnóstico válido; não é otimização | Opcional (`-DREX_DIAG_PROFILE=ON`) |
| 16/11 | Relógio guest lock-free | Reduzir custo de `QueryGuestTickCount` no poll | `third_party/rexglue-sdk/src/core/clock.cpp` | Splash Hill automatizado | `perf_clock_lockfree_60s.log`: PACE **43–47 Hz** vs **24–29** baseline diag | Ganho real, insuficiente para 60 | Sim |
| 16/11 | Tentativa `cntvct_el0` | Relógio ARM nativo | (revertido) | `perf_clock_opt_ground90s.log` | PACE ~18 Hz, CLOCKPROBE inválido | **Negativo** | Não |
| 16/11–17/11 | `-march=armv8.2-a` (runtime + recomp via helpers) | LSE em Snapdragon | `rexglue_helpers.cmake` | `perf_armv82_research_75s.log` | **~37–39 Hz** sustentado | Marginal vs lock-free isolado | Sim |
| 16/11 | `-Wl,-Bsymbolic-functions` | Menos PLT runtime/GPU | `system/graphics/CMakeLists.txt` | Não isolado A/B | Incorporado no baseline misto | Inconcluso | Sim |
| 17/11 | Fix deadlock init relógio (seqlock) | Arranque `librexruntime.so` | `clock.cpp` Reanchor | `perf_post_initfix_150s.log` | Arranque OK; PACE inalterado | Funcional | Sim |
| 17/11 | `android_thread_cores=auto` | Pinning estilo Pinyon | `threading_posix.cpp` | `perf_thread_cores_auto_120s.log` | PACE **39,7** vs **40,6** baseline; só 1 core no topo no F7 | Neutro/negativo | Cvar mantida, sem ganho |
| **17/11** | **`REX_DIAG_PROFILE` off em APK perf** | Remover atomics por draw/barrier em Release | `CMakeLists`, `build-android-arm64.bat` | POCO F7 150 s Splash Hill (`sonic4_024`) | PACE **44,3** vs **39,6** Hz (diag baseline) — `perf_nodiag_150s.log` | **Ganho real; não suficiente** | **Padrão Release** |
| **17/11** | **`REX_TIMING_PROBES` off (retail hooks)** | Remover ~25 hooks guest no hot path | `sonic4_timing_hooks_retail.cpp`, `pilot/CMakeLists.txt` | POCO F7 150 s (`sonic4_025`) | PACE **45,9** vs **44,3** Hz — `perf_retail_hooks_150s.log` | **+1,6 Hz; RC guest/Vulkan aberto** | **Padrão Android Release** |
| **17/11** | **Vulkan: decomposição + tex descriptor reuse** | `DIAG_VK_BREAKDOWN`; bindings≠invalidate | `command_processor.cpp`, `deferred_command_buffer.cpp` | Diag 120 s + retail 150 s | IssueDraw **~8,8 ms** > record **~6,2 ms**; retail2 **39,1 Hz** (sessão quente) | **Causa clarificada; PACE A/B inconcluso** | Release |
| **17/11** | **GPU timestamp queries (diag)** | `gpu_exec_ns=na` | `command_processor.cpp`, `frame_stats` | Diag build | **Pendente dispositivo** | Medição | Diag only |

---

## Verificação independente dos números da sessão anterior

Fonte primária: `evidence/ANDROID_PORT/perf_post_initfix_150s.log` (`sonic4_022`, 150 s, 23:13–23:15).

| Alegação anterior | Verificação no log | Notas |
| --- | ---: | --- |
| PACE pesado 24–29 Hz | **25,9–32,3 Hz** nos piores blocos (`23:13:37`, `23:14:12`) | Consistente |
| Média mista ~40,6 Hz | **39,6 Hz** (33 janelas `PACE_821C0770/s`) | Consistente |
| `frame_us` 22–29 ms | **28875–32999 µs** nos piores; **15374–16534 µs** a **~59 Hz** | Consistente |
| `record_us_avg` 6,5–11 ms | **8370–9849 µs** pesado; **2665 µs** a 59,7 Hz | Consistente |
| draws/present 203–248 | **306–342** no pior; **180,9** a 59,7 Hz | Pico anterior subestimado no pior momento |
| `821C4870/s` ~4–4,6M | **4,0–5,0M/s** em carga pesada | Consistente |
| `821C62C8_us_avg` 19–26 ms | **Enganoso**: inclui tempo de poll bloqueado; com PACE baixo sobe a **27–34 ms** (`DIAG_GUEST`) | Sintoma, não causa raiz |
| underrun ≈0, stutter audível | **underrun/s=0**; `worker_disp ~1,5 ms × 187/s` | Stutter correlaciona com PACE, não fila SDL |
| gpu_exec | `gpu_exec_ns=na` | Corrigido em build diag novo (`gpu_exec_us_avg`) |

**Descoberta que invalida parte do diagnóstico antigo:** no **mesmo log**, com **`SCENEDISP_821A8488/s=1519,6`** (comparável ao Windows a ~1895/s com 60 Hz), o Android atinge **`PACE_821C0770/s=59,7`** quando **`draws_per_present_avg≈181`** e **`FRAMEUPD dur_us avg≈1615`**. Já com **`draws_per_present_avg≈306`**, PACE cai para **25,9 Hz** e **`FRAMEUPD dur_us avg≈34157`**.

Conclusão de causalidade: **não** é “dispatch de cena alto ⇒ inevitavelmente lento” (Windows prova o contrário). O gargalo é **trabalho total por frame guest** (simulação recompilada ARM64 + gravação Vulkan CPU proporcional a draws/present), não a taxa `SCENEDISP` isolada.

---

## Baseline atual (pré-validação build perf 17/11)

| Métrica | Valor (log diag, build com `REX_DIAG_PROFILE=ON`) |
| --- | ---: |
| PACE médio | 39,6 Hz |
| PACE pesado (<45 Hz) média | 34,5 Hz (25 janelas) |
| `frame_us` médio | ~24,1 ms |
| Windows ref. (`win_worldselect_after_fix.log`) | ~60 Hz, `FRAMEUPD dur_us` típico 1,6–6,5 ms |

**Próximo passo obrigatório:** `build-android-arm64.bat` (diag OFF) → `adb install` → 150 s gameplay reprodutível → comparar com `perf_post_initfix_150s.log` usando `tools/summarize_probe_log.py`.

---

## Artefatos por experimento

| Log / relatório | Papel |
| --- | --- |
| `PERFORMANCE_OPTIMIZATION_REPORT.md` | Primeira linha base `-O2` |
| `android_*_perf.log` | O3 / LTO |
| `diag_sceneA_ground.log`, `VULKAN_AUDIO_PROFILING.md` | Diag cena A/B |
| `perf_clock_lockfree_60s.log` | Opt relógio |
| `perf_armv82_research_75s.log` | armv8.2 |
| `perf_post_initfix_150s.log` | **Baseline pós-fix init (referência principal)** |
| `perf_thread_cores_auto_120s.log` | A/B pinning |
| `win_worldselect_after_fix.log` | Referência Windows |