# Profiling diagnóstico Vulkan + áudio (POCO F7)

**Data:** 16–17 de novembro de 2026  
**Dispositivo:** POCO F7 (`25053PC47G`, Android 17, `arm64-v8a`)  
**APK:** build diagnóstico `REX_DIAG_PROFILE=1`, recomp **`-O3 -flto=thin`**, fix tail thunk preservado  
**Pacote:** `com.s4ep1recomp.sonic4`  
**Log bruto:** `diag_sceneA_ground.log`, `diag_sceneB_sky.log` (cópia completa de `files/user/logs/sonic4_017.log`)  
**Térmica pós-sessão:** `diag_thermal_post_run.txt`

---

## 1. Objetivo e restrições

Fase **somente diagnóstico**: contadores leves por janela de 5 s, sem alterar qualidade gráfica, buffers de áudio, prioridades de thread, batching ou timing guest.

---

## 2. Instrumentação implementada

### 2.1 Infraestrutura (`REX_DIAG_PROFILE`, só Android)

| Componente | Arquivos | Conteúdo |
| --- | --- | --- |
| Agregador | `third_party/rexglue-sdk/include/rex/diag/frame_stats.h`, `src/diag/frame_stats.cpp` | Janela rolante 5 s; taxas/s e médias µs |
| Vulkan | `deferred_command_buffer.cpp`, `command_processor.cpp`, `pipeline_cache.cpp` | Draws, vértices/índices, pipeline/descriptor binds, barriers, rendering passes, `record_us` / `submit_us` por present, PSO create |
| Áudio | `sdl_audio_driver.cpp`, `audio_system.cpp` | Callbacks SDL, underruns (fila vazia), profundidade da fila, `worker_disp` + tempo em `Execute` guest, waits ok/timeout |
| Guest | `pilot/src/sonic4_timing_hooks.cpp` | Tempos em `821A8488`, `82197F28`, `8249CF80`; hooks `821C62C8` / `821C4870`; linhas `PROBE DIAG_VK`, `PROBE DIAG_AUDIO`, `PROBE DIAG_GUEST` |

CMake: `REX_DIAG_PROFILE=1` em `rexruntime`, `rexgpu-xenos`, `rexaudio`, alvo `sonic4` (Windows **não** define a flag).

### 2.2 Probes existentes preservadas

`PROBE PACE`, `PROBE SCENEDISP_821A8488/s`, `SCENEUPD`, `OBJUPD`, `WAITSHAPE`, `PACE_821C0770`, etc.

### 2.3 GPU execution time

`gpu_exec_ns=na` em todas as janelas — **timestamp queries Vulkan não estão ligadas neste build**. Não inferimos tempo de GPU a partir de `record_us` / `submit_us`.

---

## 3. Metodologia de medição

1. `adb install -r android/dist/Sonic4-arm64-v8a.apk` (save preservado).
2. Entrada em Splash Hill via toques automatizados.
3. **Cenário A (~50 s):** corrida no chão (`input swipe` horizontal repetido).
4. **Cenário B (~45 s):** saltos + swipes (tentativa de “céu” com pouca geometria).
5. Extração: `run-as com.s4ep1recomp.sonic4 cat files/user/logs/sonic4_017.log`.
6. Cada `PROBE DIAG_*` corresponde a **uma janela de 5 s** alinhada às demais sondas.

**Limitação importante:** o log B é o arquivo **inteiro** (inclui A + aquecimento). As janelas “céu” (`22:43:*`) ainda mostram `SCENEDISP` **387–663/s** e PACE **~29–31 Hz** — a automação **não** reproduziu o contraste manual (~200/s ⇒ ~55–59 Hz). B e A foram **termicamente acoplados** (CPU ~58–64 °C após A).

---

## 4. Formato das sondas (referência)

### `PROBE DIAG_VK`

- `present_hz` — apresentações/s (host)
- `draws/s`, `draws_per_present_avg`, `vertices/s`, `indices/s`
- `pipeline_binds/s`, `renderpass/s`, `rendering/s`, `barriers/s`, `desc_binds/s`
- `record_us_avg` — CPU médio para gravar comandos **por present** (não é GPU)
- `submit_us_avg` — CPU médio de submissão **por present**
- `pso_create/s`, `pso_create_us_avg`
- `gpu_exec_ns` — `na` nesta sessão

### `PROBE DIAG_AUDIO`

- `sdl_cb/s` (~187/s ≈ taxa do buffer SDL)
- `cb_us_avg` — tempo no callback SDL (mix), tipicamente **11–18 µs**
- `underrun/s` — fila guest vazia no dequeue
- `queue_depth_max`
- `worker_disp/s` ≈ `wait_ok/s` — um dispatch guest por wait bem-sucedido
- `worker_disp_us_avg` — tempo em `function_dispatcher::Execute` no Audio Worker (**~1,3–1,5 ms**)
- `wait_to/s` — timeouts de wait (≈0)

### `PROBE DIAG_GUEST`

- `scenedisp_us_avg`, `sceneupd_us_avg`, `objupd_us_avg` — custo **por chamada** das funções hookadas
- `821C62C8/s`, `821C62C8_us_avg` — entradas/s e duração média do pacing loop
- `821C4870/s`, `821C4870_us_avg` — polls internos (**milhões/s**, ~0,1 µs cada)

---

## 5. Cenário A vs B — quadro comparativo

Valores de janelas **representativas** na mesma sessão (`sonic4_017.log`).

| Métrica | **Leve** (`22:41:11`, chão/transição) | **Pico pesado** (`22:41:26`, `SCENEDISP` 1195/s) | **Pesado sustentado** (`22:41:36–46`, chão) | **“Céu” auto** (`22:43:12–42`, B) |
| --- | ---: | ---: | ---: | ---: |
| `PACE_821C0770/s` | **59,5** | **28,1** | **24,3–28,1** | **28,8–31,2** |
| `SCENEDISP/s` | 212 | **1195** | 346–507 | 388–663 |
| `present_hz` | 59,5 | 28,1 | 24,3–28,1 | 28,8–31,2 |
| `draws_per_present_avg` | 34 | **310** | 213–266 | 180–287 |
| `draws/s` | 2032 | **8703** | 5192–7358 | 5617–8568 |
| `indices/s` | 0,95M | 0,22M | 1,58–2,66M | 2,4–2,9M |
| `record_us_avg` | 1665 | **10950** | 7444–8544 | 6872–9323 |
| `submit_us_avg` | 528 | 533 | 668–846 | 668–830 |
| `barriers/s` | 2976 | **9618** | 5049–6948 | 6431–8403 |
| `desc_binds/s` | 4971 | **17951** | 11036–15666 | 12268–18596 |
| `821C62C8_us_avg` | 5399 | 9217 | **31561–37491** | 27960–29015 |
| `821C4870/s` | 1,85M | **4,79M** | **4,5–4,7M** | **4,4–4,5M** |
| `worker_disp_us_avg` | 1473 | 1343 | 1308–1374 | 1392–1443 |
| `underrun/s` | 0 | 0 | 0 | 0 |

**Leitura:**

- Queda de PACE acompanha **aumento de dispatch de cena** e **explosão de draws por frame** (~34 → ~250–310), com **record CPU** subindo (~1,7 ms → ~8–11 ms por present).
- Em carga pesada sustentada, **`821C62C8_us_avg` domina o orçamento de frame** (~28–37 ms) enquanto `scenedisp_us_avg` permanece **&lt;10 µs** — o tempo não está na função de dispatch isolada, e sim no **loop de pacing** que gira milhões de vezes em `821C4870` até o relógio guest avançar.
- Áudio: **não há busy loop SDL**; o worker gasta **~1,4 ms × ~187/s ≈ 26% de um núcleo** em código guest, com **underrun ≈0** nesta sessão (fome é competição CPU + pacing lento, não buffer vazio frequente).

---

## 6. Correlação cena ↔ render

| Hipótese | Evidência nesta sessão |
| --- | --- |
| Objeto/cena guest cara **por chamada** | **Fraca:** `objupd_us_avg` raramente &gt;50 µs; pico 150 µs pontual |
| **Muitos** dispatches + draws por frame host | **Forte:** `SCENEDISP` &gt;500/s ⇒ 200+ draws/present, barriers/desc binds &gt;10k/s |
| CPU Vulkan submission | **Moderada:** `submit_us_avg` &lt;1 ms; não explica sozinha 30 ms frame |
| GPU shader bound | **Inconclusa:** sem `gpu_exec_ns`; índices altos com poucos vértices sugerem **muitos draws pequenos**, não megageometria |
| Sincronização host | **Secundária:** barriers ~7k/s em carga, mas abaixo do custo de `821C62C8` |
| Pacing / wait | **Primária em frames lentos:** `821C62C8` ≈ tempo de frame quando PACE baixo |

---

## 7. Audio Worker — o que faz

| Observação | Interpretação |
| --- | --- |
| `worker_disp/s` ≈ `sdl_cb/s` | Um tick guest de áudio por callback; **não** processa mais samples que o necessário no SDL |
| `worker_disp_us_avg` **1,3–1,5 ms** | Quase todo o custo é **recomp guest** no worker, não mix SDL (`cb_us_avg` ~13 µs) |
| `wait_to/s` ≈ 0 | Não fica bloqueado em timeout; acorda a cada buffer |
| `underrun/s` ≈ 0 (exc. boot) | Sem fome crônica de buffer **medida**; stutter perceptível alinha mais com **PACE** baixo |
| `top -H`: Audio Worker **~73%** + XThread **~97%** | Competição por CPU no mesmo cluster; **não alteramos** afinidade nesta fase |

**Não** é um busy loop no host: é **carga guest sustentada** (~26% core) **somada** à simulação principal saturando um núcleo.

---

## 8. Ghidra — funções guest relevantes

### `0x821A8488` — scene dispatcher

Filtra estado do mundo, ajusta flags e chama **`0x82197F28`** (scene update). É o contador `SCENEDISP`.

### `0x821C62C8` — frame pacing / wait loop

Compara marcos de tempo guest (`FUN_82475b5c`, campos `0x2a9c` / `0x2a90`). Enquanto o frame alvo não avançou, chama repetidamente **`0x821C4870`**; ao sair, **`0x821C47A8`** (acumula tempo em estruturas `0x5538`/`0x5540` e callback em `0x354c`).

**Callers:** `821C0E58`, `821C3580`, `821C54E8`, `821CE290`, …

### `0x821C4870` — poll curto

Lê relógio (`in_r13+0x100+0x58`), sincroniza ponteiros, chama `82223e18`; pode invocar `821D5478` se passaram &gt;4999 ticks. Retorna 1 para continuar o loop, 0 para parar. **Milhões de chamadas/s** com ~0,1 µs cada ⇒ **spin/yield** até o tempo guest.

### `0x821C47A8` — commit de tempo

Acumula delta de reloj guest e notifica callback de pacing.

**Correlação:** quando o trabalho de cena + render atrasa o avanço do relógio guest, o loop **`821C62C8`/`821C4870`** permanece ativo **todo o frame** (`821C62C8_us_avg` ≈ `frame_us`). Isso **não** é cheat de PACE — é o mecanismo original de sincronização reagindo a frames lentos.

---

## 9. CPU, térmica e agendamento

- **Status térmico:** 0 (sem throttle reportado); bateria ~39–44 °C, CPUs ~58–64 °C pós-sessão (`diag_thermal_post_run.txt`).
- **Frequência:** amostra `cpu0` **1516800 kHz** (não máximo; sessão já aquecida).
- **Threads quentes:** XThread + Main XThread ≈ **100%** de um núcleo; Audio Worker **~73%**; GPU Commands **~51%** — GPU **não** ociosa, mas CPU guest **limita PACE** antes de saturar GPU.

---

## 10. Gargalos identificados (diagnóstico)

### Primário

1. **Taxa de dispatch de cena** (`SCENEDISP` &gt; ~600/s, picos &gt;1000/s) ligada a queda de PACE.
2. **Loop de pacing guest** (`821C62C8` / `821C4870`) consumindo **quase o frame inteiro** quando o trabalho de cena/render atrasa o relógio.

### Secundário (contribui ao frame longo, não explica sozinho 60→30 Hz)

3. **Custos CPU Vulkan** — até **~11 ms record/present** em piches (~310 draws/present, ~18k desc binds/s).
4. **Audio Worker** — **~1,4 ms** por dispatch guest, competição por CPU com XThread.

### Não suportado por esta sessão

- Gargalo primário de **execução GPU** (sem timestamp queries).
- **Underrun** crônico de áudio (contador ≈0).
- **Otimização de compilador** como solução principal (build já `-O3 -flto=thin`).

---

## 11. Desconhecidos e próximos passos (com evidência nova)

1. **Habilitar timestamp queries** Vulkan (Adreno) para `gpu_exec_ns` real — separar record CPU vs GPU.
2. **Repetir cenário B manual** (céu, `SCENEDISP` ~200/s) **após cool-down**, mesma APK, 3×60 s — confirmar draws/present ~35 vs ~250.
3. **simpleperf** com `REX_DIAG_PROFILE` em janela pesada vs leve — validar tempo em `821C4870` vs `librexgpu`.
4. **Por que `SCENEDISP` &gt;60× taxa nominal** — callers `8249CF50` / `8249D0EC` / `821DC76C` nos logs LR; investigar redundância na tradução, **sem** mudar comportamento ainda.
5. Áudio: correlacionar `PROBE WAITSHAPE` com `underrun/s` e PACE (já contadores; falta gráfico temporal).

---

## 12. Artefatos

| Arquivo | Descrição |
| --- | --- |
| `diag_sceneA_ground.log` | Sessão chão + trechos leve/pesado |
| `diag_sceneB_sky.log` | Log completo + trecho `22:43` (tentativa céu) |
| `diag_thermal_post_run.txt` | Térmica / `top` pós-run |
| `ANDROID_VALIDATION_AND_PERFORMANCE.md` | Resumo executivo atualizado |
| `perf_lto.data`, `perf_baseline_o3.data` | simpleperf (sessão anterior, hotspots `821C62C8`/`821C4870`) |

**Nenhuma** otimização de produção foi aplicada nesta fase — apenas instrumentação e medição.