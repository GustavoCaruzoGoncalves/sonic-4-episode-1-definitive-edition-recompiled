# Baseline POCO F7 — 2026-11-17

**APK:** `android/dist/Sonic4-arm64-v8a.apk` (instalado com `adb install -r`, `primaryCpuAbi=arm64-v8a`).

**Captura:** `raw/baseline_001/` (log completo + screenshot).

## Condições

- Bateria ~11 %, temperatura **43,1 °C** (4310 mV scale), GPU **688 MHz** (`kgsl-3d0`).
- Sessão após instalação do build atual (async shader storage + warn-once cores).
- Utilizador em gameplay (navegação manual).

## PACE (`PROBE PACE_821C0770/s`, janelas de 5 s)

| Fase | Faixa observada (3 min) |
| --- | --- |
| Início estável | 59,7 Hz (várias janelas) |
| Carga pesada (meio da sessão) | 33,9–49,3 Hz (maioria 38–48 Hz) |
| Recuperação tardia | 50–52 Hz |

**Interpretação:** cenário leve sustenta ~60 Hz; cenário pesado atual cai para ~**40–45 Hz** médio — alinhado a logs históricos `sonic4_038` / `poco_bench4`.

## Evidência anterior (retail, mesma resolução 2772×1280)

| Log | PACE médio | Notas |
| --- | ---: | --- |
| `poco_bench4` (leve) | 59,2 Hz | 0 janelas &lt;45 Hz |
| `sonic4_038` (pesado) | 37,6 Hz | 65 janelas &lt;45 Hz |
| Windows retail bench2 | 59,2 Hz | referência funcional |

## Gargalo priorizado (evidência acumulada)

1. **Guest recomp** em cenas pesadas: `sub_821C62C8` + loop `sub_821C4870` com milhões de polls/s e `REX_QUERY_TIMEBASE` → `clock_gettime` (pré-fix).
2. **Vulkan CPU** em `IssueDraw`: `vertex_fetch` ~38 % / `primitive` ~25 % (diag, `ISSUE_DRAW_ROOT_CAUSE.md`).
3. **Térmico** — GPU 688 MHz e T° 43 °C durante teste; throttling possível em sessões longas.

## Baseline padrão — 1.ª fase, Sonic parado (2026-11-17)

**Procedimento:** `FIRST_STAGE_BASELINE_PROCEDURE.md`  
**Pasta:** `raw/baseline_first_stage/20261117_exp001_cntvct/`  
**Build:** retail + Exp-001 (`cntvct_el0`)

| Métrica | Valor (120 s após PACE) |
| --- | --- |
| PACE (pós-warmup ~22 janelas) | **~42,2 Hz** média (40,0–43,9) |
| GPU freq | 644 MHz |
| Bateria / T° | 7 %, **45,4 °C** |
| simpleperf 60 s | `sub_821C62C8` 7,5 %, `sub_821C4870` 5,9 %, `QueryGuestTickCount` 3,3 % |

Usar **esta pasta** como referência A/B para otimizações na mesma cena (1.ª fase, parado).

## Próximo experimento

Ver `OPTIMIZATION_EXPERIMENTS.md` — comparar sempre contra `baseline_first_stage/20261117_exp001_cntvct`.