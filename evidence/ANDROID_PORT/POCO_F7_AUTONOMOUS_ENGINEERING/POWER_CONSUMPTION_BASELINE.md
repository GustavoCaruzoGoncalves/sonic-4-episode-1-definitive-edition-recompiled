# Baseline de consumo — 1.ª fase, Sonic parado

## Referência FPS (Exp-001)

`raw/baseline_first_stage/20261117_exp001_cntvct/` — PACE médio **42,0 Hz**.

## Referência potência (pré Exp-002, carregador ligado)

Captura pontual com jogo PID 5091 (wireless):

| Métrica | Valor |
| --- | --- |
| `current_now` (µA) | −3 057 000 |
| `voltage_now` (µV) | 3 738 000 |
| Potência \|I×V\| | **~11,4 W** (proxy SoC; net com AC) |
| Temperatura | 42,6 °C |
| GPU MHz | 480 |
| gpubusy | ~60 % |

**Limitação:** com AC, `current_now` é componente de descarga da bateria; use série temporal para A/B na **mesma** condição de carregamento.

## Procedimento repetível

```powershell
# Após 1.ª fase + Sonic parado:
.\scripts\measure_power_poco.ps1 -Label exp002_before -Seconds 120
```

Comparar médias de `current_uA`, `gpubusy`, PACE e temperatura.