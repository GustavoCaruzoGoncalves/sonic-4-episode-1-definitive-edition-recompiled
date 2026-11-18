# Exp-003 — Adreno 825 frequência elevada (1.ª fase, parado)

## Semântica de medição

| sysfs / campo | Unidade | Notas |
| --- | --- | --- |
| `battery/current_now` | **µA** (microamperes) | Negativo = corrente que **descarrega** a bateria; com AC, valor é **componente líquida**, não potência certificada do SoC. |
| `battery/voltage_now` | **µV** | Multiplicar **apenas** como proxy: P ≈ \|I\|×V. |
| `kgsl-3d0/gpubusy` | dois contadores | **busy/total** no intervalo do driver (~% tempo GPU ativo). |
| `kgsl-3d0/devfreq/cur_freq` | Hz | Relógio **solicitado/atual** do Adreno. |
| `presentation mode 1` | `VK_PRESENT_MODE_MAILBOX_KHR` | Swapchain actual (log `sonic4_049.log`). |

## Respostas obrigatórias

### 1. ~42 frames completos por segundo com cena estática?

**Sim.** PACE sustentado **~40–43 Hz** na 1.ª fase parado (`baseline_first_stage`, `exp002_postfix`). O loop guest continua; “parado” só no input, não em simulação/render.

### 2. GPU executa trabalho substancial cada frame?

**Sim.** `gpubusy` típico **~60–76 %** (ex.: 767539/1010187 ≈ 75,9 %). Threads `GPU Commands` / Vulkan pipelines activas em `simpleperf`. Cada `IssueSwap` na CP conduz resolve/composite/present.

### 3. Vulkan present/sync impede estados de baixa potência?

**Parcialmente contribui.** **MAILBOX** (mode 1) mantém pipeline de apresentação com buffers extra e submissões ao ritmo guest (~42 Hz), sem amarrar ao painel como **FIFO**. O relógio GPU sobe com bursts sustentados (~873 MHz). **FIFO** pode melhorar sync com display (Xendroid+ HyperOS); não elimina custo de **render** por frame guest.

### 4. `gpubusy` é representativo?

**Sim, como ocupação Adreno**, não como “só pixels úteis”. É a métrica oficial kgsl (busy cycles / total). Não distingue overdraw vs clear — apenas actividade do shader/RB.

### 5. Causa da frequência alta?

| Factor | Peso |
| --- | --- |
| **Carga de render sustentada** (~42 full frames/s) | **Principal** |
| **Governor DVFS Adreno** (responde a ~75% busy) | **Principal** |
| **MAILBOX / compositor** | Secundário (sync) |
| Driver bug isolado | Não evidenciado |

## Exp-003a (isolado): FIFO no Android

**Hipótese:** preferir `FIFO` / `FIFO_RELAXED` em vez de `MAILBOX` reduz pressão no compositor e permite melhor idle entre frames do **painel** (60 Hz), sem alterar simulação guest.

**Mudança:** `vulkan_presenter.cpp` — ramo `#if REX_PLATFORM_ANDROID`.

**Não faz:** saltar frames guest, reduzir PACE artificialmente, ou cortar render na build retail.

## Validação Exp-002 (repetida)

| Run | \|current_now\| médio (µA) | GPU MHz médio |
| --- | ---: | ---: |
| `exp002_postfix` | 2 148 181 | 767.5 |
| `exp002_revalidate` (AC) | 2 465 181 | 671.8 |

Variação ~15 % → **não** tratar uma corrida como certificação; usar séries 120 s e mesma cena.

## Próximo passo

Instalar Exp-003a, confirmar log `presentation mode 2` ou `3` (FIFO / FIFO_RELAXED), repetir `measure_power_poco.ps1` 120 s na 1.ª fase parado.