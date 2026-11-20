# Optional Music Library host binaries

Drop platform-native tools here **or** (preferred for installed Windows apps) in the
user-writable folder so they survive application updates:

| Layout | Drop-in folder |
|--------|----------------|
| **Installed (Windows)** | `%APPDATA%\SonicColorsRecomp\tools\bin` |
| **Portable ZIP** | `tools\bin\` next to the exe (this folder when shipped) |
| **Linux AppImage / XDG** | `$XDG_DATA_HOME/SonicColorsRecomp/tools/bin` (usually `~/.local/share/...`) |

Override with `SCR_MUSIC_BIN`. Search order: `SCR_MUSIC_BIN` → user-data `tools/bin` → **managed** `%LOCALAPPDATA%\SonicColorsRecomp\dependencies\current\bin` → packaged `tools/bin` (this folder) → `PATH`.

On Windows the launcher can **Install Music tools** (pinned Python embed, cricodecs, vgmstream, LGPL FFmpeg) into `%LOCALAPPDATA%\SonicColorsRecomp\dependencies`. User drop-ins in this folder / AppData `tools\bin` still override managed copies. No admin and no PATH edits.

| Binary | Used for | Redistribution note |
|--------|----------|---------------------|
| `ffmpeg` / `ffmpeg.exe` | Prepare (WAV → 48 kHz); some exports | Prefer an **LGPL** FFmpeg build. Do **not** ship GPL-only builds inside a redistributable you publish. |
| `vgmstream-cli` / `vgmstream_cli.exe` | Original AAX → WAV preview/export | vgmstream is typically **ISC**; redistribute only when you comply with its licence. |

## What works without these

- Browse the Music catalog (`mods/music/catalog.json`)
- Preview assigned replacement WAVs
- Play with an already-prepared `music_mods` cache

## Setup order

1. Run `tools/music_deps_setup.sh` / `tools\music_deps_setup.cmd`  
   → creates `.venv-music` (+ under user data when `installed.txt` is present).
2. Copy `ffmpeg` and `vgmstream-cli` into the drop-in folder above.
3. Verify: `music_asset doctor` or Music tab → **Check tools**.

Isolation tests may set `SCR_MUSIC_APP_BIN_ONLY=1` to ignore `PATH`.

Do **not** download arbitrary binaries without checking checksums / licence yourself.
This project does not auto-fetch FFmpeg or vgmstream.
