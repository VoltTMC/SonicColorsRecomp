# Cross-Platform Compatibility — Sonic Colors Recomp

Living audit of **Linux** vs **Windows x86-64** user-facing behaviour.

Statuses: **PASS** · **FAIL** · **PARTIAL** · **UNVERIFIED** · **N/A**

Evidence: `[auto]` automated · `[source]` code review · `[wine]` Wine · `[native]` real OS host

Companion docs: `WINDOWS_FEATURE_PARITY.md`, `windows-native-checklist.md`, `MUSIC_MODDING.md`.

Last updated: 2026-11-20 (doctor exec validation + clean-env music_deps_test).

---

## Music Library dependency map

| Operation | Python | cricodecs | vgmstream | FFmpeg | Packaged-only? |
|-----------|--------|-----------|-----------|--------|----------------|
| Browse catalog | — | — | — | — | Yes (`catalog.json`) |
| Replacement WAV preview | — | — | — | — | Yes (in-process) |
| A/B (replacement side) | — | — | — | — | Yes |
| In-game replacement (after Prepare) | — | — | — | — | Yes (`music_mods` cache) |
| Original preview | required | extract AAX | decode WAV | — | No |
| Export original WAV | required | extract | decode | optional normalize | No |
| Prepare (WAV→ADX/AAX→CPK) | required | ADX/AAX/CPK | — | resample 48 kHz | No |
| Rebuild catalog from CPK | required | required | — | — | No (not needed if catalog ships) |

Discovery: `SCR_MUSIC_BIN` → user `tools/bin` → managed `%LOCALAPPDATA%\…\dependencies\current\bin` → packaged `tools/bin` → `PATH`. Doctor **runs** each binary (`ok` / `missing` / `unusable`) and lists `searched` dirs. Windows: Music tab **Install Music tools** downloads pinned Python/cricodecs/vgmstream/LGPL FFmpeg (`data/music_deps_manifest.json`) into LOCALAPPDATA (not shipped in Setup/ZIP). `SetEnvironmentVariableW` for `SCR_MUSIC_BIN` / `SCR_MUSIC_MANAGED_BIN` / `SCR_MUSIC_PYTHON`. Isolation: `SCR_MUSIC_APP_BIN_ONLY=1`.

**Bundling policy:** no GPL FFmpeg / Python / cricodecs in ZIP/AppImage.

| Check | Linux | Windows | Evidence |
|-------|-------|---------|----------|
| Clean env: browse ready, prepare blocked | PASS `[auto]` | PASS `[source]` | `music_deps_test.sh` |
| Invalid drop-in → unusable | PASS `[auto]` | PASS `[source]` | `music_deps_test.sh` |
| Paths with spaces / Unicode | PASS `[auto]` | UNVERIFIED `[native]` | `music_deps_test.sh` |
| Prepare + payload proof | PASS `[auto]` | UNVERIFIED `[native]` | `SCR_MUSIC_PIPELINE=1`; Wine ≠ native |
| Wine `music_asset.exe` loads | N/A | PASS `[wine]` | `package_cross_check.sh` |

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
| AppImage / Windows ZIP packaging | PASS `[auto]` | PASS `[auto]` | PASS | Portable ZIP optional; installer is primary Win distro |
| Windows installer (Inno, per-user) | N/A | PASS stage `[auto]` / Setup `[wine]` when ISCC | PARTIAL | `package_windows_installer.sh`; native install UI UNVERIFIED |
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
| Music prepare / preview / export | PARTIAL→PASS w/ deps `[auto]` | PARTIAL | PARTIAL | Linux pipeline smoke PASS; Windows prepare UNVERIFIED `[native]` |
| Music dependency doctor | PASS `[auto]` | PASS `[wine]` load | PASS `[auto]` | Exec probe + clean-env + invalid drop-in |
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
| Original preview | PARTIAL | PARTIAL → managed deps `[source]` | PARTIAL | Auto Install Music tools on Windows; native E2E UNVERIFIED |
| Replacement WAV preview | PASS `[source]` | PASS `[source]` | UNVERIFIED | No CPK cache needed |
| A/B compare | PASS `[source]` | PASS `[source]` | UNVERIFIED | |
| Export original WAV | PARTIAL | PARTIAL → launch fixed `[wine]` | PARTIAL | Same as preview |
| Prepare ADX/AAX/CPK patch | PARTIAL | PARTIAL → launch fixed `[wine]` | PARTIAL | Was CreateProcess 87 / rc=127; see root causes |
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
| `music_asset.exe` Unicode env block without `CREATE_UNICODE_ENVIRONMENT` | `CreateProcess failed (87)` then `PREPARE FAILED (rc=127)` when starting Python | Pass `CREATE_UNICODE_ENVIRONMENT`; discover AppData `.venv-music`; distinguish exit 126 vs 127 |
| GUI `Platform_RunProcess` + `STARTF_USESTDHANDLES` + `GetStdHandle` | `ERROR_INVALID_PARAMETER` (87) when redirecting stdout/stderr from a GUI parent | Open `NUL` for stdin/unused std handles; shared `runtime/platform/run_process.cpp` + `scr_run_process_test` |
| Windows `_putenv_s(SCR_MUSIC_BIN)` only | Music preview child never saw drop-in dir → `vgmstream-cli not found on PATH` | `SetEnvironmentVariableW` + prefer `vgmstream-cli.exe`; doctor/preview report `searched` paths |

---

## Verification procedures

### Automated (this repo)

```bash
cmake --build --preset linux-clang --target SonicColorsRecomp scr_music_paths_test scr_music_manifest_test scr_update_apply_test scr_run_process_test
./out/scr_music_paths_test
./out/scr_music_manifest_test
./out/scr_update_apply_test
./out/scr_run_process_test
cmake --build --preset windows-mingw --target SonicColorsRecomp scr_run_process_test music_asset
wine out-windows/scr_run_process_test.exe   # Wine ≠ native Windows PASS
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
| 2026-11-20 | Windows Music auto deps manager: pinned manifest → LOCALAPPDATA downloads (Python/cricodecs/vgmstream/LGPL FFmpeg); Music tab Install UI; native E2E UNVERIFIED |
| 2026-11-20 | Fix Windows vgmstream discovery: Win32 `SCR_MUSIC_BIN` env, `.exe` names, searched-path errors; CreateProcess 87 fix remains in v0.3.5 |
| 2026-11-20 | Fix Windows Music `CreateProcess` 87: Unicode env flag in `music_asset` + NUL std handles in `Platform_RunProcess`; `scr_run_process_test` (Linux + Wine) |
| 2026-11-20 | Windows per-user Inno installer (`installed.txt` → AppData); portable ZIP optional; user-data music tools |
| 2026-11-20 | Doctor exec/version probe, clean-env tests, setup.steps, optional ffmpeg fix, AppDir isolated doctor |
| 2026-11-20 | Ship `libwinpthread-1.dll` next to `tools/music_asset.exe`; Wine doctor smoke in `package_cross_check` |
| 2026-11-20 | Music deps doctor, `tools/bin` discovery, music_deps_setup, Music UI Check tools |
| 2026-11-20 | Initial `CROSS_PLATFORM_COMPATIBILITY.md`; AppImage ships `music_asset`; `package_cross_check.sh`; MusicTool prefers exe dir |
| 2026-11-20 | MusicPaths + catalog packaging + updater PS1 + tab click (see PROGRESS.md) |
