# Exp-015 / 016 / 017 — progressive gameplay stutter (first stage, moving forward)

**Date:** 2026-11-17 · POCO F7 (`192.168.70.7:36067`) · telemetry builds for measurement, Release installed at the end.
**Workload B (new):** enter first stage, stand ~30 s, then play forward ~3–4 min. Analysis:
`scripts/analyze_run.sh <full.log>` (frame-time windows `PROBE FRAMES`, per-spike attribution `PROBE SPIKE`).

## New telemetry (telemetry builds only)
- `PROBE FRAMES` every 5 s: p50/p90/p99/max main-loop frame interval, counts >20/33/50 ms, guest work,
  pipeline compiles, shader translations, texture creates/loads, upload MiB, write-watch faults and handler
  time, queue-lock wait, present/acquire time, RSS and texture-cache MiB.
- `PROBE SPIKE` for every frame >33 ms with the same counters differenced over that frame.
- Monotonic counters: `rex::diag::GetCumulative()`.

## Findings (baseline `raw/threads/exp015_legacy/`, ~4.5 min play)
| Cause | Evidence | Size |
|---|---|---|
| **Pipeline compiles mid-level, every session** | worst frames 716/359/335/281/229/205 ms each contain 1–7 pipeline creations (100 ms–2.2 s of Adreno LLVM compile) | 58 compiles, 7.2 s per session |
| **Pipeline/shader storage wiped on every launch** | `.vk.xpso`/`.xsh` opened `"a+b"`, header `fread` without seek; bionic (BSD stdio) starts append streams at EOF → header read fails → storage treated as empty and truncated; `Created N pipelines from storage` never logged | root cause of the above |
| **Driver pipeline cache never persisted** | saved only at init (before pipelines exist: 32 bytes) and shutdown (never runs: hard exit) | — |
| **Write-watch fault = full `/proc/self/maps` parse** | `MMIOHandler::ExceptionCallback` → `QueryProtect` → `std::ifstream("/proc/self/maps")`; maps has ~6,350–6,720 lines and grows as watches fragment the heap | **886 µs/fault, ~170 faults/s, 150 ms/s** |
| **GPU thread blocked behind presentation** | one `VkQueue` shared; presenter holds its lock inside FIFO `vkQueuePresentKHR` (~6 ms avg, 27 ms in spikes) | **GPU thread waits 208–236 ms/s; spike frames: 21 ms of 21.7 ms "submit" is lock wait** |
| Memory | RSS 810→850 MiB, texture cache 103→170 MiB over the run; stabilizes | no leak |
| Texture reloads | ~180–230 loads/s with no new textures (render-to-texture reloads) | not addressed yet |

External: same `/proc/self/maps` fault-path problem reported upstream as
[rexglue-sdk#441](https://github.com/rexglue/rexglue-sdk/issues/441), fixed in `ded24d4` (heap re-applies host
access, no process-map read; `IsRangeFullyMapped` via `msync`). Our vendored SDK predates it.
UnleashedRecomp avoids pipeline hitches by compiling pipelines during asset loading
([README](https://github.com/hedge-dev/UnleashedRecomp)); our equivalent is boot-time pre-creation from storage.

## Changes (all default on, cvar-controlled)
- **Exp-015** `write_watch_skip_maps_query` (`src/system/mmio_handler.cpp`): Linux/Android write faults skip the
  `QueryProtect` maps parse; the access-violation callback re-checks watch flags under the same global lock and
  its recovery path restores write access if the watch was already cleared. Read faults keep the check.
- **Exp-016** (`src/graphics/vulkan/pipeline_cache.cpp`): `Seek(0)` before reading the pipeline and shader storage
  headers; storage writer thread also saves the driver pipeline cache after new pipelines (≤ once per 10 s).
- **Exp-017** `vulkan_dedicated_present_queue` (`src/ui/vulkan/vulkan_device.cpp`, `vulkan_presenter.cpp`): second
  queue in the graphics family used only for `vkQueuePresentKHR`; paint submissions stay on queue 0 (guest-output
  hand-off ordering unchanged); swapchain retirement waits for the present queue (`vkQueueWaitIdle` added to the
  device function table).

## Results
### Exp-015 / 016 (`exp015_legacy` vs `exp015_016_on`, both ~4.5 min play)
| | Legacy | Fixed |
|---|---:|---:|
| µs per write-watch fault | 886 | **56** |
| fault handler time | 150 ms/s | **9 ms/s** |
| pipeline compiles during play | 58 (7.2 s) | **2 (136 ms)** |
| worst frame | 716 ms | 201 ms |
| frames >50 ms | 0.48 % | 0.30 % |

Storage across launches: 1st fixed launch `Created 58 pipelines from storage in 3802.8 ms`, driver cache saved
959,529 B → next launch cache loaded 996,679 B, **61 pipelines in 43 ms**; Release launch 1,041,813 B, 66 in 36 ms.

### Exp-017 controlled A/B — same APK, warm caches, setprop toggle (`exp017_present_off` vs `exp017_present_on`)
| | OFF (shared queue) | ON (dedicated present queue) |
|---|---:|---:|
| GPU-thread queue-lock wait | 208 ms/s | **1 ms/s** |
| frames >20 ms | 19.2 % | **13.1 %** |
| frames >33 ms | 2.40 % | **0.73 %** |
| p90 / p99 | 23.4 / 43.0 ms | **19.6 / 30.8 ms** |
| spike frames (~4 min) | 290 | **99** |

Controlled: Exp-017 yes (same APK, consecutive sessions); Exp-015/016 session-level (different play paths) but
causally attributed by direct counters (fault µs, compile counts, storage log lines).
Visual: game played through the first stage in every run (user-driven); no corruption reported; screenshots of
earlier experiments only. **Verdicts: SUCCESS ×3.**

## Remaining (ranked)
1. In-gameplay spikes with Exp-017 on: ~60 frames / 4 min (~0.5 %), ~40 ms, with `IssueDraw` ~27 ms (vs ~4 ms):
   profile inside draw processing (texture/render-target updates, upload-pool or fence waits).
2. Texture reloads ~200/s from render-to-texture → compute untile (3.8 ms GPU/frame, Exp-013).
3. Long frames with guest idle (36–38 draws, swapchain acquire 100–180 ms) cluster like pause/transition screens.
4. Pre-existing shutdown crash on app close: `KernelState::Terminate` → `QueueUserCallback` →
   `pthread_gettid_np` on an exited thread (SIGABRT; seen 19:52 and 20:07).
5. D3D12 backend has the same `"a+b"` storage pattern (not changed; Windows CRT behaviour not verified).

## Exp-018 (diagnosis only, no fix yet) — remaining in-gameplay spikes
Telemetry: cumulative `IssueDraw` phase timers (`REX_IDRAW_PHASE` now also accumulates in telemetry builds) +
`vkWaitForFences` timer; `PROBE SPIKE_PHASES` per spike. Run `raw/threads/exp018_phases/` (`sonic4_097`, ~8.5 min,
30k frames): gt20 6.6 %, gt33 0.47 %, p99 32.2 ms.
**75 in-gameplay spikes (~35–43 ms): one `vkWaitForFences` of 20–30 ms each (avg 20.5 ms); all IssueDraw phases
together < 6 ms.** The wait is the frame-start throttle in `BeginSubmission` (waits for the frame
`kMaxFramesInFlight = 3` back), i.e. queue 0 is ~3 frames behind.
Hypothesis (not yet tested): display-driven, not GPU compute. The presenter's paint submission waits on the
swapchain acquire semaphore **on queue 0** (Android returns from acquire immediately; the semaphore signals when the
display releases the image), so in-order queue execution holds the game's later submissions behind FIFO scan-out.
In spike frames `present_us` is 30–40 ms while CPU-side acquire is < 0.2 ms.
Candidate fixes to A/B: (a) more swapchain images (`minImageCount` = max(3, surface min) at
`vulkan_presenter.cpp:1336`), (b) move the paint submission to the present queue with a semaphore from the guest-output
producer, (c) MAILBOX present mode (Exp-003a history).
