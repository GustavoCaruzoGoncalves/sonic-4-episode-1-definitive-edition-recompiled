# Exp-004 — Coleta em bateria (120 s)

**Quando:** 2026-11-17 ~15:50 (após entrada na 1.ª fase parado)  
**Condição:** `AC powered: false`, `status: 3`, nível **19–20 %**, ~46 °C  
**Log:** `sonic4_052.log`  
**Build:** `build-android-arm64-exp004.bat` (telemetria com bug `RENDER_VK=0` — ver `COMPUTATIONAL_OVERHEAD_ROOT_CAUSE.md` §8)

## Resultados (`power_timeseries.csv`, n=22)

| Métrica | Mín | Máx | Média | Mediana |
|---------|-----|-----|-------|---------|
| **PACE (Hz)** | 26,7 | 46,9 | **41,8** | **42,5** |
| **frame_us** | 18 082 | 28 505 | **20 019** | **19 619** |
| **GPU MHz** | 510 M | 763 M | 666 M | **644 M** |
| **gpubusy %** | 67,8 | 78,9 | **74,1** | **74,5** |
| Corrente (µA) | 878k | 1 687k | 1 357k | 1 344k |
| Tensão (µV) | 3,717 M | 3,740 M | 3,724 M | 3,724 M |
| Proxy P (mW, I×V) | 3 284 | 6 276 | **5 055** | **5 004** |

## Comparação com Exp-003a (não equivalente em energia)

| | Exp-003a | Exp-004 |
|---|----------|---------|
| AC | **true** | **false** |
| PACE médio | 37,4 | **41,8** |
| gpubusy | ~70,6 % | **~74,1 %** |
| Corrente | ~−923 mA (líquido com carregador) | +1,3–1,7 mA sysfs (ver nota sinal) |

**Conclusão energética:** A/B de mAh **inválido** entre corridas; footprint computacional (PACE, gpubusy, draws quando telemetria corrigida) é o comparável.

## Exp-004-opt1

`present_render_pass_clear=false` — **sem** validação visual/documentada nesta corrida; impacto em gpubusy **não** isolado (opt1 + telemetria atomics ainda activos).