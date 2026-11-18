# Exp-004 — Hipótese e isolamento

## Referência

- Baseline visual/energia: `raw/power/exp003a_fifo/` (FIFO, retail sem telemetria render).
- Exp-004a (este build): **Exp-004-opt1** + **telemetria**.

## Exp-004-diag — telemetria (`REX_RENDER_TELEMETRY`)

- Contadores `frame_stats` (atomics por draw/barrier/present), **sem** `REX_IDRAW_PHASE`.
- Hooks retail: `PROBE RENDER_VK` a cada ~5 s (alinhado a `PROBE PACE`).

## Exp-004-opt1 (única mudança de produção)

- **Android:** `present_render_pass_clear` default **false** no presenter Vulkan (swapchain pass: `DONT_CARE` em vez de `CLEAR` quando o quad de composição cobre o ecrã).
- Hipótese: poupa trabalho de clear na passagem final de apresentação; **não** altera guest nem número de draws guest.

## Medição

- Cena: 1.ª fase, Sonic parado.
- **Bateria:** AC **desligado** para A/B de corrente vs Exp-003a (AC invalida corrente).
- Script: `scripts/measure_power_poco.ps1 -Label exp004_static_render -Seconds 120`
- Comparar: PACE, `draws_per_present` no log, gpubusy, GPU MHz, corrente.

## Critérios

- Render correcto (sem artefactos no swapchain).
- `draws_per_present` estável (sem frame-skip artificial).
- Redução mensurável em gpubusy/corrente **em bateria**, ou reverter opt1.