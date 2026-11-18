# PrimitiveProcessor — index `RequestRange` (investigação)

## Achado (código)

- `Process()` chamava `shared_memory_.RequestRange` **por draw** em `kGuestDMA` / `kHostBuiltinForDMA` (TODO: shared memory request cache no upstream Xenia).
- Perfil antigo atribuiu ~25% de `IssueDraw` a `PrimitiveProcessor` — **não assumido** como próximo gargalo; medição H1 mostrou que micro-otimizações de `RequestRanges` não movem `record_us_pf` na 1.ª fase.

## Alteração Exp-006 (implementada)

- `ProcessingResult::guest_index_residency_size_bytes` — pedido adiado ao command processor.
- Com `REX_BATCH_DRAW_RESIDENCY=ON`: índice entra no **mesmo** batch que vertex+memexport → **1** `RequestRanges`/draw em vez de 1+1.
- Com `OFF`: `RequestRange` imediatamente após `Process` (equivalente semântico ao código antigo).

## Status

| | |
|--|--|
| Implemented | Exp-006 |
| Correctness | Sessão sem crash óbvio; semântica preservada (range pedido antes do draw) |
| Performance | **INCONCLUSO** — `exp006_index_defer_h1on` mediana **6827 µs** vs `h1_on_gated` **7263 µs** (sessões diferentes; ~6 % dentro do ruído Exp-004) |

Não tratar como breakthrough.

## Se inconcluso

Investigar próximo: volume de trabalho (~198 draws/present), `UpdateBindings`, gravação deferred CB — não mais batching cosmethic de `RequestRange`.