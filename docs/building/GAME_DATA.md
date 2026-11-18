# Game data setup (user-supplied)

You must provide files from a **legally obtained** Xbox 360 copy of *Sonic the Hedgehog 4: Episode I*
(Title ID **`0x58410A07`**). This repository **never** includes those files.

## Expected layout (local only)

```
game/
  58410A07/000D0000/<STFS_CONTAINER_HASH>   # raw STFS (optional if already extracted)
  extracted/
    default.xex
    ... game assets as extracted from STFS ...
```

`pilot/sonic4_manifest.toml` references:

```toml
game_root = "../game/extracted"
file_path = "../game/extracted/default.xex"
```

Adjust paths if your layout differs.

## Extract STFS container

From repository root (Python 3 required):

```powershell
python third_party\360tools\tools\extract_stfs.py `
  "PATH\TO\STFS_CONTAINER" `
  game\extracted
```

**Verification:**

- `game\extracted\default.xex` exists
- First four bytes of the XEX are `XEX2`
- File list is consistent with your package (compare mentally to `evidence/stfs_filetable.txt` if
  present locally)

## What the runtime loads

- `default.xex` — recompiled entry and guest image
- VFS root `game:\` maps to your extracted folder (ReXGlue host path device)
- Shaders: transcode/cache via ReXGlue build (`shaders.xex` sidecar in build output)

## Do not

- Commit `game/` to git (see `.gitignore`)
- Upload STFS/ZIP/XEX to issues, PRs, or Releases
- Share extracted folders publicly

For legal context: [../legal/DISTRIBUTION_POLICY.md](../legal/DISTRIBUTION_POLICY.md).