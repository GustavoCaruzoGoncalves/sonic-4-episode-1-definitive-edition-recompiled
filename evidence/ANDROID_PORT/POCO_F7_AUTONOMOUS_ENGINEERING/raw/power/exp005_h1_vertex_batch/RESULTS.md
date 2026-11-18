# Exp-005 — H1 batched `RequestRanges` (vertex fetch + shared memory scratch)

**Patch:** `IssueDraw` batches all vertex-buffer `RequestRange` calls into one `RequestRanges`; memexport ranges merged into the same batched call; `SharedMemory::request_ranges_merge_scratch_` reuse; skip redundant fetch-constant register writes/invalidations.

**Cena:** 1.ª fase, Sonic parado, ~189 draws/present.

## Telemetria (`PROBE RENDER_VK`)

| Janela | `record_us_pf` | `present_hz` | `frame_us` (PACE) | Notas |
|--------|----------------|--------------|-------------------|--------|
| **Baseline Exp-004** | ~6800–7500 | ~39–44 | ~20000 | `exp004_telemetry_fixed` |
| **Exp-005a heavy** (`sonic4_057`, pico térmico ~415 °C) | **4688–5164** | **57.9–59.6** | **13522–14782** | só batch vertex (antes de merge memexport) |
| **Exp-005b** (`sonic4_059`, ~443 °C) | 6288–6440 | 46.4–47.7 | 17680–18557 | batch vertex+memexport |
| **Exp-005c steady** (`sonic4_061`) | 7233–7467 | 38.6–40.0 | ~39 Hz PACE | + noop fetch registers |

## Estado da validação (correcção 2026-11-17)

| Status | H1 |
|--------|-----|
| **Implemented** | Sim (batching + scratch + fetch noop + CMake) |
| **Correctness** | Parcial (screenshot 1.ª fase sem corrupção óbvia; sem suite formal) |
| **Performance** | **NÃO VALIDADO** — comparações anteriores podem misturar menu vs 1.ª fase ou condições térmicas; **não** usar −30 % `record_us_pf` como facto |

Revalidação obrigatória: `scripts/validate_first_stage_workload.ps1` (gate `draws_per_present` 175–205) + A/B control vs H1-on no **mesmo** regime.

## Notas históricas (não usar como prova)

Medições antigas nesta pasta podem incluir janelas de menu (`draws_per_present` &lt; 80) e janelas de 1.ª fase na mesma sessão — tratar apenas como leads, não como conclusão.

## Ficheiros alterados

- `third_party/rexglue-sdk/src/graphics/vulkan/command_processor.cpp`
- `third_party/rexglue-sdk/src/graphics/d3d12/command_processor.cpp`
- `third_party/rexglue-sdk/src/graphics/shared_memory.{h,cpp}`
- `pilot/CMakeLists.txt` (`file(TO_CMAKE_PATH)` para codegen Windows)