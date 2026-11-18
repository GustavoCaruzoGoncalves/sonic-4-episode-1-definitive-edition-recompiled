# Política de medição — POCO F7 (ciclo rápido)

**Objectivo:** validar hipóteses de **overhead computacional** com o mínimo de tempo morto. O problema não é subtil — janelas curtas bastam para A/B.

## Regra padrão

| Tipo | Duração | Amostras (intervalo 5 s) |
|------|---------|---------------------------|
| **Diagnóstico / A/B** | **20 s** | ~4 linhas CSV + 3–4 linhas `PROBE PACE` / `RENDER_VK` |
| **Série longa** | 120 s+ | Só com motivo explícito (térmico, spikes intermitentes, fugas, publicação de baseline histórica) |

Script:

```powershell
.\scripts\measure_power_poco.ps1 -Label expNNN_hypothesis
# opcional: -Seconds 120 quando documentado
```

## Ciclo de engenharia

1. Identificar overhead concreto (código + perfil).
2. Uma mudança isolada → build ARM64 Release → `adb install -r`.
3. Utilizador: 1.ª fase, Sonic parado (**sem** automação de menus).
4. **Medir 20 s** (mesma cena, anotar AC/bateria).
5. Comparar com baseline **imediato** (Exp-003a / última corrida equivalente).
6. Manter ou reverter; passar ao próximo gargalo.

## O que recolher em 20 s

- sysfs: `current_uA`, `voltage_uV`, GPU MHz, `gpubusy`
- Log: `PACE_821C0770`, `RENDER_VK` (se build com telemetria)
- `dumpsys battery` (AC, nível, temp.) no fim

**Não** repetir 120 s só porque a melhoria parece pequena — aprofundar código ou mudar hipótese.

## Baselines históricos

Corridas **120 s** (Exp-001, Exp-003a, Exp-004 longas) **mantêm-se** como referência; novas iterações usam **20 s** por defeito.