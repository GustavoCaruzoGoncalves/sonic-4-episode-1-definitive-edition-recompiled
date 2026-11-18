# Exp-012 — hoist shared-memory uploads out of the render pass

**Date:** 2026-11-17 · **Device:** POCO F7 (`192.168.70.7:36067`) · first stage, stationary, 189 draws/present

## Attribution (new telemetry `PROBE RP_BREAK`, classic render passes, hoisting off)
Per present: **~84 render-pass begins**. Ends caused by: shared-memory upload **57.6**, EDRAM
resolve 12.0, submission/swap 6.6, framebuffer switch 4.0, other barriers 3.0, texture 0.3.
Uploads: ~58 per present, ~1.6 MiB per present (dynamic CPU-written vertex/index data).

## Change (cvar `shm_upload_hoist`, default true)
- `DeferredCommandBuffer`: side stream spliced in at a marked position (render-pass begin).
- `VulkanCommandProcessor`: marks the hoist point at each guest render-pass begin, splices at
  every end; records the shared-memory byte ranges each draw reads (all vertex fetch constants
  incl. already-resident ones, guest-DMA index range); flags render passes containing memexport.
- `VulkanSharedMemory::UploadRanges`: snapshot first (memcpy to upload buffer), then hoist
  barrier+copy+barrier before the render pass **only if** every byte the draws already in the
  pass read from the uploaded pages is identical to the previous snapshot of that page in this
  submission (`memcmp`), no draw in the pass wrote shared memory, and usage state is plain read.
  Otherwise legacy path (end render pass). GPU writes (`RangeWrittenByGpu`: resolves, memexport)
  drop page snapshots via a new `SharedMemory::OnRangeWrittenByGpu` hook.

## Result — controlled A/B, same APK, both stationary 189 draws/present, ~12 min apart, GPU 937 MHz

| Metric | ON `exp012_hoist_on` | OFF `exp012_hoist_off` |
|---|---:|---:|
| Uploads hoisted / present | 30.3 of 57.8 | 0 |
| Render-pass begins / present | **53** | 83 |
| Process CPU | 1.82 cores | **1.63 cores** |
| GPU Commands thread | 0.484 | 0.390 |
| `issue_draw_us_pf` | 4164–4311 µs | 2622–3503 µs |
| `record_us_pf` | 1532–1590 µs | 1421–1714 µs |
| GPU busy | 77 % | 78 % |
| PACE | 59.6–59.7 Hz | 59.7 Hz |

Visual (ON): `raw/threads/exp012_hoist_on/screen.png` — correct.

**Verdict: FAILURE.** Fewer render passes, but net CPU cost went up (per-draw read tracking,
`GetVertexFetch` for already-resident fetch constants, snapshot `memcmp`/map upkeep) and GPU busy did
not move. After Exp-011 (classic render passes) a render-pass restart is cheap on this driver, so the
work removed is smaller than the bookkeeping added. `shm_upload_hoist` now defaults to **false**; code
kept behind the flag, `PROBE RP_BREAK` attribution telemetry kept.

Lesson: re-measure the cost of a cause after the previous fix — Exp-011 changed what a render-pass
break costs.
