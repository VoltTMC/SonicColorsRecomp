# Cross-Platform Compatibility — Sonic Colors Recomp

Living audit of **Linux** vs **Windows x86-64** user-facing behaviour.

Statuses: **PASS** · **FAIL** · **PARTIAL** · **UNVERIFIED** · **N/A**

Evidence: `[auto]` automated · `[source]` code review · `[wine]` Wine · `[native]` real OS host

Companion docs: `WINDOWS_FEATURE_PARITY.md`, `windows-native-checklist.md`, `MUSIC_MODDING.md`.

Last updated: 2026-11-20 (Music Library doctor / tools/bin / music_deps_setup).

---

## Music Library dependency map

| Operation | Python | cricodecs | vgmstream | FFmpeg | Packaged-only? |
|-----------|--------|-----------|-----------|--------|----------------|
| Browse catalog | — | — | — | — | Yes (`catalog.json`) |
| Replacement WAV preview | — | — | — | — | Yes (in-process) |
| A/B (replacement side) | — | — | — | — | Yes |
| Original preview | required | extract AAX | decode WAV | — | No |
| Export original WAV | required | extract | decode | optional normalize | No |
| Prepare (WAV→ADX/AAX→CPK) | required | ADX/AAX/CPK | — | resample 48 kHz | No |
| Rebuild catalog from CPK | required | required | — | — | No (not needed if catalog ships) |
| Cache / `current.json` | via prepare | via prepare | — | via prepare | After Prepare only |

Discovery order: `tools/bin` (and `SCR_MUSIC_BIN`) → system `PATH`. Setup: `tools/music_deps_setup.sh` / `.cmd`. Report: `music_asset doctor` / Music UI **Check tools**.

**Bundling policy:** packages ship `music_asset*` + `tools/bin/README.md` + setup helpers only. Do **not** redistribute GPL FFmpeg, system Python, or cricodecs inside the ZIP/AppImage; place ISC vgmstream-cli and an LGPL FFmpeg build in `tools/bin/` when desired.

Evidence: `[auto]` `music_deps_test.sh` (doctor JSON + `tools/bin` preference); `[auto]` `SCR_MUSIC_PIPELINE=1` → `music_pipeline_smoke.sh` (prepare + payload proof on Linux host with deps).

---

## How to read this

- **PASS** requires exercised behaviour (automated, Wine, or native), not mere compilation.
- **UNVERIFIED** means the code path exists on both sides but this environment has not proven it on a real Windows host (or Linux AppImage end-to-end).
- Development runs from the git tree are **not** substitutes for clean extracted packages.

---

## Summary matrix

| Area | Linux | Windows | Status | Notes / action |
|------|-------|---------|--------|----------------|
| Build (clang / MinGW) | PASS `[auto]` | PASS `[auto]` | PASS | Presets `linux-clang`, `windows-mingw` |
| AppImage / Windows ZIP packaging | PASS `[auto]` | PASS `[auto]` | PASS | `package_cross_check.sh` |
| Launcher boot + tabs | PASS `[auto]` | PASS `[source]` | UNVERIFIED native | Tab click fixed v0.3.1 (`bootFrames`) |
| Config / portable / user-data | PASS `[source]` | PASS `[source]` | PASS `[source]` | Shared `Platform_UserDir` |
| Game launch / disc install | PASS | UNVERIFIED | UNVERIFIED | Need `[native]` |
| Saves / NAND | PASS `[auto]` | PASS `[auto]` | PASS | `scr_nand_*_test` |
| Graphics / window / borderless | PASS | PASS `[source]` | UNVERIFIED | Exclusive FS = Linux only |
| Audio output | PASS | UNVERIFIED | UNVERIFIED | SDL vs XAudio2 |
| Controllers / rumble | PASS | UNVERIFIED | UNVERIFIED | SDL vs XInput |
| Mods / GameBanana | PASS `[source]` | PASS `[source]` | UNVERIFIED | Shared libarchive |
| Music catalog browse | PASS `[auto]` | PASS `[auto]` | PASS package | `catalog.json` in both packages |
| Music original CPK discovery | PASS `[auto]` | PASS `[auto]` | PASS `[auto]` | `MusicPaths_*` / `scr_music_paths_test` |
| Music replacement cache status | PASS `[source]` | PASS `[source]` | PASS `[source]` | Clarified “not prepared yet” |
| Music prepare / preview / export | PARTIAL | PARTIAL | PARTIAL | Host deps; `music_asset doctor` + `tools/bin` drop-ins + `music_deps_setup` |
| Music dependency doctor | PASS `[auto]` | PASS `[source]` | PASS `[auto]` | `tools/music_deps_test.sh` |
| Auto-update check + download | PASS `[source]` | PASS `[source]` | PASS `[source]` | Shared `update_check` |
| Auto-update install | PASS AppImage `[source]` | PASS helper `[source]` | UNVERIFIED native | Win: `apply_update.ps1`; Linux: AppImage replace |
| Crash / diagnostics | PASS | PASS `[source]` | UNVERIFIED | Win dump richer |

---

## Feature inventory

### Launcher

| Feature | Linux | Windows | Status | Required action |
|---------|-------|---------|--------|-----------------|
| Startup / shutdown | PASS | PASS `[wine]` | UNVERIFIED native | Checklist |
| Tab navigation (mouse) | PASS | PASS `[source]` | PASS `[source]` | Fixed v0.3.1 |
| Window resize / DPI | PASS | UNVERIFIED | UNVERIFIED | Multi-monitor `[native]` |
| Settings persistence | PASS | PASS `[source]` | PASS `[source]` | |
| Play / install game data | PASS | UNVERIFIED | UNVERIFIED | |
| Portable vs user-data dirs | PASS | PASS `[source]` | PASS `[source]` | |
| Update check / download | PASS | PASS `[source]` | PASS `[source]` | |
| Update install & relaunch | PASS AppImage | PASS script `[source]` | UNVERIFIED native | Disposable install test |
| Logging / diagnostics | PASS | PASS `[source]` | UNVERIFIED | |
| First-run / Easy Setup | PASS | PASS `[source]` | UNVERIFIED | RecompLauncher |

### Music Library

| Feature | Linux | Windows | Status | Required action |
|---------|-------|---------|--------|-----------------|
| Ship `catalog.json` | PASS | PASS | PASS `[auto]` | Both packages |
| Ship `music_asset` helper | PASS AppDir | PASS ZIP | PASS `[auto]` | Win: `tools/libwinpthread-1.dll` beside helper; host Python still required |
| Locate `manifest.toml` | PASS | PASS `[source]` | PASS `[source]` | Created on Assign Replacement |
| Browse full catalog (~114) | PASS | PASS package | UNVERIFIED UI | Clean ZIP unzip test |
| Locate original CPK | PASS | PASS `[auto]` | PASS `[auto]` | Portable `exe/game/...` |
| Replacement CPK cache (`current.json`) | PASS | PASS `[source]` | PASS `[source]` | Only after Prepare |
| Original preview | PARTIAL | PARTIAL | PARTIAL | vgmstream + Python |
| Replacement WAV preview | PASS `[source]` | PASS `[source]` | UNVERIFIED | No CPK cache needed |
| A/B compare | PASS `[source]` | PASS `[source]` | UNVERIFIED | |
| Export original WAV | PARTIAL | PARTIAL | PARTIAL | Same tool chain |
| Prepare ADX/AAX/CPK patch | PARTIAL | PARTIAL | PARTIAL | Same tool chain |
| In-game replacement | PASS `[source]` | PASS `[source]` | UNVERIFIED | Shared `music_mod` |
| Original CPK untouched | PASS | PASS | PASS `[source]` | |

### Game runtime

| Feature | Linux | Windows | Status | Required action |
|---------|-------|---------|--------|-----------------|
| Boot → title / stage | PASS | UNVERIFIED | UNVERIFIED | `[native]` / Wine play |
| Graphics init (D3D11/DXVK) | PASS | PASS `[wine]` | UNVERIFIED | |
| Audio init | PASS | UNVERIFIED | UNVERIFIED | |
| Input + hot-plug | PASS | UNVERIFIED | UNVERIFIED | |
| Saves / NAND | PASS | PASS `[auto]` | PASS | |
| Frame rate modes | PASS | PASS `[source]` | UNVERIFIED feel | |
| Fullscreen | PASS exclusive+borderless | Borderless only | N/A / PLATFORM | Documented |
| Crash recovery banner | PASS | PASS `[source]` | UNVERIFIED | |

### Mods & packaging

| Feature | Linux | Windows | Status | Required action |
|---------|-------|---------|--------|-----------------|
| Mod scan / enable | PASS | PASS `[source]` | UNVERIFIED | |
| GameBanana install | PASS | PASS `[source]` | UNVERIFIED | |
| AppImage | PASS | N/A | PASS / N/A | Includes catalog + music_asset |
| Windows portable ZIP | N/A | PASS | PASS `[auto]` | Includes catalog + music_asset |
| Clean extract without repo | PASS smoke | PASS music check | PASS `[auto]` | GUI still UNVERIFIED |

---

## Shared root causes (and fixes)

| Root cause | Impact | Fix |
|------------|--------|-----|
| Cwd-first resource discovery | Empty Music catalog / wrong cache when launched from another folder | Prefer `Platform_ExeDir` / `Platform_UserDir` (`MusicPaths_*`, `MusicTool_Find`, `MusicManifest_DefaultDir`) |
| Missing packaged `catalog.json` | Windows Music tab empty | Ship in ZIP + AppDir |
| Missing packaged `music_asset` on Linux | AppImage Music prepare/preview couldn’t find tool | Ship `usr/bin/tools/music_asset` + `music_asset_pkg` |
| UI “CPK cache: not built” | Looked like a hard failure | Clarify: cache is from Prepare; originals use game CPK |
| Windows `apply_update.cmd` `%%ZIP%%` | Download OK, install fail | `apply_update.ps1` with embedded paths |
| Launcher tab `forceTab` stuck | Tabs not clickable | Dedicated `bootFrames` |
| Host music toolchain not bundled | Prepare/preview/export PARTIAL both OS | Doctor + setup scripts + optional `tools/bin` drop-ins; no GPL FFmpeg in packages |
| `music_asset.exe` MinGW DLL next to main exe only | Helper failed to load under Wine/clean ZIP (`libwinpthread-1.dll`) | Also ship winpthread under `tools/` |

---

## Verification procedures

### Automated (this repo)

```bash
cmake --build --preset linux-clang --target SonicColorsRecomp scr_music_paths_test scr_music_manifest_test scr_update_apply_test
./out/scr_music_paths_test
./out/scr_music_manifest_test
./out/scr_update_apply_test
cmake --build --preset windows-mingw --target SonicColorsRecomp
./tools/package_linux.sh -S
./tools/package_windows_cross.sh -v test
./tools/package_cross_check.sh
```

### Clean package (manual / CI)

1. Extract AppImage or Windows ZIP into a temp directory **outside** the repo.
2. Launch with a different cwd (`cd /tmp && /path/to/AppRun` or `SonicColorsRecomp.exe`).
3. Confirm Music tab lists ~114 tracks without regenerating the catalog.
4. With game data installed, confirm status is “not prepared yet” (not a red failure) until Prepare.
5. With Python/cricodecs/vgmstream/ffmpeg available, Assign + Prepare → `music_mods/current.json`.

### Native Windows

Follow `docs/windows-native-checklist.md`. Items marked UNVERIFIED above stay UNVERIFIED until checked there.

---

## Remaining limitations

1. No native Windows host in the default agent environment — do not claim `[native]` PASS.
2. Music prepare/preview/export require host tools on **both** platforms.
3. Exclusive fullscreen is Linux-only.
4. In-game play, audible audio, pads, DPI — UNVERIFIED on Windows.

---

## Change log (compatibility)

| Date | Change |
|------|--------|
| 2026-11-20 | Ship `libwinpthread-1.dll` next to `tools/music_asset.exe`; Wine doctor smoke in `package_cross_check` |
| 2026-11-20 | Music deps doctor, `tools/bin` discovery, music_deps_setup, Music UI Check tools |
| 2026-11-20 | Initial `CROSS_PLATFORM_COMPATIBILITY.md`; AppImage ships `music_asset`; `package_cross_check.sh`; MusicTool prefers exe dir |
| 2026-11-20 | MusicPaths + catalog packaging + updater PS1 + tab click (see PROGRESS.md) |
