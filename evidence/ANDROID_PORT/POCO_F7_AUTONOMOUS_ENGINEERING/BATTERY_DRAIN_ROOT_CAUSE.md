# Drenagem de bateria — hipóteses e evidência (vivo)

**Cena:** 1.ª fase, Sonic parado. **Dispositivo:** POCO F7, wireless ADB.

## Sintoma

- Consumo percebido **superior a GRID Legends** com personagem parado (comparação informal do utilizador; sem protocolo idêntico ainda).
- PACE sustentado **~40–43 Hz** com cena estática — o jogo **não entra em idle** real.

## Medições proxy (pré Exp-002, com carregador AC)

| Fonte | Valor |
| --- | --- |
| `current_now` | **−3 057 000 µA** (componente de descarga; net com AC) |
| `voltage_now` | 3 738 000 µV |
| Potência aprox. (|I|×V) | **~11,4 W** (carga do SoC, não só bateria) |
| `kgsl gpubusy` | 701 534 / 1 161 297 (~**60 %** tempo GPU ativo) |
| GPU freq | 480–644 MHz |
| CPU top threads | Guest `XThread*`, audio, Vulkan pipeline workers |

## Causas prováveis (prioridade)

1. **Guest ARM64 a ~42 Hz com spin** — `sub_821C62C8` / `sub_821C4870` (~13 % CPU em `simpleperf`); polling de tempo mesmo parado.
2. **Espera ativa no runtime host**
   - VSync worker: `Sleep(1 ms)` → ~**1000 wakeups/s** + `QueryGuestTickCount` cada iteração.
   - `PaceGuestMainLoopFrame`: spin `MaybeYield` para restos &lt; 1 ms.
   - GPU `WAIT_REG_MEM` com `wait < 0x100`: só `sched_yield` (alto custo energético).
3. **GPU continua trabalho** — apresentação/comandos ~60 % busy; cena “parada” ainda desenha frame completo cada tick guest (~42/s), não 0.
4. **Não é interpretador PPC** — ver `CPU_EXECUTION_ARCHITECTURE_AUDIT.md`.

## Exp-002 (idle scheduling) — em teste

| Mudança | Ficheiro | Objetivo |
| --- | --- | --- |
| VSync dorme até próximo vblank (guest ticks) | `graphics_system.cpp` | ~60 wakeups/s vs ~1000 |
| Pacing: sempre `Sleep(remain_us)`, sem spin | `xboxkrnl_video.cpp` | Menos CPU em espera de frame |
| Android `WAIT_REG_MEM` curto: `Sleep(200µs)` se vsync | `command_processor.cpp` | Menos spin na thread GPU |

Validação: `scripts/measure_power_poco.ps1` + PACE na 1.ª fase parado vs `baseline_first_stage/20261117_exp001_cntvct`.