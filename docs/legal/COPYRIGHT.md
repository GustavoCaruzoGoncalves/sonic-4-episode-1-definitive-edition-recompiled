# Copyright and trademarks

## Unofficial status

**Sonic 4 Episode 1 — Definitive Edition Recompiled** is an independent, community engineering
project. It is **not** affiliated with, endorsed by, sponsored by, or supported by **SEGA** or any
other rights holder of the original game.

## Trademarks

- **SONIC THE HEDGEHOG** and related names, logos, and trademarks are properties of their
  respective owners (including SEGA).
- This project uses those names **only** to identify the subject game and engineering goal (nominative
  fair use / descriptive use). **No** official endorsement is claimed.

## Game content

The original *Sonic the Hedgehog 4: Episode I* game, including but not limited to executables,
archives, music, sound effects, voice, textures, models, animations, UI art, and level data, remains
the exclusive property of its rights holders.

This source repository contains:

- Tooling, runtime patches, and recompilation **infrastructure**
- **No** retail game assets intended for redistribution

Users must obtain and extract game data from a **lawfully acquired** copy for local use.

## Generated / transformed code

Static recompilation produces C++ from the game’s PowerPC machine code. Legal treatment of
decompiled or recompiled output varies by jurisdiction. This project:

- Does **not** distribute generated game function bodies in the public repository (`pilot/generated/`
  is gitignored; users run `rexglue codegen` locally).
- Publishes **patches** to open-source runtime components under their licenses.

**This document is informational, not legal advice.**

## SEGA fan content

We have **not** verified a current, project-specific approval from SEGA for this recompilation.
Community projects often adopt these **precautions** (similar in spirit to
[Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp)):

- No distribution of copyrighted assets
- Clear “unofficial / not affiliated” labeling
- Non-commercial, no-charge source release
- No implication of official endorsement

SEGA’s public fan-creator policies change over time. Contributors should review current SEGA
guidance independently before any public binary distribution **they** perform—this repository
does not ship game binaries.