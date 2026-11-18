# Coleta “live” — 1.ª fase parado (120 s)

**Ficheiro:** `power_timeseries.csv`  
**Nota:** início da coleta com **AC ligado** (carregador detectado durante a janela). Usar como proxy de CPU/GPU/PACE; corrente **não** é descarga pura.

| Métrica | Média | Mediana |
|---------|-------|---------|
| **PACE (Hz)** | **47,17** | **46,6** |
| **frame_us** | **18,32 ms** | **18,48 ms** |
| **GPU MHz** | 720 M | 688 M |
| **gpubusy %** | **79,2** | **79,4** |
| Corrente (µA) | −2,20 M | −2,20 M |
| Proxy P (mW) | ~**8,44 W** | — |

**23/22 amostras PACE válidas.** gpubusy **~9 pp** acima da sessão Exp-003a (~71 %), alinhado a maior ocupação GPU na janela.

APK em seguida actualizado com fix `REX_FRAME_STATS` em `libmain` — necessário **reentrar na 1.ª fase** para `PROBE RENDER_VK` com draws/frame.