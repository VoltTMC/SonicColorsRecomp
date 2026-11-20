# Windows Feature Parity — Sonic Colors Recomp

Living audit of Linux (baseline) vs Windows x86-64. **Compile success is not parity.**

Statuses: **FULL** · **PARTIAL** · **MISSING** · **UNVERIFIED** · **PLATFORM LIMITATION**

Evidence tags: `[native]` `[wine]` `[auto]` `[source]`

Last updated: 2026-11-20 (continuous improvement audit; stick deadzone; spiral gates removed).

---

## Summary

| Area | Linux | Windows | Status | Required action |
|------|-------|---------|--------|-----------------|
| Core boot / shutdown | FULL | UNVERIFIED* | UNVERIFIED | `[native]` play smoke |
| Saves / NAND / config paths | FULL | FULL | FULL | — |
| Graphics / window / borderless FS | FULL | FULL | FULL | Exclusive FS: Linux only |
| Exclusive fullscreen | FULL | — | PLATFORM LIMITATION | Document; borderless is the Win equivalent |
| VSync / frame rate / host FPS cap | FULL | FULL `[source]` | UNVERIFIED | `[native]` pacing |
| Audio playback | FULL | UNVERIFIED audible | UNVERIFIED | `[native]` listen |
| Audio device enum / select | FULL | FULL `[source]` | UNVERIFIED | `[native]` switch |
| Audio hot-plug remaster | FULL | PARTIAL | PARTIAL | Win: IMMNotificationClient; disconnect `[native]` open |
| Input + remapping | FULL | FULL `[source]` | UNVERIFIED | `[native]` pad |
| Stick deadzone settings | FULL | FULL `[source]` | FULL `[source]` | Added 2026-11-20 |
| Launcher + Music Library UI | FULL | FULL `[source]` / Wine boot | UNVERIFIED | Interactive `[native]` |
| music_asset prepare pipeline | FULL | PARTIAL | PARTIAL | Host Python/cricodecs/vgmstream/ffmpeg |
| Packaging | FULL AppDir | FULL ZIP | FULL `[auto]` | — |
| Crash / diagnostics | FULL | FULL `[source]` | UNVERIFIED | `[native]` crash dump |

\* Wine boot exercised; full stage play and audible audio need a real Windows host (`docs/windows-native-checklist.md`).

---

## Feature matrix

| Feature | Linux | Windows | Status | Required action |
|---------|-------|---------|--------|-----------------|
| Startup / shutdown | yes | yes | FULL `[wine]` | — |
| Boot → stage load | yes | Wine GX frames | UNVERIFIED | `[native]` TR1 |
| Wii HLE (shared) | yes | yes | FULL `[source]` | — |
| Saves / NAND atomic write | yes | yes | FULL `[auto]` | — |
| Save backup / restore UI | yes | yes | FULL `[source]` | — |
| Config.ini persistence | yes | yes | FULL `[source]` | — |
| Portable mode (`portable.txt` / `--portable`) | yes | yes | FULL `[source]` | — |
| Frame rate 30 / 60 / uncapped | yes | yes | FULL `[source]` | `[native]` confirm feel |
| Host FPS cap (uncapped) | yes | yes | FULL `[source]` | — |
| Spring CD gate at 60 | yes | yes | FULL `[source]` | — |
| Windowed / resize | yes | yes | FULL `[wine]` | — |
| Borderless fullscreen | yes | yes | FULL `[source]` | — |
| Exclusive fullscreen | yes (SDL mode) | no | PLATFORM LIMITATION | Keep borderless on Win |
| VSync | yes | yes | FULL `[source]` | — |
| Integer scaling | yes | yes | FULL `[source]` | — |
| Per-monitor DPI | yes (SDL) | yes (awareness API) | UNVERIFIED | `[native]` multi-mon |
| Internal resolution scale | yes | yes | FULL `[source]` | — |
| Shader cache / async compile | yes | yes | FULL `[source]` | — |
| Game audio output | PipeWire/SDL | XAudio2 | UNVERIFIED | `[native]` audible |
| Device enumeration | SDL | WASAPI | FULL `[source]` | — |
| Device selection (`audio_device`) | yes | yes | FULL `[source]` | Fix stale “Linux only” comment (done) |
| Hot-plug / default change | SDL events | IMMNotificationClient | PARTIAL | Disconnect path `[native]` |
| Master / speaker volume | yes | yes | FULL `[source]` | — |
| `--list-audio-devices` | yes | yes | FULL `[auto]` | — |
| Keyboard / mouse | yes | yes | FULL `[source]` | — |
| Gamepad (SDL / XInput) | yes | yes | UNVERIFIED | `[native]` |
| Button remapping | yes | yes | FULL `[source]` | — |
| Keyboard remapping | yes | yes | FULL `[source]` | — |
| Stick deadzone (L/R) | yes | yes | FULL `[source]` | Settings + config.ini |
| Trigger deadzone | yes | yes | FULL `[source]` | Settings + config.ini (2026-11-20) |
| Graphics presets | yes | yes | FULL `[source]` | Video tab Performance/Balanced/Quality |
| Config backup before reset | yes | yes | FULL `[source]` | `user_data/config_backups/` (2026-11-20) |
| On-screen FPS overlay | yes | yes | FULL `[source]` | System → Show FPS overlay |
| Pause-on-focus (mute + rumble stop) | yes | yes | FULL `[source]` | Audio suspended while guest paused |
| Launcher settings summary | yes | yes | FULL `[source]` | Display / audio / pads on Play tab |
| Input binding profiles | yes | yes | FULL `[source]` | `user_data/input_profiles/*.ini` |
| Auto save backup on launch | yes | yes | FULL `[source]` | System toggle; skipped for `--nand` |
| Mod enable/order profiles | yes | yes | FULL `[source]` | `user_data/mod_profiles/*.ini` |
| UI scale (launcher/overlay) | yes | yes | FULL `[source]` | System `ui_scale` 75–150% |
| Remappable host hotkeys | yes | yes | FULL `[source]` | F1–F12 settings/screenshot/fullscreen |
| Persist launcher tab | yes | yes | FULL `[source]` | `launcher_tab` in config.ini |
| Persist launcher window size | yes | yes | FULL `[source]` | `launcher_width` / `launcher_height` |
| FPS overlay (ms / 1% / guest / RSS) | yes | yes | FULL `[source]` | Host present + process RSS |
| On-screen input hints | yes | yes | FULL `[source]` | Jump/Slash/Pause pad+key text |
| Check for updates (GitHub) | yes | yes | FULL `[source]` | Tag compare only; no auto-install |
| Play-tab quick folders | yes | yes | FULL `[source]` | Game data / user data / mods |
| Rumble | yes | yes | UNVERIFIED | `[native]` |
| Hot-plug toasts | yes | yes | FULL `[source]` | — |
| Launcher home / Play | yes | yes | FULL `[source]` | Version on home (2026-11-20) |
| Launcher settings tabs | yes | yes | FULL `[source]` | — |
| Music Library browse / filter | yes | yes | FULL `[source]` | — |
| Music preview A/B | yes | yes | UNVERIFIED | `[native]` |
| music_asset inventory | yes | PE + launcher | PARTIAL | Needs Python stack on Win host |
| Original CPK untouched design | yes | yes | FULL `[source]` | — |
| Mods browser / GameBanana | yes | yes | FULL `[source]` | — |
| F12 screenshots | yes | yes | FULL `[source]` | — |
| Diagnostic report | yes | yes | FULL `[source]` | — |
| Crash dialogs / dump | yes | richer AV dump | UNVERIFIED | `[native]` |
| AppImage / AppDir | yes | N/A | FULL / N/A | — |
| Windows ZIP | N/A | yes | FULL `[auto]` | — |
| AP1 spiral @ wait=1 | deferred | deferred | — | **Do not work** |

---

## Automated tests

| Test | Linux | Wine / Windows PE |
|------|-------|-------------------|
| `scr_config_test` (Audio + stick deadzone) | PASS target | — |
| `scr_platform_test` | PASS | PASS |
| `scr_nand_fs_test` | PASS | — |
| `scr_nand_backup_test` | PASS | — |
| `scr_music_manifest_test` | PASS | — |
| `music_asset` inventory | PASS | PE starts (deps may be missing) |
| `tools/regression_smoke.sh` | target | — |

---

## Outstanding limitations

1. **No native Windows verification in this environment** — use `docs/windows-native-checklist.md`.
2. **music_asset runtime deps** — not bundled; see `tools/MUSIC_ASSET_WINDOWS.txt`.
3. **Exclusive fullscreen** — Linux-only; Windows stays borderless.
4. **H7 black-screen** and **AP1 spiral** — deferred; not active work.
5. Audible audio, Music Library interaction, controllers, DPI — need `[native]`.

---

## Distribution

- Exe: `out-windows/SonicColorsRecomp.exe`
- Helper: `out-windows/music_asset.exe`
- ZIP: `dist/SonicColorsRecomp-<ver>-windows-x86_64.zip`
- Checklist: `docs/windows-native-checklist.md`
