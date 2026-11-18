# Evidence and benchmarks

This tree holds benchmark logs, scripts, and engineering notes from local devices.

## What is committed

- Markdown experiment reports (also copied under `docs/experiments/`)
- Analysis scripts under `ANDROID_PORT/POCO_F7_AUTONOMOUS_ENGINEERING/scripts/`
- Small text summaries where useful

## What is gitignored

- `ANDROID_PORT/research_clones/` (full third-party clones — see `REFERENCE_PROJECTS.md`)
- `**/game_assets/` (may contain `default.xex`)
- `*.png`, `*.data`, large `full.log` captures (optional; see `.gitignore`)
- Any path under `game/`

## Reproducing POCO F7 numbers

See [../docs/performance/CURRENT_STATUS.md](../docs/performance/CURRENT_STATUS.md) and scripts in
`ANDROID_PORT/POCO_F7_AUTONOMOUS_ENGINEERING/scripts/`.