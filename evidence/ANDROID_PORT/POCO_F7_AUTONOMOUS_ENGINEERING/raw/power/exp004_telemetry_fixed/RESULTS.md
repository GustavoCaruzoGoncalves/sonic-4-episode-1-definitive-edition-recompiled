# Exp-004 telemetria corrigida — `sonic4_056.log`

**Coleta:** 120 s (pré-política 20 s); cena: 1.ª fase parado.  
**AC:** ligado durante a janela (`status=2` no fim).

## Telemetria RENDER_VK (estável em gameplay)

| Métrica | Valor típico (regime estável) |
|---------|-------------------------------|
| **present_hz** | ~39–44 (alinhado a PACE) |
| **draws_per_present** | **~189** |
| **draws/s** | **~7 400–8 400** |
| **record_us_pf** | **~6 800–7 500 µs** (~**7 ms**/present só gravação Vulkan) |
| **submit_us_pf** | ~640–770 µs |
| **barriers/s** | ~**7 900–8 800** |
| **rendering/s** | ~**3 300–3 700** |
| **last_present_draws** | ~190 |
| **gpu_exec_us_avg** | **0** (contador GPU não activo neste build) |
| **bind_skip_*** | 0 (dedup não incrementa skip counters ou path diferente) |

## Série sysfs (`power_timeseries.csv`)

| | Média | Mediana |
|---|-------|---------|
| PACE | 42,6 Hz | 43,7 Hz |
| frame_us | 20,0 ms | 19,6 ms |
| gpubusy | 77,7 % | 78,7 % |
| GPU MHz | 643 M | 644 M |
| \|I\|×V proxy | ~9,5 W | (AC — não descarga pura) |

## Conclusão para overhead computacional

Mesmo parado: **~189 draws/present** + **~7 ms** CPU record/present → explica gpubusy **~78 %** sem precisar de 120 s de reconfirmação. Próximo A/B: **H1 vertex_fetch** (ou redução de trabalho Vulkan com prova), medir **20 s** após patch.