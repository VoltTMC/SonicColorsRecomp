# Native Windows verification checklist

Use on a real Windows 11 host or a Windows 11 VM with working graphics/audio/USB.
Wine results are **not** a substitute. Virtualised GPU/audio/controllers may limit
what you can prove — note that in any report.

## Setup

1. Extract `SonicColorsRecomp-*-windows-x86_64.zip` to:
   - `C:\SCR\SonicColorsRecomp` (clean)
   - `C:\SCR\Sonic Colors Recomp` (spaces)
2. Confirm `portable.txt` is present for portable mode.
3. Install game data via the launcher (own disc / extracted SNCP8P).
4. For Music Library decode/export/prepare: install Python 3.11+, `cricodecs`,
   `vgmstream-cli`, and `ffmpeg` (see `tools/MUSIC_ASSET_WINDOWS.txt`).

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
- [ ] XInput pad detect / disconnect / reconnect (+ toast)
- [ ] Dual pads if available
- [ ] Launcher and in-game focus
- [ ] Pause-on-focus-loss: game freezes, audio mutes, rumble stops

## Display

- [ ] Windowed, resize, borderless fullscreen
- [ ] VSync on/off; frame rate 30 / 60 / uncapped
- [ ] Graphics presets (Performance / Balanced / Quality)
- [ ] FPS overlay (System toggle or `--show-fps`)
- [ ] UI scale 75% / 150% (launcher + settings overlay)
- [ ] Remap settings/screenshot/fullscreen hotkeys (System → Host hotkeys)
- [ ] Resize launcher; quit and reopen — size restored
- [ ] System → Check for updates (network); Open Releases page; Download update package → `downloads\` (newest 3 kept)
- [ ] Play → Open downloads; optional CLI `--download-update`
- [ ] System → Save diagnostic report (includes `.1.log` / free space); or `--save-diagnostics`
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
