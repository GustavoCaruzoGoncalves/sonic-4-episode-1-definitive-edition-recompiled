# Experimentos de otimização — POCO F7

## Exp-001 — Relógio host ARM64 (`cntvct_el0`)

**Data:** 2026-11-17  
**Hipótese:** polls de `REX_QUERY_TIMEBASE` no loop `sub_821C62C8` / `sub_821C4870` chamam `QueryGuestTickCount` → `QueryHostTickCount` → `clock_gettime(CLOCK_MONOTONIC_RAW)` a cada iteração; no ARM64 isso domina CPU em cenas pesadas (milhões de chamadas/s em logs diag).

**Mudança:** `third_party/rexglue-sdk/src/core/clock_posix.cpp` — em `__aarch64__` (Linux/Android), `host_tick_count_platform` usa `cntvct_el0` e `host_tick_frequency_platform` usa `cntfrq_el0`.

**Risco:** pacing do guest e `PaceGuestMainLoopFrame` devem permaneer monótonos; baseline realinhado via mesma API.

**Baseline:** `POCO_F7_PERFORMANCE_BASELINE.md` (PACE 38–48 Hz cena pesada na mesma sessão).

**Validação:** repetir coleta PACE 3 min em gameplay pesado; opcional `simpleperf` antes/depois em `clock_gettime` vs `libmain.so`.

**Baseline comparável:** `raw/baseline_first_stage/20261117_exp001_cntvct` — 1.ª fase, Sonic parado, ~42 Hz PACE sustentado.

**Status:** instalado e medido; PACE na cena fixa **não** atingiu 60 Hz (gargalo de cena/workload, não só relógio). `QueryGuestTickCount` ainda ~3,3 % no simpleperf — próximo passo: reduzir polls no caminho `821C4870` ou re-perfil após warm-up térmico.

## Exp-000 (controle histórico)

- Shader storage assíncrono no Android (`rex_app.cpp`) — corrige hang, sem ganho POCO medido em leve (~60 Hz já OK).
- `REX_PHYS_HOST_OFFSET` = 0 no Android — mapeamento físico já alinhado (`xmemory.cpp`).