# Music Library (file-based CPK replacement)

Replace any Sonic Colors soundtrack asset by converting a WAV into the game’s
CRI **AAX** format and patching a **copy** of `sonic2010_0.cpk`. Playback stays
on the original Wii CRI/AX path. There is **no** host MixInto overlay and **no**
AX muting.

## Catalog vs manifest

| File | Role |
|---|---|
| `mods/music/catalog.json` | Read-only inventory of original music AAX assets |
| `mods/music/manifest.toml` | User replacements (`[assets.*]`) |

```bash
./tools/music_asset catalog \
  --cpk game/SNCP8P/files/sonic2010_0.cpk \
  --out mods/music/catalog.json
```

SNCP8P Rev.00: **114** music AAX files in `sonic2010_0.cpk`. Voice/SFX excluded.
`_fx` variants are independent music entries.

## Launcher — Music Library

Wide windows: soundtrack table (left) + **player panel** (right).  
Narrow windows: player panel above the scrollable list.

### Player panel

- Track name, category, variant, stage/world, original filename, replacement.
- **ORIGINAL** Play / Pause / Resume / Stop (from unmodified CPK via vgmstream).
- **REPLACEMENT** Play / Pause / Resume / Stop (assigned WAV).
- Shared seek / loop / volume transport for the active preview.
- **Compare Original / Replacement** (A/B).
- **Export Original as WAV** (native save dialog).
- Assign / Enable / Disable / Restore replacement.

Only one preview plays at a time. Changing selection stops playback.  
Preview never patches a CPK or touches game AX audio.

### Export Original as WAV

Always extracts from the **original** `sonic2010_0.cpk` (never the music_mods cache).

| Mode | Behaviour |
|---|---|
| Complete | Intro + loop once (`vgmstream -i`) |
| Expanded loops | Intro + N loop plays (`-l N`) |
| Loop-aware WAV | Complete + `smpl` loop chunk (`-i -L`) |

PCM 16-bit or 24-bit at the original sample rate/channels. ADX is lossy; the WAV
is a decoded reconstruction, not a studio master.

```bash
./tools/music_asset export-original \
  --cpk game/SNCP8P/files/sonic2010_0.cpk \
  --path sound/Synth/bgm_stg110_rso_wav.aax \
  --out ~/Music/Tropical_Resort_Act_1_Original.wav \
  --mode complete --bits 16

./tools/music_asset export-all \
  --catalog mods/music/catalog.json \
  --out-dir ~/Music/SonicColorsOST \
  --mode complete
```

Bulk export writes only catalog music assets (no voice/SFX). Existing files are
not overwritten unless `--overwrite` is set.

## Manifest (`[assets.*]`)

```toml
[assets.stg110_main]
archive = "sonic2010_0.cpk"
path = "sound/Synth/bgm_stg110_rso_wav.aax"
source = "tracks/tropical_resort_act1.wav"
enabled = true
variant = "main"

[assets.stg110_fx]
archive = "sonic2010_0.cpk"
path = "sound/Synth/bgm_stg110_rso_fx_wav.aax"
source = "tracks/tropical_resort_act1_fx.wav"
enabled = true
variant = "fx"
```

Legacy `[tracks.*]` is still accepted by prepare (main AAX only).

## Pipeline

```text
WAV → 48 kHz PCM → CRI ADX → AAX (template segments, UTF data_alignment=8)
    → patched copy of sonic2010_0.cpk → Disc_RegisterHostOverride → CRI/AX
```

## Requirements

- `ffmpeg`, `vgmstream-cli` on `PATH`
- `.venv-music` with **cricodecs**

## Automated prepare check

```bash
./tools/music_pipeline_smoke.sh
```

Builds an isolated TR1 (`stg110`) diagnostic replacement and checks the patched
CPK + payload proof. This does **not** replace an in-game listen test.

## Replacement CPK cache (`music_mods/current.json`)

Created only after a successful **Prepare** with enabled replacements (or a
prepare that records `use_original`). It is **not** required to browse the
catalog, preview originals, or export WAVs — those read the untouched game CPK.

On Windows portable installs the cache lives next to the exe under
`music_mods/` (same as `%APPDATA%\SonicColorsRecomp\music_mods` for non-portable).
The launcher Music tab and in-game override share this path via `MusicPaths_*`.

## Restore original music

Disable mappings, turn off custom music assets, or delete
`music_mods/current.json`. Original CPK is never modified.
