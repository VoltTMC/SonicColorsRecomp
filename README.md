# SonicColorsRecomp

Native Linux static recompilation of **Sonic Colors (Wii)** — PAL `SNCP8P` revision 00.

You must own the game. No disc image or assets are shipped in this repository.

## Status (2026-11-19, Phase 19)

| Area | State |
|---|---|
| Recompilation | **Done** — Broadway/Gekko → native x86-64 |
| Runtime | OS / IOS / GX / AX / WPAD / PAD HLE |
| Boot → title → Tropical Resort Act 1 | **Verified** |
| Music Library (launcher) | Catalog, preview, WAV export, file-based replacement prep |
| Release polish | In progress — see [`docs/QUALITY_AUDIT.md`](docs/QUALITY_AUDIT.md) |
| Full-game matrix | **Not** claimed — many stages / abilities `NT` |

Architecture: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) · Progress: [`docs/PROGRESS.md`](docs/PROGRESS.md) · Compatibility: [`docs/GAME_COMPATIBILITY.md`](docs/GAME_COMPATIBILITY.md) · Music: [`docs/MUSIC_MODDING.md`](docs/MUSIC_MODDING.md)

## Foundation

Adapted from [BlackKnightRecomp](https://github.com/yunophilia/BlackKnightRecomp) (GPL-2.0-only): PPC→C++ tools and Wii runtime.  
[DolRecomp](https://github.com/ExpansionPak/DolRecomp) is pinned under `external/` for instruction-coverage cross-checks only.

## Prerequisites

- Clang/LLVM, CMake ≥ 3.21, Ninja, Python 3.10+, `wit` (Wiimms ISO Tools)
- Runtime link deps: see Black Knight’s Linux list in `external/BlackKnightRecomp/docs/linux.md` (Vulkan, Wayland/X11, audio, bison/flex for vkd3d)
- Optional for music tooling: `ffmpeg`, `vgmstream-cli` / cricodecs as documented in `docs/MUSIC_MODDING.md`

**CMake note:** Cursor’s AppImage can break stock `cmake` module discovery. Use:

```bash
./scripts/cmake.sh ...
```

## Quick start

```bash
# 1. Extract your disc (if game/SNCP8P is missing)
wit EXTRACT "/path/to/Sonic2010 [SNCP8P].iso" --DEST game/SNCP8P/ --overwrite

# 2. Configure + build (first run fetches SDL3 / dxvk / vkd3d / nod)
./scripts/cmake.sh --preset linux-clang
./scripts/cmake.sh --build --preset linux-clang

# 3. Launch (launcher → Install / locate data → Play)
./out/SonicColorsRecomp

# Or skip the launcher once data is configured:
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher
```

User data (config, NAND saves, music cache) lives next to the executable for writable trees, or under `$XDG_DATA_HOME/SonicColorsRecomp` for AppImage / read-only installs.

## Music replacement (file-based)

Do **not** use host MixInto / AX mute overlays. Workflow:

1. Open **Music Library** in the launcher.
2. Assign WAV replacements; prepare patched CPKs via `tools/music_asset`.
3. Runtime registers overrides with `Disc_RegisterHostOverride`.

See [`docs/MUSIC_MODDING.md`](docs/MUSIC_MODDING.md).

## Packaging (Linux AppImage)

```bash
./scripts/cmake.sh --build --preset linux-clang
./tools/package_linux.sh -S          # stage dist/AppDir (no game data)
# optional: ./tools/package_linux.sh -A dist/AppDir -c /path/to/shadercache
```

Redistributable builds must **not** include `game/` or copyrighted assets.

## Layout

```
tools/          PPC analysis + recomp + music_asset
runtime/        Wii OS / GX / AX / IOS / Linux platform + launcher UI
generated/      Recompiled SNCP8P C++ (gitignored)
game/           Extracted disc data (gitignored)
mods/music/     catalog.json (tracked); your tracks/manifest (local)
external/       Pinned upstream checkouts
docs/           Architecture, progress, compatibility, quality audit
out/            Built SonicColorsRecomp + shared libs
```

## Licence

Project runtime/tools derived from BlackKnightRecomp: **GPL-2.0-only** — see `licenses/`.  
Preserve upstream notices when copying further. Game code/data remain your property and must not be redistributed.
