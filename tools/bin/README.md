# Optional Music Library host binaries

Drop platform-native tools here so Sonic Colors Recomp can find them **without**
modifying `PATH`.

| Binary | Used for | Redistribution note |
|--------|----------|---------------------|
| `ffmpeg` / `ffmpeg.exe` | Resample WAV → 48 kHz; some exports | Prefer an **LGPL** FFmpeg build. Do **not** drop GPL-only builds into a redistributable you plan to ship. |
| `vgmstream-cli` / `vgmstream_cli.exe` | Original AAX → WAV preview/export | vgmstream is typically **ISC**; redistributable when you comply with its licence. |

Search order: `tools/bin/` (this folder) → `SCR_MUSIC_BIN` → system `PATH`.

Python + **cricodecs** are separate: run `tools/music_deps_setup.sh` or
`tools\music_deps_setup.cmd` to create `.venv-music` and install cricodecs.

Check status:

```bash
./tools/music_asset doctor
# or Windows:
tools\music_asset.exe doctor
```
