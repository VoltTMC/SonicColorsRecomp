# Native Windows verification checklist

Use on a real Windows 11 host or a Windows 11 VM with working graphics/audio/USB.
Wine results are **not** a substitute. Virtualised GPU/audio/controllers may limit
what you can prove — note that in any report.

## Setup

**Preferred:** run `SonicColorsRecomp-Setup-v*.exe` (per-user, no admin).

1. Default install: `%LOCALAPPDATA%\Programs\Sonic Colors Recomp` (`installed.txt` present).
2. Confirm user data under `%APPDATA%\SonicColorsRecomp` (not beside the exe).
3. Optional: import an old portable folder during Setup (copies; does not delete source).
4. Install game data via the launcher (own disc / extracted SNCP8P).
5. Music tools: `tools\music_deps_setup.cmd` then drop-ins in `%APPDATA%\SonicColorsRecomp\tools\bin`.

**Portable ZIP (optional):** extract `SonicColorsRecomp-*-windows-x86_64.zip` and confirm `portable.txt`.

Also test a path with spaces / Unicode for either layout.

## Audio (unmute — do not use `--mute`)

- [ ] Game BGM and SFX audible
- [ ] Settings device list shows WASAPI endpoints
- [ ] Switch output device live; audio continues
- [ ] Unplug default device (if safe); app falls back or reports clearly
- [ ] Music Library: original preview, replacement preview, pause, seek, loop, A/B

## Music Library / music_asset

- [ ] Catalogue loads (or builds via `tools\music_asset.exe catalog`)
- [ ] Preview main and `_fx` tracks
- [ ] Export original WAV (complete / loops / smpl)
- [ ] Assign replacement WAV → Prepare → cache + patched CPK
- [ ] Original `game\...\sonic2010_0.cpk` unchanged (compare hash before/after)
- [ ] Config/manifest persists across restart

## Input

- [ ] Keyboard + rebinding
- [ ] Stick / trigger dead zones feel correct
- [ ] Save / load an **input profile** (Settings → Input)
- [ ] `--list-gamepads` shows the pad on **slot 0** (SDL packed slots; `SDL3.dll` present)
- [ ] Xbox / DualShock / DualSense detect / disconnect / reconnect (+ toast)
- [ ] Launcher ImGui nav with the same pad (A confirm, D-pad / stick)
- [ ] Dual pads if available (slots pack to 0,1,…)
- [ ] DualSense / DS4 via SDL HIDAPI (no wrapper) — Wine API OK; native Windows still required
- [ ] Launcher and in-game focus
- [ ] Pause-on-focus-loss: game freezes, audio mutes, rumble stops
- Optional: `SCR_INPUT_DIAG=1` for throttled button/axis logs

## Display

- [ ] Windowed, resize, borderless fullscreen
- [ ] VSync on/off; frame rate 30 / 60 / uncapped
- [ ] Graphics presets (Performance / Balanced / Quality)
- [ ] FPS overlay (System toggle or `--show-fps`)
- [ ] UI scale 75% / 150% (launcher + settings overlay)
- [ ] Remap settings/screenshot/fullscreen hotkeys (System → Host hotkeys)
- [ ] Resize launcher; quit and reopen — size restored
- [ ] Music tab shows the full catalog (~114 tracks) from `mods\music\catalog.json` without the repo
- [ ] Status says “Replacement CPK: not prepared yet” before Prepare (not a hard error); originals still preview when tools are installed
- [ ] With game data installed, Music finds `sonic2010_0.cpk` under `game\extracted\DATA\files\` (portable) even if launched from another cwd
- [ ] Assign Replacement writes `mods\music\manifest.toml` + `tracks\`; Prepare writes `music_mods\current.json`; Play uses the same cache folder
- [ ] Music → Check tools reports missing/`unusable` deps and ready|blocked ops; after `tools\music_deps_setup.cmd` + tools in `tools\bin`, re-check is OK
- [ ] Extract ZIP to a path with spaces; drop-ins in `tools\bin` still discovered
- [ ] Prepare + export original WAV work with payload/output validation (not exit code alone); original CPK hash unchanged
- [ ] Prepare works when Python/cricodecs/vgmstream/ffmpeg are installed (see `tools\MUSIC_ASSET_WINDOWS.txt`)
- [ ] System → Check for updates (network); Open Releases page; Download update package → `downloads\` (newest 3 kept)
- [ ] After download → Reveal package / Open downloads; Play shows What's new after a version bump
- [ ] Install & relaunch on a portable zip install (exits → `downloads\apply_update.ps1` extract → relaunch); on failure check `downloads\apply_update.log`
- [ ] Confirm config/saves/mods survive Install & relaunch; AppImage path is Linux-only
- [ ] Play → Open downloads; optional CLI `--download-update`
- [ ] Quit during Verify files → confirm dialog
- [ ] Resize game window, quit, relaunch → size restored; maximize restores; minimize with pause-on-focus-loss pauses
- [ ] System → Save diagnostic report (includes `.1.log` / free space / pad names); or `--save-diagnostics`
- [ ] `--list-gamepads`; System → Open shader cache / Clear shader cache…
- [ ] `--verify-data-quick` (and optional full `--verify-data` when time allows)
- [ ] After Play once, relaunch launcher → Last played timestamp on Play tab
- [ ] Reinstall from disc (when data OK) → confirm dialog; Copy next to Open game/user data
- [ ] System → Copy log path / Copy user data path; after a forced crash, Play banner → Save diagnostic report
- [ ] FPS overlay shows guest Hz + memory; input hints for Jump/Slash/Pause
- [ ] Save/load a **video profile** (Settings → Video)
- [ ] Missing/corrupt data: Releases / log recovery buttons work
- [ ] Move between monitors; DPI change (125% / 150%)

## User data

- [ ] With `portable.txt`: data next to exe
- [ ] Without `portable.txt` (rename aside): `%APPDATA%\SonicColorsRecomp`
- [ ] `--portable` forces exe-local
- [ ] Reset settings creates `config_backups\config_*.ini`
- [ ] Auto save backup on launch (optional) writes `save_backups\`
- [ ] Auto save backup after write (optional) after stage clear / menu save; prune respects Keep N
- [ ] Unplug default audio device mid-game → toast “Audio output reconnected/lost”; sound returns on replug
- [ ] Mod profile Save/Load under `mod_profiles\`

## Record

Note host OS build, GPU, whether VM or bare metal, and any failures with logs from
the user data directory.
