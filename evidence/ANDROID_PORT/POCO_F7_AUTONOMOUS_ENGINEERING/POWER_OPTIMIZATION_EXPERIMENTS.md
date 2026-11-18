# Experimentos de energia

## Exp-002 — reduzir wakeup/spin em idle (Android)

**Hipótese:** drenagem alta com Sonic parado vem de polling 1 kHz (VSync), spin de frame pacing &lt;1 ms, e `sched_yield` na thread GPU — não só de “FPS alto”.

**Mudanças:** ver `BATTERY_DRAIN_ROOT_CAUSE.md` (graphics_system, xboxkrnl_video, command_processor).

**Baseline:** `POWER_CONSUMPTION_BASELINE.md` + PACE 42 Hz (`exp001` folder).

**Métricas:** `scripts/measure_power_poco.ps1` 120 s; comparar `current_uA`, gpubusy %, PACE, `simpleperf` top.

**Status:** medido `raw/power/exp002_postfix/` (120 s, carregador AC).

| Métrica | Pré (pontual) | Pós Exp-002 (média 22 amostras) |
| --- | ---: | ---: |
| \|current_now\| | 3 057 000 µA | **~2 050 000 µA** (1,67–3,02 M) |
| GPU MHz | 480 | **~76–873** (média ~805) ⚠️ subiu |
| Temperatura | 42,6 °C | 40,7→44,2 °C |

Corrente média **desceu ~33 %** no mesmo modo AC (proxy); GPU DVFS **não** refletiu idle (873 MHz sustentado). Próximo alvo: reduzir trabalho GPU/frame em cena estática + confirmar PACE na série.

## Exp-003a — Android FIFO present (Vulkan)

**Hipótese:** `VK_PRESENT_MODE_MAILBOX_KHR` (log mode **1**) sustenta clocks Adreno altos; **FIFO** alinha ao painel.

**Mudança:** `vulkan_presenter.cpp` (só Android).

**Investigação:** `ADRENO_825_GPU_FREQUENCY_EXP003.md`

**Status:** APK instalado — entra 1.ª fase parado; medir `presentation mode` no log + `measure_power_poco.ps1 -Label exp003a_fifo`.