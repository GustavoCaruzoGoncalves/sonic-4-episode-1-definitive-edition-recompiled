# T7_SPEED_MASTER

Investigação da velocidade de jogo variável em Sonic 4 Episode I recompilado.
O executável anterior à mudança está em `sonic4_before_master.exe`.

## Causa

O fio de tarefas `sub_82488B10` despachava a lista de objetos duas vezes por quadro.

O loop principal, a ~59 Hz, faz duas coisas no fim de `sub_8249E2F8`:

1. Acorda o fio com `NtSetEvent` no evento do temporizador `0x824F0FA8`.
2. Empurra o prazo em `+24` para `agora + 833350` ticks (1/60 s a 50 MHz) via `sub_82488738`.

`sub_82488958` devolve o controle ao despacho em dois casos: o evento foi sinalizado, ou o prazo venceu. No Xbox os dois instantes coincidem e há um despacho por quadro. No host o prazo vence alguns milésimos de segundo depois do sinal. O fio, ocupado noutro núcleo, completa o primeiro despacho e volta a entrar na espera com o prazo já vencido. A função retorna de imediato e `sub_8249ECA8` corre outra vez.

Medição no menu, antes da correção (janelas de 5 s, `probe8.log`):

| Medida | Valor |
|---|---|
| Despachos do fio (`LR 82488B70`) | 114/s |
| Despachos do loop principal (`LR 82164718`) | 59/s |
| `sub_820C0178` (trampoline de objeto) | ~1940/s, 17 objetos × 114 Hz |
| Esperas | metade < 3 ms, metade 16,6 ms |
| Prazo gravado por `sub_82488738` | 833350 ticks, 60,00 Hz |
| Relógio guest | 50,00 MHz, razão 1,000 |

A variação entre áreas sai daqui. O segundo despacho só acontece se o primeiro terminar antes de o prazo ser empurrado. Cena leve (menu, Sonic parado): os dois despachos cabem no quadro e a simulação anda a ~2×. Cena pesada: o primeiro despacho ocupa o intervalo e o segundo não chega a ocorrer. Daí a aceleração de ~1,2–1,35× nalguns sítios e o salto quando a zona fica mais leve.

O trampoline `sub_820C0178` não está na lista dos 59 Hz. Os callbacks `8215B6A8` e `82188D88` acompanham o loop principal. Os objetos da cena acompanhavam o fio a 114 Hz.

## O que foi excluído nesta sessão

- Relógio guest e `guest_time_scalar`. O campo 21792 de `sub_821D30E0` soma o tempo real (`sim_ratio` 1,000).
- Frequência do loop principal. `sub_8249E2F8` já estava limitado a ~59 Hz e o intervalo pedido a `sub_82488738` é 16667 µs, convertido para 833350 ticks.
- `sub_821D2478` continua sem chamadas em jogo. Não é o passo da simulação.
- Trocar a ordem “sinal, depois prazo” por “prazo, depois sinal” em `sub_8249E2F8`. No instante do sinal o prazo já fica 833343 ticks à frente (`probe10.log`) e o duplo despacho continua. O fio observa o prazo antigo porque já está a meio da espera.

## Correção

Em `sub_82488958` (`pilot/generated/default/sonic4_recomp.1.cpp`), quando o prazo vence, a função já não volta ao despacho. Rearma o prazo (o catch-up que já existia em `loc_82488A20`) e continua à espera do sinal. Um segundo vencimento na mesma chamada devolve o controlo, para um sinal perdido não prender o fio.

Em `sub_8249E2F8` (`sonic4_recomp.12.cpp`) o prazo passa a ser publicado antes do `NtSetEvent`. Sozinha, esta troca não eliminou o segundo despacho. Fica porque o valor observado no sinal passa a ser o quadro seguinte.

Os dois ficheiros são gerados. O codegen atual não os reescreve (`0 unchanged`). Uma regeneração do módulo apaga a correção.

## Validação no menu

`probe11_fix.log`, janelas estáveis por volta de 19:30:30:

| Medida | Antes | Depois |
|---|---|---|
| Despachos do fio | 114/s | 58,8/s |
| Despachos do loop principal | 59/s | 58,8/s |
| `sub_820C0178` | ~1940/s | 1003/s = 17 × 58,8 |
| Esperas curtas (< 3 ms) | ~270 / 5 s | 0 |
| Duração da espera | 0,1 ms ou 16,6 ms | 14,9–20 ms, uma por quadro |
| Relógio guest | 1,000 | 1,000 (49 999 635 Hz) |

O scheduler `sub_821C0770` manteve 59,7/s com `r29_nonzero = 0`. Não houve segundo passo no agendador de quadros.

## Validação em jogo

Sessão fechada pelo jogador às 19:33, com a velocidade considerada correta. Em `probe11_fix.log`, de 19:31 a 19:33, a cena passou de 17 objetos no menu para cerca de 100 (`sub_820C0178` ~30 000 chamadas em 5 s). O fio manteve-se a 60 despachos por segundo, igual ao loop principal. Esperas abaixo de 3 ms: zero nas janelas estáveis.

Não há instalação do Sonic 4 de Windows nos caminhos Steam e XboxGames verificados. A comparação foi feita contra o comportamento já conhecido do jogo, não contra um executável oficial lado a lado.

Funções já excluídas nas iterações anteriores e não alteradas: `sub_821C1C80`, `sub_821C30D0`, `sub_8249D308`, `sub_8249CC00`, `sub_821B35C8`.
