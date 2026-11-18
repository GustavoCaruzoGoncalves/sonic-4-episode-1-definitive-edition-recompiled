# Otimização de performance ARM64 (Sonic 4 EP1 — Android)

**Objetivo:** desempenho comparável ao Windows (~60 Hz guest, 60 FPS, áudio estável) no POCO F7, **sem** reduzir qualidade visual nem manipular PACE artificialmente.

**Status (2026-11-17):** **não concluído.** Arranque e gameplay básico OK; performance sustentada **~32–41 Hz** em fase pesada (**39,6 Hz** média verificada em 33 janelas) vs **~60 Hz** no Windows — ver `perf_post_initfix_150s.log`. **Correção arquitetural 17/11:** Release Android não deve usar `REX_DIAG_PROFILE` (ver `PERFORMANCE_HISTORY_REVIEW.md`); build perf: `build-android-arm64.bat`.

**Referência Windows:** `evidence/ANDROID_PORT/win_worldselect_after_fix.log` (~60 Hz PACE sustentado).

**Baseline Android (pré-otimização):** `VULKAN_AUDIO_PROFILING.md`, logs `diag_sceneA_ground.log` — carga pesada típica **PACE 24–29 Hz**, `SCENEDISP` 350–1200/s, `821C62C8_us` **28–37 ms**.

**Baseline pós-arranque (build atual: lock-free + fix seqlock + `-march=armv8.2-a` + `-Bsymbolic-functions`):** `perf_post_initfix_150s.log` (`sonic4_022.log`, 150 s gameplay real no POCO F7, 23:13–23:15 UTC-3).

| Métrica | Fase pesada (janelas representativas) | Pico leve (fim da sessão) | Windows (ref.) |
| --- | ---: | ---: | ---: |
| `PROBE PACE` | **28,4–41,5 Hz** (min das janelas ~32 Hz) | até **59,7 Hz** | ~60 Hz |
| `frame_us` | **22–29 ms** | ~15–16 ms | ~16 ms |
| `821C62C8_us_avg` | **19–26 ms** | ~1,5–8 ms | (não extraído) |
| `821C4870/s` | **~4,0–4,6M/s** | ~0,5–2,5M/s | |
| `record_us_avg` (Vulkan) | **6,5–11 ms** | ~6 ms | |
| `draws_per_present_avg` | **203–248** | menor | |
| `worker_disp_us_avg` | **~1,5 ms** × ~187/s | igual | |
| `underrun/s` | **0** | 0 | |
| `gpu_exec_ns` | **na** (sem timestamp queries) | na | |

**Workflow obrigatório:** cada mudança em CPU/Vulkan/áudio/relógio/threading → rebuild → `adb install -r` → gameplay → extrair `files/user/logs/sonic4_*.log` → comparar com baseline **antes** de considerar sucesso.

---

## 1. Estratégia

| Prioridade | Área | Hipótese |
| --- | --- | --- |
| P0 | Runtime `QueryGuestTickCount` / MFTB | Milhões de leituras/s no poll `821C4870`; mutex + `clock_gettime` por chamada saturam CPU |
| P0 | Carga guest cena + Vulkan record | `SCENEDISP` alto ⇒ draws/present e ~8–11 ms record CPU — limita avanço do relógio ⇒ poll prolongado |
| P1 | Código recompilado ARM64 | Hot paths `821C4870`, dispatch de cena, helpers `REX_LOAD` |
| P1 | Vulkan Adreno | Barriers/desc binds; PSO cache 32 B; timestamps GPU ausentes |
| P2 | Audio Worker | ~1,4 ms/dispatch guest — competição CPU, não underrun |

**Fora de escopo como benchmark:** comparação “céu vs chão” (observação histórica apenas).

---

## 2. Otimização #1 — relógio guest analítico lock-free

**Arquivos:** `third_party/rexglue-sdk/src/core/clock.cpp`  
**Plataformas:** Windows + Android (runtime compartilhado)

**Antes:** cada `QueryGuestTickCount()` → `UpdateGuestClock()` com `mutex` try_lock + atualização incremental.

**Depois:** `guest = anchor_guest + (host - anchor_host) * num / den`; leituras sem mutex; `ResetGuestClockHostBaseline()` só avança `anchor_host` (preserva semântica pós-sleep do `PaceGuestMainLoopFrame`); seqlock (`guest_clock_seq`) em mutações.

**Tentativa revertida:** `cntvct_el0` mal calibrado → `CLOCKPROBE` inválido e PACE ~18 Hz — **não** integrado.

### Benchmark Android (POCO F7, Splash Hill automatizado, build 2026-11-16 ~22:57)

| Métrica | Baseline diag | Pós lock-free (`perf_clock_lockfree_60s.log`) |
| --- | ---: | ---: |
| PACE (SCENEDISP ~430–710/s) | 24–29 Hz | **43–47 Hz** |
| PACE (SCENEDISP ~1100/s) | ~28 Hz | **~33 Hz** |
| `821C62C8_us_avg` (carga média) | 28–37 ms | **~19 ms** |
| `821C4870/s` | ~4,5M/s | ~4,7M/s (inalterado — poll guest original) |

**Conclusão:** ganho **mensurável** no pacing host e no tempo em `821C62C8`, mas **insuficiente** para 60 Hz sob carga pesada. O poll guest continua dominante enquanto o frame guest não termina a tempo.

### Regressão Windows

Pendente build Windows pós-mudança de relógio (obrigatório antes de merge).

---

## 2b. Otimização #2 — `-march=armv8.2-a` (Android)

**Fonte:** Pinyon Shift / ShiftGlue (`PinyonShiftRexGlue.cmake` NN-1.11).  
**Arquivo:** `third_party/rexglue-sdk/cmake/rexglue_helpers.cmake`  
**Efeito:** LSE para atomics (runtime `clock`, diag counters, threading) em Snapdragon 8-series.

**Medição:** `perf_armv82_research_75s.log` — PACE sustentado **~37–39 Hz** (gameplay automático; dispositivo já aquecido). Ganho marginal vs lockfree-only (~43–47 Hz em trecho anterior); **rebuild completo** do `libmain` confirmado (`-march=armv8.2-a` no ninja).

---

## 2c. Otimização #3 — linker `-Bsymbolic-functions`

**Fonte:** Pinyon Shift NN-3.6.  
**Arquivos:** `src/system/CMakeLists.txt`, `src/graphics/CMakeLists.txt` (`rexruntime`, `rexgpu-xenos`).  
**Objetivo:** menos PLT entre funções do runtime Vulkan/kernel.

**Validação de performance:** **não isolada** após #2; baseline atual mistura #1–#3. Ganho marginal possível; **não** aprovado como “sucesso” sem A/B dedicado.

---

## 2d. Correção funcional — deadlock na init estática (não é otimização)

**Arquivo:** `third_party/rexglue-sdk/src/core/clock.cpp`  
**Bug:** `RecomputeGuestTickScalar()` → `ReanchorGuestClockAtCurrentSample()` → `ComputeGuestTickFromHost()` com seqlock ímpar → **spin infinito** ao carregar `librexruntime.so`.  
**Fix:** re-anchor usa `GuestTickAtHostSample()` direto (sem leitor seqlock).  
**Prova:** arranque + `SDL_main` + logs `PROBE` em `sonic4_022.log`.  
**Impacto em PACE:** **nenhum** (só desbloqueia teste). Performance continua inadequada.

---

## 3. Pesquisa externa

Ver **`XBOX360_ANDROID_RESEARCH.md`** — Xeo, xenia-android, Asura, Pinyon Shift, Forza Horizon Android (recomp, não emulador).

---

## 4. Gargalo prioritário (reavaliação 17/11)

1. **`FRAMEUPD` (8249E44C):** **~34 ms** em regime degradado vs **~1,6 ms** a 59,7 Hz no **mesmo log** — déficit de simulação guest ARM64 + trabalho gráfico, não só pacing.
2. **Vulkan `record_us_avg`:** escala com **draws/present** (**8370 µs @ 306 draws** vs **2665 µs @ 181**); com **SCENEDISP ~1520/s** o jogo **pode** estar a 60 Hz se o host couber no orçamento (Windows faz isso a **1895/s**).
3. **Poll `821C4870` (~4–5M/s):** sintoma de frame atrasado; não tratar como causa primária pós lock-free.
4. **Overhead diag em Release (F1):** `REX_DIAG_PROFILE` estava ON em todo APK — **removido** do build perf (`-DREX_DIAG_PROFILE=OFF`).
5. **Áudio:** underrun 0; stutter = irregularidade de produção guest + contenção CPU (`ANDROID_RUNTIME_ARCHITECTURE_AUDIT.md`).
6. **GPU:** timestamps diag implementados → campo `gpu_exec_us_avg` (antes `gpu_exec_ns=na`).

### A/B — build perf sem diag (2026-11-17, POCO F7 `a63a397c`)

| Métrica | Baseline diag ON (`perf_post_initfix_150s.log`, `sonic4_022`) | Perf OFF (`perf_nodiag_150s.log`, `sonic4_024`) | Δ |
| --- | ---: | ---: | ---: |
| `PACE_821C0770` média | 39,6 Hz | **44,3 Hz** | **+4,7 Hz** |
| PACE &lt;45 Hz (média) | 34,5 Hz | **40,7 Hz** | **+6,2 Hz** |
| PACE mínimo | 25,9 Hz | **34,1 Hz** | +8,2 Hz |
| `frame_us` médio | 24 104 µs | **20 973 µs** | −3,1 ms |
| `FRAMEUPD` médio | 18 856 µs | **16 836 µs** | −2,0 ms |
| Janelas `FRAMEUPD` &gt;20 ms | 22/33 (67%) | **12/44** (27%) | menos picos |
| `DIAG_VK` | presente | **ausente** (build correto) | — |

**Procedimento:** `adb install -r` → Splash Hill ~150 s (save existente, swipes horizontais) → `sonic4_024.log`.

**Veredito:** remover instrumentação diagnóstica do Release é **ganho real e mensurável**, mas **insuficiente** para 60 Hz sustentado — gargalo estrutural guest/Vulkan permanece.

### A/B — retail timing hooks (`REX_TIMING_PROBES=OFF`, 2026-11-17)

| Métrica | Probes ON (`perf_nodiag_150s.log`) | Retail hooks (`perf_retail_hooks_150s.log`, `sonic4_025`) | Δ |
| --- | ---: | ---: | ---: |
| PACE médio | 44,3 Hz | **45,9 Hz** | +1,6 Hz |
| PACE &lt;45 Hz (média) | 40,7 Hz | **40,4 Hz** | ~0 |
| `frame_us` médio (main loop) | 20 973 µs | **18 495 µs** | −2,5 ms |

**Implementação:** `sonic4_timing_hooks_retail.cpp`; `-DREX_TIMING_PROBES=OFF` em `build-android-arm64.bat`. Detalhes: `ROOT_CAUSE_PERFORMANCE_INVESTIGATION.md`.

## 4b. Vulkan CPU (2026-11-17)

- Decomposição medida: **`VULKAN_CPU_RECORDING_INVESTIGATION.md`**.
- Correções: rebind de textura condicional; dedup `Execute`.
- APK Release revalidado: `perf_vk_opt_retail2_150s.log` (PACE médio **39,1 Hz** — sessão quente; baseline **45,9 Hz** ainda referência até reteste frio).

## 5. Próximas otimizações (fila — cada uma com A/B)

| Prioridade | Item | Fonte / hipótese | Medição de sucesso |
| --- | --- | --- | --- |
| P0 | `android_thread_cores` (auto: GPU Commands nos big cores) | Pinyon `docs/ANDROID.md` L62–72; implementado `threading_posix.cpp` (`REXCVAR` `android_thread_cores`) | Ver A/B abaixo |
| P0 | Vulkan **timestamp queries** no diag | Pinyon backlogs; `frame_stats` já expõe `gpu_exec_ns=na` | `gpu_exec_ns` preenchido + separar CPU record vs GPU |
| P0 | Sub-timers `IssueDraw` (RT/pipeline/barrier) | `DIAG_VK_BREAKDOWN` v2 | Identificar % dentro dos **8,8 ms** |
| P1 | Investigação `SCENEDISP` / draws | Mesmo título Win vs Android, simpleperf | `SCENEDISP/s` e draws/present alinhados |
| P1 | Hot path `sub_821C4870` no ARM64 gerado | `simpleperf` + disassembly `libmain.so` | Redução mensurável de `821C4870/s` ou custo/frame |
| P1 | Turnip (opcional, licença MIT) | Pinyon `docs/ANDROID.md` L18–44 | GPU ms/frame sem perder qualidade |
| P2 | `function_dispatcher` cache | Observação + áudio worker | −CPU em Audio Worker sem regressão |

### A/B — `android_thread_cores=auto` (2026-11-17)

| Build | Log | PACE médio | PACE fase pesada | Pinning ativo? |
| --- | --- | ---: | ---: | --- |
| Antes | `perf_post_initfix_150s.log` | **40,6 Hz** | **28–41 Hz** | N/A |
| Auto (novo) | `perf_thread_cores_auto_120s.log` | **39,7 Hz** | **26–37 Hz** (picos 59 Hz em trecho leve) | **Não** — só `cpu7` @ 3206 MHz; regra Pinyon exige ≥2 cores no topo (`cpu0–1` 2016, `cpu2–4` 3014, `cpu5–6` 2803, `cpu7` 3206 via `adb`) |

**Veredito:** manter cvar (permite `GPU_Commands=7` manual), **não** conta como otimização no F7 até testar override explícito.

---

## 6. Artefatos

| Arquivo | Descrição |
| --- | --- |
| `perf_post_initfix_150s.log` | **Baseline atual** pós-fix init (gameplay real) |
| `perf_thread_cores_auto_120s.log` | A/B pinning auto (sem efeito no F7) |
| `perf_clock_lockfree_60s.log` | PACE pós-opt relógio (pré-init-bug; ainda válido para efeito #1) |
| `perf_armv82_research_75s.log` | armv8.2 + symbolic (marginal) |
| `perf_clock_opt_ground90s.log` | Regressão cntvct (histórico — build inválido) |
| `diag_sceneA_ground.log` | Baseline diagnóstico cena A |

---

## 6. Restrições mantidas

Fix thunk `0x8218FB50`, timing guest sem cheats, qualidade visual, Win D3D12/Vulkan, Android Vulkan, toque, retail.