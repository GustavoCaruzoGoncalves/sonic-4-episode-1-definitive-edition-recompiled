# Desempenho no POCO F7

Medido em 16 de novembro de 2026. APK instalado ao fim desta passagem: `C:\S4EP1Recomp\android\dist\Sonic4-arm64-v8a.apk`, compilado com `-O2 -fno-inline -fno-strict-aliasing` no código recompilado. O processo `11333` continuava vivo e o log `sonic4_011.log` não tem `[FATAL]`.

## Linha de base (`-O1`)

O build anterior, só com `-O1`, usava cerca de 411% de CPU (quatro threads do guest perto de um núcleo cada: `XThread`, `Main XThread`, `Audio Worker`, `SDLThread`). A GPU Adreno 825 estava em 75% a 644 MHz. Status térmico Android: 0. CPU na casa de 65–72 °C, GPU na casa de 65 °C. Não havia throttling reportado pelo `thermalservice`, mas o aparelho esquenta porque a CPU fica saturada.

Sonda do guest, alvo 60, em `sonic4_003.log`: 49, 60, 55 e 51 no começo; depois 28–34 enquanto pipelines ainda nasciam. O relógio do guest ficou perto de 50 MHz. O áudio é o `Audio Worker` do guest mais o callback SDL. Com a CPU saturada, a fila de frames esvazia e o callback preenche silêncio. Isso é o corte que se ouve. O callback também segura o mutex durante a conversão.

## O que o `-O2` e o `-O3` fizeram

Clang 19 do NDK r28 compila os TUs em `-O3` sem a flag `-fasync-exceptions` que derrubava o compilador antes. O crash antigo do compilador não se repetiu.

No aparelho, `-O3` e `-O2` com inline chegaram a 59,5 atualizações por segundo e em seguida abortaram:

`Call to invalid or unregistered function at guest address 0x8218FB50 lr=0x8249ECDC ctr=0x8218FB50`

`0x8249ECDC` é o `bctrl` de `sub_8249ECA8` (`sonic4_recomp.61.cpp`). `0x8218FB50` cai dentro de `sub_8218FB38`, que termina em `bctr` e não é um ponto de entrada registrado. O ponteiro já vem errado da memória do guest: deixar só o arquivo `.61` em `-O1` não evitou o aborto. `-O1 -finline-functions` também abortou no mesmo endereço. `-O2 -fno-inline-functions` também.

## Build que ficou instalado

`-O2 -fno-inline -fno-strict-aliasing` em todos os TUs recompilados. Não abortou. Sonda em `sonic4_011.log`:

| Hora | Atualizações/s |
| --- | ---: |
| 21:31:41 a 21:31:56 | 52, 56, 60, 59 |
| 21:32:01 | 52 |
| 21:32:06 em diante, até 21:33:11 | 35, 32, 38 e depois 30 estável |

Relógio do guest: razão 0,992–0,994 contra 50 MHz. A simulação não foi acelerada. O desempenho sustentado continua o do `-O1`. O salto para 60 Hz dependia do inline, e o inline é o que grava o ponteiro de tarefa inválido.

Um PSO de cerca de 500 ms ainda nasce no thread de apresentação nas builds otimizadas (`worker=0`). O cache de driver continua pequeno. Isso explica soluços, não os 30 Hz contínuos.

## O que não foi mudado

Ritmo do guest, física, Title ID `58410A07`, licença (`flag160=0`), Vulkan, landscape, toque, caches e os backends de Windows.
