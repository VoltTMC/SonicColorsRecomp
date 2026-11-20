# Optional Music Library host binaries

Drop platform-native tools here so Sonic Colors Recomp can find them **without**
modifying `PATH` or editing application files elsewhere.

| Binary | Used for | Redistribution note |
|--------|----------|---------------------|
| `ffmpeg` / `ffmpeg.exe` | Prepare (WAV → 48 kHz); some exports | Prefer an **LGPL** FFmpeg build. Do **not** ship GPL-only builds inside a redistributable you publish. |
| `vgmstream-cli` / `vgmstream_cli.exe` | Original AAX → WAV preview/export | vgmstream is typically **ISC**; redistribute only when you comply with its licence. |

## What works without these

With only the packaged app (no Python tools):

- Browse the Music catalog (`mods/music/catalog.json`)
- Preview assigned replacement WAVs
- Play the game with an already-prepared `music_mods` cache

## Setup order

1. Run `tools/music_deps_setup.sh` (Linux) or `tools\music_deps_setup.cmd` (Windows)  
   → creates `.venv-music` and installs **cricodecs** (for prepare / original extract).
2. Copy `ffmpeg` and `vgmstream-cli` into **this folder** (`tools/bin/`).
3. Verify: `music_asset doctor` or Music tab → **Check tools**.
4. After changing drop-ins, run **Check tools** again.

Search order: `tools/bin/` (this folder) → `SCR_MUSIC_BIN` → system `PATH`.  
Isolation tests may set `SCR_MUSIC_APP_BIN_ONLY=1` to ignore `PATH`.

Do **not** download arbitrary binaries into this folder without checking checksums / licence yourself. This project does not auto-fetch FFmpeg or vgmstream.
