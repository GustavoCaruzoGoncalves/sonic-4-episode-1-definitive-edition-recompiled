# Roadmap (engineering priorities)

This is a living list—not a release promise.

## P0 — Stability & measurement

- [ ] Reproducible benchmark harness (first stage: stationary + forward play scripts)
- [ ] Align vendored MMIO write-watch path with upstream [rexglue-sdk#441](https://github.com/rexglue/rexglue-sdk/issues/441)
- [ ] Fix shutdown crash on `KernelState::Terminate`
- [ ] Document official asset deployment path for Android installs

## P1 — Stutter & GPU scheduling (Exp-018 follow-ups)

- [ ] A/B more swapchain images / present mode (see Exp-003a history)
- [ ] Decouple paint/wait from guest submission queue (present-queue semaphore handoff)
- [ ] Reduce `vkWaitForFences` stalls in `BeginSubmission`
- [ ] Profile render-to-texture reload / untile path (~3.8 ms GPU/frame)

## P2 — CPU efficiency

- [ ] Guest spin waiting on GPU ring (`sub_821C62C8` region) — investigate fence semantics
- [ ] Reduce submissions/frame (~7.7) where safe
- [ ] Audit global critical region contention after vblank fix

## P3 — Platform parity

- [ ] Windows D3D12 full playthrough on current SDK
- [ ] Track upstream ReXGlue releases; plan patch queue or fork publication

## P4 — Community

- [ ] Contributor guide for minimal telemetry captures (no copyrighted screenshots in git)
- [ ] Issue templates for perf regressions with required counters (`PROBE RENDER_VK`, `PROBE SPIKE`)

## Non-goals (for now)

- Public binary releases with bundled game data
- GLES backend production port (feasibility doc only)
- Claiming “Definitive Edition” feature parity with any official product