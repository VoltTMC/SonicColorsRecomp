# Development roadmap — SonicColorsRecomp

Living backlog for ongoing PC development. Keep entries tied to real code/docs state.
**H7 (black world after stomp) is deferred — do not schedule GX/XFB/EFB work for it.**

Baseline: Phase 19 `72ae9de` (+ stability-recovery docs). Matching decomp: read-only.

---

## Current priorities

| Pri | Item | Why | Status |
|---|---|---|---|
| A | Linux/Windows feature parity (honest matrix) | Equal UX | **Ongoing** — matrix refreshed 2026-11-20 |
| B | H8 — Confirm in-game music replacement audible in TR1 | Prepare smoke PASS; playtest open | **PART** |
| B | Stick / trigger deadzones, graphics presets, FPS overlay, config backup, input profiles, focus mute | PC QoL | **Done** (2026-11-20) |
| B | Independent music / SFX volume (host) | Shared Wii mix — master + speaker only | **Done** (master) |
| D | Packaging smoke (`tools/package_smoke.sh`) | Regression net | **Done** |
| — | H7 black-screen + soft-lock | Known unresolved | **DEFERRED** |
| — | AP1 spiral / ForcePath | Known unresolved | **DEFERRED** |

---

## Top improvement backlog (audit 2026-11-20)

Ranked for the continuous-improvement goal. Spiral/H7 excluded.

| # | Area | Improvement | Status |
|---|---|---|---|
| 1 | Parity | Keep `WINDOWS_FEATURE_PARITY.md` honest; expand `[native]` checks | Ongoing |
| 2 | Stability | Config backup before Reset | **Done** |
| 3 | QoL | On-screen FPS overlay + `--show-fps` | **Done** |
| 4 | Stability | Focus-loss mute + rumble stop | **Done** |
| 5 | Launcher | Play-tab settings / pad summary | **Done** |
| 6 | Input | Named binding profiles | **Done** |
| 7 | Input | Stick + trigger deadzones | **Done** |
| 8 | Video | Graphics presets | **Done** |
| 9 | Audio | H8 in-game music audibility confirmation | PART |
| 10 | Parity | Native Win WASAPI / DPI / pad checklist | UNVERIFIED |
| 11 | Launcher | First-run setup wizard polish | Improved (portable status + versioned titles + RecompLauncher settings) |
| 12 | Performance | Frame-time / startup benchmarks doc | **Done** (`docs/PERFORMANCE.md`) |
| 13 | QoL | Controller prompt glyphs in overlay | **Done** (coloured pad chips + key glyphs) |
| 14 | Accessibility | Remappable F1/F12 / UI scale | **Done** (UI scale + F-key hotkeys) |
| 15 | Mods | Mod profiles (enable sets) | **Done** (2026-11-20) |
| 16 | Saves | Auto-backup on launch + after write + retention | **Done** (2026-11-20) |
| 17 | Packaging | Auto-updater / version check | Check + download + AppImage / Windows zip Install & relaunch **Done** |
| 18 | Launcher | Resize / HiDPI stress-test checklist | Size persist + PERFORMANCE.md smoke **Done** |
| 19 | Runtime | In-game performance overlay (CPU/GPU) | FPS+ms+1%+Present+GPU blit timestamps+draws/XFB+guest+RSS+audio **Done** |
| 20 | Distribution | Windows installer (per-user Inno) | Done (unsigned; signing open) |

---

## Completed (recent)

| When | Item |
|---|---|
| 2026-11-19 | Phase 19 quality audit, NAND C1, branding, packaging, music paths |
| 2026-11-19 | Stability recovery: revert Phase 20 black-screen experiments; restore `72ae9de` runtime |
| 2026-11-19 | `music_mod` sample-manifest test skips assets-only Music Library manifests |
| 2026-11-19 | **M7** — `Fs_WriteFileAtomic` + `scr_nand_fs_test` |
| 2026-11-19 | CLI `-h` / `--help` (no launcher/log side effects) |
| 2026-11-19 | `docs/DEVELOPMENT_ROADMAP.md` living backlog |
| 2026-11-19 | Exclusive fullscreen config (Linux SDL3 mode); borderless default |
| 2026-11-19 | First-launch / missing-data UX: Colors branding, supported release, skip-launcher checks |
| 2026-11-19 | `tools/package_smoke.sh` — AppDir layout + `--help` + shot-abort |
| 2026-11-19 | VRAM bounds: tex/EFB hard caps + hi-res LRU eviction; GX stats log; Win32 title M6 |
| 2026-11-19 | Controller hotplug: compact slots on disconnect, probe at startup, settings status |
| 2026-11-19 | GameBanana game id → Sonic Colors (6243); SCR_* env aliases; Win32 class rename |
| 2026-11-19 | Save backup/restore in System settings; `scr_nand_backup_test`; remaining SCR_* debug envs |
| 2026-11-19 | Host master + remote-speaker volume (`[Audio]` config + Audio settings) |
| 2026-11-19 | Linux audio output device picker (`audio_device` + live switch) |
| 2026-11-19 | System → Open log folder (`Log_Path`); `scr_config_test` Audio roundtrip |
| 2026-11-19 | `tools/music_pipeline_smoke.sh` — isolated TR1 WAV→AAX→CPK prepare + proof |
| 2026-11-19 | Optional integer scaling present; `tools/regression_smoke.sh`; desktop Keywords |
| 2026-11-19 | F12 user screenshots → `screenshots/`; System → Open screenshots folder |
| 2026-11-19 | Optional host FPS cap for uncapped mode (`host_fps_cap`) |
| 2026-11-19 | `--version` / `--portable` (SCR_PORTABLE); System shows user data dir |
| 2026-11-19 | AppStream metainfo in AppDir; System → Open user data folder |
| 2026-11-19 | Linux SDL audio device hotplug (reopen on remove/add) |
| 2026-11-19 | System → Reset settings to defaults (keeps Paths/Mods/Music) |
| 2026-11-19 | System → Save diagnostic report (version/paths/config/log tail) |
| 2026-11-19 | F12 user screenshots → full-res PNG (TAS/--shots still BMP) |
| 2026-11-19 | In-game toasts: F12 screenshot + controller hotplug (Linux) |
| 2026-11-19 | `--list-audio-devices`; Win32 XInput hotplug toast; audio reconnect toast |
| 2026-11-19 | Save restore: pick any backup (newest-first); diag path → clipboard |
| 2026-11-19 | README launch table; keyboard binding conflict warnings |
| 2026-11-19 | `tools/package_test_linux.sh` shareable Linux test archive |
| 2026-11-19 | `tools/package.ps1` Windows shareable pack (exe + licenses + TESTING) |
| 2026-11-19 | Overlay Restart/Quit confirmation; package_smoke checks pack scripts |
| 2026-11-19 | Fix ISO install “damaged”: drop wit helpers from SNCP8P file manifest |
| 2026-11-19 | install_manifest_check + verify.cpp skip wit helpers (regression) |
| 2026-11-19 | README launch table (Linux/Windows); keyboard binding conflict warnings |
| 2026-11-19 | MSVC: CXX23 single /std; ppcrecomp /Z7+/Ob1; tests EXCLUDE_FROM_ALL; -j2 preset |
| 2026-11-19 | `tools/package_windows_src.sh` refreshes Windows transfer zip |
| 2026-11-19 | music_mod: in-process SHA-256 (no popen) — unblocks Windows clang-cl |
| 2026-11-19 | overlay.h cstdint; perf.cpp PPC_GEN_FUNC decl (MSVC restrict mangling) |
| 2026-11-19 | Win32: MEM1 mirror probe at init; richer AV crash dump; /OPT:NOICF; log fflush |
| 2026-11-19 | Linux→Windows MinGW cross: `windows-mingw` preset, `out-windows/`, `docs/windows-cross.md` |

---

## Deferred / do-not-touch

- **H7** — Black world + soft-lock after stomp (TR1/TR3). No GX/XFB/EFB experiments, framebuffer resets, or forced redraws unless explicitly requested.
- **AP1 spiral / ForcePath / OOC tube (`stg510`)** — deferred indefinitely. No spiral gates, PathTick hacks, dt overrides, or related diagnostics unless explicitly requested.
- Matching decomp tree `/home/volt/Decomp/sonic2010-sncp8p` — read-only.

---

## Session log

### 2026-11-20 — Release v0.3.1 (hotfix)

- Ship AppImage + Windows zip with clickable launcher tabs after boot.
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — Release v0.3.0

- Ship redistributables covering post-v0.2.0 launcher/updater/QoL batches.
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — Reinstall confirm + Copy paths

- Accidental reinstall guard; clipboard paths on Play Quick folders.
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — Last played on Play tab

- Persist and show last session start time on Play.
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — Versioned window titles + RecompLauncher settings

- Titles include build version; RecompLauncher pause/hints/backup + diagnostics action.
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — --verify-data CLI

- `--verify-data` / `--verify-data-quick` for support and automation; AppStream keywords.
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — Clear shader cache

- Confirmed clear of user shadercache; Windows package.ps1 / README mention.
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — list-gamepads + shader cache folder

- Support CLI for pads; Open shader cache; diag pad names.
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — Maximized restore + release-notes unit test

- Persist/restore maximized; SoftenReleaseBody covered by regression_smoke.
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — Persist game window size + pause on minimize

- Last windowed size saved on quit; minimize pauses with focus-loss option.
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — Cross-platform compatibility audit

- Compatibility matrix doc; AppImage music_asset packaging; package_cross_check.

### 2026-11-20 — Windows Music CPK / cache path fix

- Shared MusicPaths for original CPK + music_mods cache; UI no longer treats “cache not built” as blocking originals.

### 2026-11-20 — Windows per-user installer

- Inno Setup primary distro; `installed.txt` + AppData user data; portable ZIP optional; music tools in user-data.

### 2026-11-20 — Release v0.3.3 (hotfix)

- AppImage + Windows zip: Music Library dependency doctor/setup, tools/bin discovery, music_asset MinGW DLL next to helper.

### 2026-11-20 — Release v0.3.2 (hotfix)

- AppImage + Windows zip: updater install fix + Music Library catalog packaging.

### 2026-11-20 — Windows Music Library catalog packaging

- Ship `mods/music/catalog.json` in Windows ZIP / AppDir; harden DefaultDir + prepare manifest save.

### 2026-11-20 — Windows auto-update install fix

- Replaced broken `apply_update.cmd` (`%%ZIP%%` never expanded) with `apply_update.ps1` (embedded paths, PID wait, robocopy, rollback, log).
- Package validation before install; AppImage backup/replace hardened; `scr_update_apply_test`.

### 2026-11-20 — Windows zip Install & relaunch

- Portable Windows installs schedule deferred apply (wait → Expand-Archive → copy → relaunch).
- Next: Win native checklist; Recomp Achievements; signed builds.

### 2026-11-20 — AppImage Install & relaunch

- Writable AppImage builds can replace `$APPIMAGE` from downloads and relaunch; Windows zip still manual.
- Next: Win native checklist; Windows zip apply helper / Recomp Achievements.

### 2026-11-20 — What's new + open downloaded package

- Version bump banner; AppImage/zip open after download; quit confirm during verify.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Crash-banner diagnostics + copy paths

- One-click diagnostic from crash banner; clipboard helpers for log/user-data; desktop Keywords refresh.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Install helpers test

- `scr_install_helpers_test` in regression_smoke; Windows `package.ps1` TESTING sync.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Nearby disc formats + last ISO

- `Install_LooksLikeDiscImage`; persist `iso=` after install; Play shows free space when data missing.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Release-notes softener fix + AppStream

- Line-leading markdown only; `--check-updates` prints notes; metainfo homepage/bugtracker.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Update notes + diagnostics prune

- Softened release `body` on Play/Settings when newer; prune diagnostic reports to 20.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Richer diagnostics + CLI

- Diagnostic reports add free space / `.1.log` / crash marker; `--save-diagnostics` for scripts.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Install free-space check

- Early reject with GB needed vs free; installer shows free space + reinstall headroom note.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Previous-crash banner

- Abnormal-exit marker + Play recovery banner; package_smoke covers `--prepare` / `--game` / `--download-update` help.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Wrong-region disc messaging

- `Release_UnsupportedMessage` names USA/Japan Colors discs; wired through install/verify/region/UI.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Updater polish

- `+x` on AppImage, prune downloads to 3, Play quick folder, `--download-update`.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — Log rotation + update package download

- Keeps last 5 run logs; downloads AppImage/zip to `downloads/` without replacing the running build.
- Next: Win native checklist; optional auto-install / Recomp Achievements.

### 2026-11-20 — CLI `--check-updates`

- Packaging/scripts can query the latest release tag without launching the UI.
- Next: Win native checklist; packaging auto-download.

### 2026-11-20 — Play-tab update availability hint

- Shared `update_check`; Play banner when newer GitHub release tag exists.
- Next: Win native checklist; packaging auto-download.

### 2026-11-20 — Screenshot retention

- `screenshot_keep` prunes oldest PNGs after F12 (default 100).
- Next: Win native checklist; packaging auto-download.

### 2026-11-20 — Config backups folder + save backup count

- System → Open config backups; save backup count vs keep limit.
- Next: Win native checklist; packaging auto-download.

### 2026-11-20 — GPU blit timing on FPS overlay

- D3D11 TIMESTAMP + DISJOINT around present blit; resolved one frame late.
- Next: Win native checklist; packaging auto-download.

### 2026-11-20 — Persist Music Library filters

- `filter_cat/var/repl/world` in config.ini; kept across Settings Reset.
- Next: Win native checklist; D3D timestamp queries.

### 2026-11-20 — Win32 audio remaster toast

- Match Linux toast on IMMNotificationClient remaster; parity matrix updated (native still UNVERIFIED).
- Next: Win native checklist; D3D timestamp queries.

### 2026-11-20 — Play portable status + config backup prune

- Portable / user-data line on Play; Open screenshots; prune config_backups to 20.
- Next: Win native checklist; D3D timestamp queries.

### 2026-11-20 — Input hint glyphs

- Coloured face/shoulder chips + key glyphs on Show input hints overlay.
- Next: Win native checklist; D3D timestamp queries; first-run polish.

### 2026-11-20 — FPS overlay Present ms + draws/XFB

- Present wall timing always sampled; overlay + PERFORMANCE.md updated.
- Next: Win native checklist; D3D timestamp queries; glyph art for prompts.

### 2026-11-20 — Save backup on write + retention

- `auto_backup_saves_on_write` + `save_backup_keep`; prune after launch/write backups.
- Next: Win native checklist items; GPU counters; glyph art for prompts.

### 2026-11-20 — Video profiles + Play recovery

- `video_profiles/` Save/Load; missing/corrupt Play-tab recovery links.
- Next: Win native checklist items; GPU counters; glyph art for prompts.

### 2026-11-20 — Perf overlay detail + input hints

- Guest Hz / RSS / audio underruns on FPS overlay; on-screen Jump/Slash/Pause hints.

### 2026-11-20 — Launcher size + FPS detail + update check

- Persist launcher WxH; FPS overlay ms/1%; System update check vs GitHub Releases.

### 2026-11-20 — Persist launcher tab + Music empty state + startup log

- `launcher_tab` remembered; Music catalog empty guidance; `startup: guest entry ready in N ms`.

### 2026-11-20 — Remappable host hotkeys

- Settings / screenshot / fullscreen remappable to F1–F12; input profiles include them.

### 2026-11-20 — UI scale

- System `ui_scale` for launcher/overlay (75–150%).

### 2026-11-20 — Mod profiles + PERFORMANCE.md

- `mod_profiles/` Save/Load in Mods tab; `docs/PERFORMANCE.md`.

### 2026-11-20 — Auto save backup on launch

- Optional `auto_backup_saves_on_launch`; preserved across Settings Reset.

### 2026-11-20 — `--show-fps` + backlog

- CLI `--show-fps`; top-20 improvement backlog in this file.

### 2026-11-20 — Input binding profiles

- Named profiles for binds/deadzones under `input_profiles/`; Input tab Save/Load.

### 2026-11-20 — Focus-loss mute + launcher status

- `AudioOut_SetSuspended` + rumble stop on pause-on-focus; launcher Current settings strip.

### 2026-11-20 — Config backup on reset + FPS overlay

- Reset settings copies `config.ini` → `config_backups/` first; System → Show FPS overlay.

### 2026-11-20 — Trigger deadzone + graphics presets

- `trigger_deadzone` config + Input slider; remaps LT/RT before digital threshold.
- Video **Graphics preset** (Performance / Balanced / Quality) for scale/AF/VSync/async/gpu thread.
- Launcher missing-data first-run bullets; README play-without-compile + Windows status honesty.

### 2026-11-20 — Continuous improvement initiative

- Removed experimental AP1 spiral gates (`spiral_60.cpp`, spiral game_addrs/overrides); docs mark **DEFERRED**.
- Configurable left/right stick deadzone (`stick_deadzone` / `stick_deadzone_right`) + Input settings sliders.
- Corrected `audio_device` comment (Windows WASAPI supported).
- Launcher home shows build version.
- Refreshed `docs/WINDOWS_FEATURE_PARITY.md` feature matrix from code audit.

### 2026-11-19 — Goal armed; M7 + `--help` + exclusive FS + first-launch UX

- M7 `Fs_WriteFileAtomic` + `scr_nand_fs_test`; CLI `--help`; exclusive fullscreen (Linux).
- First-launch UX: removed Black Knight install copy; launcher guidance; `--skip-launcher` validates data; `scr_install` in fatals.
- Added `tools/package_smoke.sh` (PASS).
- VRAM: hard-cap tex (4096) / EFB copies (256); hi-res pack LRU (age 1200 / cap 512); richer GX stats; Win32 title fixed (M6).
- Controller hotplug: compact slots on unplug; `Input_ProbeConnectedPads`; Input settings shows live pad names.
- GameBanana browse was still on Black Knight (6511) → Colors **6243**; Win32 classes `SonicColorsRecomp*`; UI test envs prefer `SCR_*` (BKR_* fallback).
- System → Save data: open folder / backup / restore latest; `nand_backup.h` + `scr_nand_backup_test`.
- Remaining debug envs also accept `SCR_*` (`SLOW_SAMPLE`, `IRQ_STALLS`, `TEST_PAUSE`, `DUMPMEM2`).
- Host audio: `[Audio] master_volume` / `speaker_volume`; live sliders in Audio settings (music+SFX stay one Wii mix).
- Linux: output device combo (`audio_device`); mutex around SDL reopen / PushDSP.
- Diagnostics: System → Open log folder; `scr_config_test` covers Audio config roundtrip.
- H8 PART: `music_pipeline_smoke.sh` prepares TR1 (stg110) diag tone + payload proof (PASS). In-game audibility still needs playtest.
- Video: optional integer scaling (640×360/480 multiples); `regression_smoke.sh` aggregates unit/package/music smokes; desktop Keywords + PrefersNonDefaultGPU.
- F12 saves XFB BMP under `screenshots/`; System settings opens that folder.
- Uncapped mode: optional `host_fps_cap` (60–240) paces guest FrameLoopWait; 30/60 unchanged.
- CLI `--version` / `--portable` (+ `SCR_PORTABLE`); System tab shows user data directory.
- AppStream `soniccolorsrecomp.metainfo.xml` staged into AppDir; System → Open user data folder.
- Linux: reopen audio output on SDL device remove/add (`AudioOut_HandleEvent`).
- System → Reset settings to defaults (preserves Paths / Mods / Music folder).
- System → Save diagnostic report → `diagnostics/scr_diag_*.txt` (config + log tail).
- F12 screenshots → full-resolution PNG under `screenshots/` (debug/TAS BMP unchanged).
- `Overlay_Toast`: F12 confirm + Linux controller connect/disconnect (startup probe silent).
- `--list-audio-devices` CLI; package_smoke checks it; Win32 `Input_PollHotplug` toasts; Linux audio reconnect toast.
- Save restore combo (any `save_backups/*`); diagnostic report copies path to clipboard.
- README launch table (Linux/Windows); Input settings warn on shared keyboard bindings.
- `tools/package_test_linux.sh` → `dist/SonicColorsRecomp-<ver>-linux-x86_64.tar.zst` (+ TESTING.txt).
- `tools/package.ps1` → `dist/SonicColorsRecomp/` on Windows (exe + licenses; optional shader precompile).
- Overlay Restart/Quit ask for confirmation; `package_smoke` asserts pack scripts exist.
- ISO install false “damaged”: `files_sncp8p.inc` no longer requires wit `align-files.txt` / `setup.*` / `include.fst`.
- `install_manifest_check` in regression_smoke; `Install_QuickCheck` skips wit helpers if reintroduced.
- Windows transfer zip refreshed under `dist/SonicColorsRecomp-windows-build-src.zip`.
- Next: H8 in-game TR1 listen; Win32 WASAPI/XAudio device enum if needed.
- Note: git commits blocked (no `user.email` in this environment).
