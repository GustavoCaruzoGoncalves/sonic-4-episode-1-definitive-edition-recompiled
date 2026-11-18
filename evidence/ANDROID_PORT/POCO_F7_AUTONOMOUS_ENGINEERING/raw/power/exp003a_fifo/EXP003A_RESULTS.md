# EXP-003a — Resultados (FIFO / `VK_PRESENT_MODE_FIFO_KHR`)

## Identificação da medição

| Campo | Valor |
|--------|--------|
| Dispositivo | POCO F7 (wireless `192.168.70.7:44743`) |
| Cena | 1.ª fase, Sonic **parado** (sessão preservada, **sem** `force-stop` / reinstall) |
| Início ISO | `2026-11-17T15:39:15.0207241-03:00` |
| Fim ISO | `2026-11-17T15:41:20.2647095-03:00` |
| Duração pedida | 120 s |
| Duração efectiva (script) | ~125 s (23 amostras × 5 s + primeira leitura imediata) |
| Log de jogo | `sonic4_050.log` |
| APK | `com.s4ep1recomp.sonic4` v1.0 (`versionCode=1`), `lastUpdateTime=2026-11-17 15:33:34` |
| Build / Vulkan | `VulkanPresenter: … presentation mode **2**` (= FIFO) em `pace_and_vulkan_grep.txt` |
| Artefactos | `power_timeseries.csv`, `collection_meta.txt`, `stats_computed.json`, `pace_and_vulkan_grep.txt`, `battery_dumpsys_end.txt`, `thermal_zones_end.txt`, `simpleperf_top.txt`, `simpleperf_meta.txt` |

## Sensores — caminhos e unidades

| Grandeza | Fonte | Unidade verificada | Notas |
|----------|--------|-------------------|--------|
| Corrente | `/sys/class/power_supply/battery/current_now` | **µA** | Negativo = componente de descarga no sysfs; com **AC** reflecte corrente líquida, **não** descarga pura |
| Tensão | `/sys/class/power_supply/battery/voltage_now` | **µV** | |
| Nível / temp. | `dumpsys battery` | nível %, temp. **0,1 °C** | Ex.: `temperature: 461` → 46,1 °C |
| GPU freq. | `/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq` | Hz | |
| GPU ocupação | `/sys/class/kgsl/kgsl-3d0/gpubusy` | `busy total` (contadores cumulativos desde boot) | Rácio `busy/total` ≈ % tempo GPU ocupada (vida útil da amostra) |
| PACE / frame | `PROBE PACE_821C0770` / `frame_us=` em `sonic4_050.log` | Hz / µs | Janela de log ~5 s no guest |

**Potência calculada:** `P_mW = current_uA × voltage_uV / 10⁹` — apenas proxy com AC ligado; **não** usar para A/B de drenagem sem mesmo estado de carga.

**Condição de carga durante a coleta:** `AC powered: true`, `status: 2` (a carregar), `level: 18`, `temperature: 461` (46,1 °C). Baseline Exp-001 tinha `level: 7` no mesmo `status: 2` — **correntes não são equivalentes** entre corridas.

## Desempenho (PACE / frame)

### Amostras do script (`power_timeseries.csv`, n=23, **23/23 PACE válidos**)

| Métrica | Mín | Máx | Média | Mediana | σ |
|---------|-----|-----|-------|---------|---|
| **PACE (Hz)** | 24,7 | 43,1 | 37,38 | 38,75 | 4,95 |
| **frame_us** | 19 890 | 30 858 | 22 360 | 21 576 | 2 921 |

### Log contínuo na mesma janela (`pace_and_vulkan_grep.txt`, amostras a cada ~5 s)

- Antes da queda: ~38–40 Hz estável.
- **15:40:05–15:40:20:** queda a **24,7–30,4 Hz**, `frame_us` até ~31 ms (coincide com GPU a **510 MHz** no CSV).
- Recuperação até **~43,1 Hz** (mediana do pico) até ao fim da coleta.

**Interpretação:** o main loop guest continua a ~**37–40 Hz** em média (objectivo 60). FIFO **não** força 60 Hz nem elimina frames enquanto o guest emite ~40 Hz. Sonic parado **não** reduz PACE — confirma render/issue contínuos no guest, não só apresentação Vulkan.

## GPU

| Métrica | Mín | Máx | Média | Mediana |
|---------|-----|-----|-------|---------|
| **cur_freq (Hz)** | 510 M | 937 M | 632 M | 644 M |
| **gpubusy lifetime %** (`busy/total`) | — | — | **~72,0** (média 23 amostras) | ~72,5 |

- Frequência domina **644 MHz** com picos **763 / 937 MHz** e vale **510 MHz** durante a queda de PACE.
- Ocupação GPU permanece **alta (~72 %)** — alinhado com diagnóstico anterior (render completo por frame guest).

`gpubusy_pct_5s` por delta entre linhas **não é fiável** nesta corrida (contadores reset/wrap em alguns intervalos); usar **lifetime ratio** acima.

## Energia / térmico

| Métrica | Valor (média) | Notas |
|---------|---------------|--------|
| `current_uA` | −923 043 | AC ligado — comparar só qualitativamente |
| `voltage_uV` | 3 759 000 | |
| Proxy `P_mW` (I×V) | ~3470 mW | **Não** é drenagem certificada com carregador |
| Temp. bateria | 45,9–46,0 °C (série CSV) | |
| Zonas térmicas | `thermal_zones_end.txt` | CPUs ~66–73 °C (mili°C no sysfs); sem evidência clara de throttling severo durante os 120 s |

## CPU — Simpleperf (60 s, **após** a janela de 120 s, jogo **não** reiniciado)

Ficheiro: `simpleperf_top.txt` (PID 15229, 1 215 545 amostras).

| Símbolo | Exp-003a | Baseline Exp-001 (`simpleperf_top.txt`) |
|---------|----------|----------------------------------------|
| `sub_821C62C8` | 7,27 % | 7,51 % |
| `sub_821C4870` | 5,74 % | 5,86 % |
| `QueryGuestTickCount` | 2,59 % | 3,27 % |
| `__kernel_clock_gettime` | 3,28 % | 3,76 % |

Perfil **essencialmente igual** ao baseline — FIFO não alterou de forma material o hotspots de CPU guest/scheduler.

## Comparação A/B (mesma cena: 1.ª fase parada)

| Experimento | Apresentação | PACE típico | GPU MHz | gpubusy | Corrente | PACE no CSV |
|-------------|--------------|-------------|---------|---------|----------|-------------|
| **Exp-001** `20261117_exp001_cntvct` | MAILBOX (build anterior) | ~40–43 Hz estável (`pace_timeseries.txt`) | ~644 M no snapshot | não na série longa | N/A série 120s | sim (log) |
| **Exp-002** `exp002_postfix` / `exp002_revalidate` | MAILBOX (Exp-002) | — | 644–873 M | colunas GPU partidas no script antigo | −2,0 a −2,7 M µA | **vazio** |
| **Exp-003a** (esta corrida) | **FIFO (modo 2)** | 37,4 Hz média; 24,7–43,1 Hz | 510–937 M; med. 644 M | **~72 %** | −873 a −942 k µA (**AC**) | **23/23** |

### O que a mudança para FIFO **não** fez (com evidência)

1. **Não** estabilizou PACE em 60 Hz — continua limitado pelo guest (~40 Hz).
2. **Não** reduziu ocupação GPU (~72 %) nem eliminou picos de frequência (937 MHz ainda observado).
3. **Não** mudou o perfil CPU de forma relevante vs Exp-001.

### O que FIFO **pode** ter alterado (hipótese fraca nesta corrida)

- Comportamento de fila de apresentação vs MAILBOX (menos frames descartados na swapchain, mais bloqueio em `vkQueuePresent`) — **sem métrica directa de blocking** nesta coleta.
- Corrente sysfs **não comparável** (AC, nível 18 % vs corridas Exp-002 com magnitudes diferentes).

## Limitações

1. Coleta com **carregador AC** — `current_now` não reflecte drenagem autónoma.
2. Amostragem PACE alinhada a poll de 5 s + `tail` do log (pode desfasar 0–5 s do instante sysfs).
3. Queda de PACE a ~25 Hz **durante** os 120 s — possível interacção DVFS / ADB wireless / carga; **não** atribuída só a FIFO.
4. Simpleperf **60 s imediatamente depois** — leve overhead possível no jogo, mas sem reinício.
5. Exp-002 sem PACE/GPU fiáveis no CSV antigo — comparação parcial.

## Conclusões (suportadas por dados)

1. **Exp-003a (FIFO) está confirmado em runtime** (`presentation mode 2`).
2. Na 1.ª fase parada, o pipeline guest **continua a submeter ~37–40 frames/s** com GPU **~72 %** ocupada — o gargalo de potência **não** é só o modo de apresentação Vulkan.
3. **FIFO, isoladamente, não melhora** PACE nem reduz carga GPU de forma mensurável vs baseline de cena fixa; uma queda transitória de PACE ocorreu com GPU a 510 MHz.
4. O próximo experimento de **maior valor** (prioridade): reduzir trabalho de render **no guest/host** quando a cena está estática (p.ex. detecção de idle / menos `IssueSwap` ou skip de draw sem alterar gameplay), ou atacar `sub_821C62C8` / loop de tempo — **não** outra troca só de `VkPresentModeKHR`.

## Próximo passo sugerido (sem executar agora)

- **Exp-004 candidato:** throttle de apresentação ou batching no presenter **condicionado** a frame duplicado / sem delta de input na 1.ª fase parada, com A/B em bateria (AC desligado, nível ≥30 %) e mesmas métricas.

---

*Jogo mantido em execução após coleta; utilizador pode continuar na 1.ª fase.*