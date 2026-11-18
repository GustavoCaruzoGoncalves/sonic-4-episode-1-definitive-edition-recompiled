# Distribution policy

## What this repository distributes

- Source code for the recompilation pilot, Android wrapper, documentation, and **modified vendored
  ReXGlue SDK**
- Benchmark **reports** and scripts (not raw game captures where avoidable)

## What we do **not** distribute (and you must not commit)

| Prohibited | Examples |
|----------|----------|
| Game binaries | `default.xex`, STFS containers, GOD/ZIP packages, Title ID folders (`58410A07/...`) |
| Extracted retail assets | Textures, audio, CPK/CSB, levels, meshes under `game/` |
| ISO / disc dumps | Xbox 360 disc images |
| Decrypted or ripped content | Any data extracted from a copy you do not own |
| Pirated installers | Third-party “完整版” repacks |

The repository `.gitignore` lists common patterns (`game/`, `*.xex`, `*.zip`). **Ignoring a path does
not remove files already committed**—verify with `git ls-files` before publishing.

## User-provided game data

You must:

1. **Legally purchase or otherwise own** the Xbox 360 version of *Sonic the Hedgehog 4: Episode I*
   (Title ID `0x58410A07`).
2. Extract the STFS container locally. See [../building/GAME_DATA.md](../building/GAME_DATA.md).
3. Point `pilot/sonic4_manifest.toml` and `game_root` at your **local** extracted tree.
4. Run `rexglue codegen` to generate `pilot/generated/` on your machine.

We do **not** provide download links for game files.

## Binary releases from this project

GitHub **Releases** with APKs or prebuilt bundles are **not** offered until the project explicitly
decides otherwise. Any future release must still **exclude** copyrighted assets and require users to
supply game data.

## Forks and downstream builds

If you fork or build binaries:

- Comply with [../../THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) (BSD, FFmpeg, SDL3, etc.).
- Do **not** bundle SEGA copyrighted material in the repository, CI artifacts, or release ZIPs.
- Mark builds clearly as **unofficial**.

## Screenshots and videos

Marketing screenshots that show in-game copyrighted art may implicate rights holders’ policies. This
repo **gitignores** evidence PNGs; avoid uploading gameplay captures to the git tree.