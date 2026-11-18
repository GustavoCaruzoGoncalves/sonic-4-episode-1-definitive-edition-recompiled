# H1 A/B — 1.ª fase, Sonic parado (workload gate)

**Gate:** `draws_per_present` 175–205, `draws/s` ≥ 5500, `barriers/s` ≥ 6500, `last_present_draws` ≥ 120.

| Build | Label | Valid samples | `draws_per_present` | **median `record_us_pf`** |
|-------|--------|---------------|---------------------|---------------------------|
| H1 **ON** (`REX_BATCH_DRAW_RESIDENCY=ON`) | `h1_on_gated` | 4/4 | ~198 | **7263 µs** |
| H1 **OFF** | `h1_off_gated` | 4/4 | ~198 | **7230 µs** |

## Verdict — performance

**INCONCLUSIVE / sem efeito mensurável** neste A/B (~0,5 % diferança, ruído térmico).

O alegado −30 % e ~59 Hz da Exp-005 **não se repetem** com cena comparável (1.ª fase ~198 draws/present). Valores alinhados ao Exp-004 (~7,2 ms).

## Status H1

| | |
|--|--|
| Implemented | Sim |
| Correctness | Sem regressão óbvia em screenshot; A/B OFF não piora |
| Performance validated | **Não** — manter ON por semântica (menos chamadas `RequestRanges`); não contar como win |

## Próximo

Ver `PRIMITIVE_INDEX_RESIDENCY.md` (Exp-006). H1 e index-defer **não** reduziram trabalho mensurável; foco estrutural: ~198 draws/present, `record_us_pf` ~7 ms, guest ARM64 + Vulkan record escala com draws.