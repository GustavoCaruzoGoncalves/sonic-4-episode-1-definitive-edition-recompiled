# Contributing

Thank you for helping improve **Sonic 4 Episode 1 — Definitive Edition Recompiled**. This is an
experimental systems project; careful review beats large unvalidated changes.

## Before you start

1. Read [README.md](README.md) and [docs/performance/CURRENT_STATUS.md](docs/performance/CURRENT_STATUS.md).
2. Do **not** commit or upload **game assets**, XEX files, dumps, or screenshots containing
   copyrighted in-game art. See [docs/legal/DISTRIBUTION_POLICY.md](docs/legal/DISTRIBUTION_POLICY.md).
3. Large SDK changes usually belong in `third_party/rexglue-sdk/` with a short experiment note in
   `docs/experiments/` when measurable.

## How to contribute

- **Issues**: reproducible bugs, profiling captures (redacted), architecture questions.
- **Pull requests**: keep diffs focused; explain measurement or rationale for performance changes.
- **Discussions**: use GitHub Discussions if enabled, or issues for design threads.

## Development setup

- [docs/building/ANDROID.md](docs/building/ANDROID.md) — primary platform
- [docs/building/WINDOWS.md](docs/building/WINDOWS.md) — host / D3D12 path

## Coding expectations

- Match surrounding C++ style in ReXGlue and `pilot/`.
- Prefer **cvars** or CMake options for experimental behavior (see existing `Exp-*` patterns).
- Document **test conditions** (device, scene, stationary vs moving, telemetry on/off).
- Do not enable optimizations by default unless a controlled A/B supports it.

## Profiling on Android (POCO F7 workflow)

Scripts under `evidence/ANDROID_PORT/POCO_F7_AUTONOMOUS_ENGINEERING/scripts/` (e.g.
`measure_threads.sh`, `analyze_run.sh`) expect a telemetry build and a fixed first-stage workload
when comparing numbers.

## Code of conduct

This project follows [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## Security

Report sensitive issues per [SECURITY.md](SECURITY.md) (no public game piracy or credential leaks).