# Progress log

## 2026-11-20 — `--show-hints` + HiDPI checklist

- CLI `--show-hints` for this-run input prompts; PERFORMANCE.md HiDPI/resize smoke steps.

## 2026-11-20 — Perf overlay detail + input hints

- FPS overlay: guest Hz, RSS MB, audio underruns; `Platform_ProcessRssMb`.
- System → Show input hints (Jump/Slash/Pause pad+key prompts).

## 2026-11-20 — Play-tab quick folders

- Open game data / user data / mods; Copy version on the Play tab.

## 2026-11-20 — Launcher size, richer FPS overlay, update check

- Persist launcher window size; FPS overlay shows ms + approx 1% low.
- System → Check for updates (GitHub Releases latest tag; no download).

## 2026-11-20 — Persist launcher tab

- `[System] launcher_tab` remembers Play/Settings/Mods/Music across launches.

## 2026-11-20 — Hotkey conflict warn + startup timing + Music empty state

- Warn when two host hotkeys share a key; log `startup: guest entry ready in N ms`.
- Music Library empty-catalog guidance bullets.

## 2026-11-20 — Remappable host hotkeys

- `[System] hotkey_settings|screenshot|fullscreen` (F1–F12); System UI combos; Linux/Win handlers.
- Included in input profiles; Escape still closes overlay; Alt+Enter still toggles fullscreen.

## 2026-11-20 — Native checklist + launcher status polish

- Expanded `docs/windows-native-checklist.md` for profiles, FPS overlay, UI scale, backups.
- Launcher Current settings shows UI scale and auto-backup hint.

## 2026-11-20 — UI scale

- `[System] ui_scale` (75–150%) for launcher + overlay; System slider; config_test roundtrip.

## 2026-11-20 — Mod profiles + PERFORMANCE.md

- Named mod enable/order profiles under `user_data/mod_profiles/`; Mods tab Save/Load.
- `docs/PERFORMANCE.md` measurement notes; backlog item 12/15 updated.

## 2026-11-20 — Auto save backup on launch

- `[System] auto_backup_saves_on_launch` + System checkbox; runs `nand_backup` before Play (not for `--nand`).

## 2026-11-20 — `--show-fps` + backlog audit

- CLI `--show-fps` enables the overlay for the current run without editing config.ini.
- `docs/DEVELOPMENT_ROADMAP.md` top-20 improvement backlog (spiral/H7 excluded).

## 2026-11-20 — Input binding profiles

- Save/load controller + keyboard + deadzone profiles under `user_data/input_profiles/`.
- Input settings UI: name, Save, Load combo, open folder; `scr_config_test` roundtrip.

## 2026-11-20 — Focus-loss mute + launcher status

- Pause-on-focus-loss mutes host audio (`AudioOut_SetSuspended`) and stops rumble immediately.
- Launcher Play tab shows current display/audio/controller summary from config.

## 2026-11-20 — Config backup on reset + FPS overlay

- `Config_BackupCurrent` writes `user_data/config_backups/config_YYYYMMDD_HHMMSS.ini` before System → Reset.
- `[System] show_fps_overlay` + in-game corner FPS (host present rate); toast confirms backup filename.
- `scr_config_test` covers show_fps_overlay roundtrip and backup path.

## 2026-11-20 — Trigger deadzone + graphics presets

- `trigger_deadzone` + Input UI; Video graphics presets (Performance/Balanced/Quality).
- Launcher first-run bullets when data missing; README Releases play path + Windows honesty.
- `scr_config_test` extended for trigger roundtrip.

## 2026-11-20 — Continuous improvement (parity + QoL)

- Fresh audit → `docs/WINDOWS_FEATURE_PARITY.md` matrix (FULL/PARTIAL/MISSING/UNVERIFIED/PLATFORM LIMITATION).
- Removed experimental AP1 spiral gates; issue **DEFERRED** in FRAME_RATE / audit / roadmap.
- Stick deadzone L/R in config + Input settings; `scr_config_test` covers roundtrip.
- Launcher home shows build version; `audio_device` docs corrected for Windows WASAPI.
- Validated: linux-clang build, MinGW `out-windows/SonicColorsRecomp.exe`, `regression_smoke` OK.

## 2026-11-20 — 60 FPS compatibility audit (no code fixes)

Wrote `docs/FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md`: Dolphin wiki + Colors DX
issues A–G classified (WideSpring soft-lock **ALREADY FIXED**; flicker leans
**DOLPHIN-SPECIFIC**). **Issue B CONFIRMED** in recomp: Aquarium Park Act 1
(`stg410`) opening loop — stuck and/or shot into void/death at wait=1. Fix not
AP1 spiral (`stg510`): **DEFERRED indefinitely**. Experimental spiral gates removed
from the tree. Do not resume without an explicit new plan.

---

## 2026-11-20 — Spring soft-lock second RCA (gate audit)

### Why first `--spring-cd-gate` (early-return) was insufficient

- Boot log ≠ in-stage proof the override ran during the bounce.
- Pointer filter was MEM1-only (`>>28 == 0x8`); MEM2 springs would bypass.
- Early-return ≡ native cooldown epilogue, yet soft-lock persisted → either gate
  never engaged, or **repeated `ObjSpringActivate` is not the sole cause**.

### Changes

- Gate now **ORs bit15 into `+0xF0`** then calls original (native `+0x188` path).
- MEM1+MEM2 EA check; `--spring-diag` event log; `ObjSpringLaunch` hook;
  `Perf_GuestFrameIndex()`.
- Default 30/60 unchanged. True-60 fix **not claimed** until diag capture.

### Diag sessions (`spring_diag_guest60.log` / `guest60b.log`)

stg110 guest60c: soft-lock = **WideSpring** (`wact` every frame, `col=act=0`).
Cause: collide→activate has no cooldown. Gate early-returns WideSpring activate for
~0.5s guest time. **Verified:** soft-lock gone with gate. **Launcher:** `frame_rate=60`
now defaults to wait=1 + spring gate (no separate `--guest-60` / `--spring-cd-gate`).

### Dolphin AR (unchanged conclusion)

`008F2D97` / `0090DB0B` = force wait=1 only; same as `--guest-60`.

---

## 2026-11-20 — 60 FPS spring soft-lock (wait-field policy)

### Root cause

`--60fps` / `frame_rate=60` previously set VI wait fields to **1** (true 60 Hz
guest). Float dt from `fn_803E19B0` scales with N, but spring object state at
`CObjSpring+0xE0` / `+0xF0` (`fn_80078C24`, `fn_80078524`) advances once per
guest update. At wait=1 the spring re-arms while Sonic still overlaps →
infinite activation loop. Same class as Dolphin AR 60fps / Ultimate spring bugs.

### Fix

`Perf_WaitFields()`: `frame_rate=60` → **wait=2** (native sim, spring-safe).
Opt-in true guest-60 via `SCR_GUEST_60=1` / `--guest-60` (wait=1, known broken).
Status text: `60/sim30` vs `60/guest`. Docs: `docs/FRAME_RATE.md`.

### Verify

- Build: `linux-clang` link OK after `crash.cpp` includes `perf.h` (C linkage).
- Boot log: `--60fps` → `wait=2`; `--60fps --guest-60` → wait=1 warn.
- Matching decomp untouched. H7 black-screen still deferred.

---

## 2026-11-19 — Ongoing development (M7 + `--help` + exclusive FS + first-launch UX)

- Extracted `Fs_WriteFileAtomic`; NAND `Flush` uses it (C1 behaviour unchanged).
- Added `scr_nand_fs_test` (create / replace / read-only parent / rename-onto-dir).
- CLI `-h` / `--help` prints usage and exits before launcher/log.
- Added `docs/DEVELOPMENT_ROADMAP.md`.
- Config/settings: `exclusive_fullscreen` (Linux exclusive mode; default borderless).
- First-launch / missing-data: install dialogs say Sonic Colors (not Black Knight); launcher shows supported release + Play disabled reason; `--skip-launcher` / empty data fatals mention `scr_install` + `kReleaseNames`; quick-check before boot.
- `tools/package_smoke.sh`: stages AppDir, checks libs/desktop/icon, `--help`, `BKR_UI_SHOT` abort.
- Long-session VRAM: tex/EFB age+hard-cap eviction; hi-res texture LRU; GX log includes efbCopies/hires.
- Win32 window title → “Sonic Colors - Recompiled” (class name unchanged).
- Controller hotplug: pack slots on disconnect; probe already-connected pads at event-loop start; Input settings shows live controller list (`Pad_Count` / `Pad_Name`).
- GameBanana game id **6243** (Sonic Colors; was Black Knight 6511).
- Win32 window/launcher classes renamed to `SonicColorsRecomp` / `SonicColorsRecompLauncher`.
- UI/test env vars prefer `SCR_*` with `BKR_*` fallback (`runtime/env.h`); smoke/layout scripts updated.
- System settings: save folder open / backup / restore latest (`nand_paths` + `nand_backup`); `scr_nand_backup_test`.
- Debug envs (`SLOW_SAMPLE`, `IRQ_STALLS`, `TEST_PAUSE`, `DUMPMEM2`) accept `SCR_*`.
- Host volume: `master_volume` / `speaker_volume` in config + Audio settings (live via `AudioOut_ApplyVolumes`).
- Linux audio device picker (`audio_device`); Windows remains system default.
- System → Diagnostics: show log path + Open log folder (`Log_Path`).
- `scr_config_test`: Audio/Video config load/save roundtrip.
- H8 PART: `tools/music_pipeline_smoke.sh` — isolated TR1 prepare → patched CPK + payload proof (PASS). In-game audibility still open.
- Optional integer scaling (`integer_scale`); `tools/regression_smoke.sh`; desktop Keywords / PrefersNonDefaultGPU.
- F12 screenshot to `screenshots/scr_*.bmp`; System → Open screenshots folder.
- Uncapped host FPS limit (`host_fps_cap`); live Video setting.
- `--version`, `--portable` / `SCR_PORTABLE`; System shows user data path + Open user data folder.
- AppStream metainfo packaged into AppDir (`usr/share/metainfo/`).
- Linux audio hotplug: reopen preferred/default device on SDL remove/add.
- System → Reset settings to defaults (keeps Paths/Mods/Music); README status updated.
- System → Save diagnostic report (`diagnostics/scr_diag_*.txt`).
- F12 user screenshots → full-res PNG (`screenshots/scr_*.png`).
- In-game toasts for F12 + Linux controller hotplug.
- `--list-audio-devices`; Win32 XInput hotplug toast; Linux audio reconnect toast.
- Save restore: pick any backup; diagnostic path copied to clipboard.
- `tools/package_test_linux.sh`; `tools/package.ps1` (Windows share pack).
- Overlay Restart/Quit confirmation; package_smoke checks pack scripts.

Verification: build PASS; `package_smoke` OK.

---

## 2026-11-19 — Stability recovery (Phase 20 black-screen work reverted)

### Goal

Stop black-screen experimentation. Restore the Phase 19 (`72ae9de`) runtime/presentation baseline and treat H7 as a known deferred bug.

### Reverted (black-screen only)

| Ref | What |
|---|---|
| `388ee48` | Phase 20 XFB centre/HUD luminance + world/hud counters |
| `509781e` | EFB-copy `ramHash` / keep-GPU-snapshot “fix” |
| `0f8bc9c` | EFB copy-clear always-clear-depth change |
| `de7a72e` | Expanded BLACK-WORLD dumps / `SCR_GX_NO_DEPTH` probe |
| Uncommitted WIP | Richer latch (`efbC`/`efbD`/frustum/`mtxNan`), PE token `GX_SyncGPU`, guest-60 gate, indexed-XF gather expand + 256KiB lag cap, NaN XF skip |

### Preserved

Music, launcher, NAND C1, input, packaging. Frame-rate: default 60 now wait=2 (spring-safe; see 2026-11-20). Matching decomp untouched.

### Recovery points

- Branch: `backup/pre-stability-recovery-de7a72e` (= `de7a72e`)
- Stash: `WIP black-screen investigation before stability recovery …` (post-`de7a72e` WIP)

### Baseline

`main` → `72ae9de` (Phase 19).

### Stability notes this pass

- No new runtime crash/leak fixes beyond restoring Phase 19 (NAND C1 already present).
- `tools/music_mod_pkg/test_music_mod.py::test_sample_repo_manifest` was failing on assets-only `manifest.toml`; skip when no legacy `[tracks.*]` (Music Library format).

### Verification

| Check | Result |
|---|---|
| `runtime/` vs `72ae9de` | **identical** |
| Build `SonicColorsRecomp` (linux-clang) | **PASS** |
| `scr_music_manifest_test` | **PASS** |
| `music_asset` unit tests | **8/8 PASS** |
| `music_mod` unit tests | **PASS** (1 skipped) |
| `package_linux.sh -S` | **PASS** → `dist/AppDir` (AppRun + `usr/bin/SonicColorsRecomp`) |
| Binary start (no display) | Process starts; launcher falls through without window (expected headless) |
| Manual TR1 / black-screen | **Not** re-run (deferred bug; no BS code path left) |

---

## 2026-11-19 (Phase 19) — Quality audit, critical save fix, release polish

### Inventory

Full severity table: [`docs/QUALITY_AUDIT.md`](QUALITY_AUDIT.md).

### Critical / high fixes

- **NAND Flush**: no longer clears dirty after failed rename/write (`runtime/ios/dev_fs.cpp`).
- **gitignore**: track `mods/music/catalog.json` + README; ignore userdata tracks/manifest.
- **Launcher**: `BKR_UI_SHOT` with no window/D3D aborts instead of starting the game.
- **Music paths**: tool/cache/catalog resolution via cwd + exe parents + `SCR_MUSIC_ASSET` / user dir.
- **Branding**: crash/fatal/install/HTTP/config/user-dir docs → SonicColorsRecomp.
- **Packaging**: `tools/package_linux.sh` + `data/linux/soniccolorsrecomp.{desktop,svg}`.
- **Black-screen RCA aid**: `SCR_GX_XFB_DIAG=1` logs zero-draw XFB presents (no render workaround).

### Still open

- Intermittent black world after stomp (HUD OK) — **DEFERRED** (see stability recovery above); not an active RCA.
- TR1 in-game music replacement audibility confirmation.
- Full-game compatibility matrix beyond TR1.

### Verification this phase

| Check | Result |
|---|---|
| Incremental build (`SonicColorsRecomp`) | **PASS** |
| `scr_music_manifest_test` | **PASS** |
| `music_asset` unit tests | **8/8 PASS** |
| `package_linux.sh -S` | **PASS** → `dist/AppDir` |
| Manual TR1 play | **Not** re-run (efficiency; no gameplay change beyond diagnostics) |

---

## 2026-11-19 (Phase 18) — Sharp resize, WAV export, player panel

### Blurry resize — root cause

ImGui lays out in **logical** pixels (`DisplaySize`) and draws into a framebuffer
of `DisplaySize × DisplayFramebufferScale`. The DXGI/DXVK swapchain was only
resized from coalesced `PIXEL_SIZE_CHANGED` events and could stay at a **stale
pixel size** while the Wayland compositor stretched the previous buffer → soft /
blurry UI after (and during) live resize. Restarting “fixed” it by recreating
the swapchain.

### Rendering fix

- `UiWindow_Pump` always reports current `SDL_GetWindowSizeInPixels` (Linux) /
  client rect (Windows).
- Launcher `SyncSwapchainToWindow` every frame when drawable size ≠ backbuffer,
  and again after ImGui NewFrame using `DisplaySize × FramebufferScale`.
- Screenshot path sets `FramebufferScale = 1` when forcing pixel DisplaySize.

### Original WAV export

- `tools/music_asset export-original` / `export-all`
- Modes: complete (`-i`), expanded loops (`-l N`), smpl (`-i -L`)
- PCM 16/24-bit; atomic publish; refuse overwrite by default
- Launcher: **Export Original as WAV** + export options tree
- TR1 export test: 48 kHz stereo, ~137 s, `lossy_source=true`

### Music Library player

Integrated detail panel: track info, ORIGINAL / REPLACEMENT transport, Compare,
Export, Assign Replacement. Side-by-side when wide; stacked when narrow.

### Verification

| Check | Result |
|---|---|
| Build | PASS |
| Unit tests | 8/8 PASS (incl. export) |
| Shots | `out/ui_shots/music_p18/` (800 / 1280 / 1920) |
| Exportable music assets | 114 (catalog) |
| Interactive Wayland live-resize sharpness | Fixed by per-frame swapchain sync (confirm in session) |

---

## 2026-11-19 (Phase 17) — Music preview clicks + launcher resize

### Preview controls — root cause

Table rows used `ImGuiSelectableFlags_SpanAllColumns` without
`AllowOverlap`. The selectable’s full-width hitbox intercepted pointer events
meant for later-column widgets (and made in-row Preview buttons unusable).

Original Play also ran `std::system(preview-original)` on the UI thread, so a
successful click could freeze the launcher until decode finished.

### Fixes

- Removed in-row Preview buttons; selection-only rows + detail pane controls.
- `SpanAllColumns | AllowOverlap` on table selectables.
- ASCII **Play / Pause / Stop** for Original and Replacement, plus Compare A/B.
- Async original AAX decode (background thread → WAV → SDL preview).
- Transport bar: seek, loop, volume, stop; switching sources stops the previous
  stream; changing selection stops playback.
- Wide layout: side-by-side list | detail. Narrow (`<1000` / short height):
  detail (preview) above the scrollable list.

### Responsive layout — root cause

Music Library used a fixed ~14-line table height and packed `SameLine` filters,
computed pane heights from **initial** `GetContentRegionAvail()` before drawing
the header (so list+detail overflowed / clipped), and had no window minimum
size. Resize itself was wired (`PIXEL_SIZE_CHANGED` → DXGI `ResizeBuffers`);
the breakage was content layout, not missing resize events.

### Fixes

- Flexible remaining-space split; side-by-side vs stacked.
- Wrapping filter bar; compact header.
- Launcher min size **800×560** (`SDL_SetWindowMinimumSize` / `WM_GETMINMAXINFO`).
- `BKR_UI_SIZE=WxH` for layout shots; `tools/music_ui_layout_shots.sh`.

### Verification

| Check | Result |
|---|---|
| Build `SonicColorsRecomp` | PASS |
| Music asset unit tests | PASS (unchanged pipeline) |
| Layout shots 800×600, 1280×720, 1366×768, 1920×1080 | PASS → `out/ui_shots/music/` |
| 2560×1440 | Clamped to ~90% of display usable bounds (expected) |
| High-DPI | `SDL_WINDOW_HIGH_PIXEL_DENSITY` + ImGui framebuffer scale unchanged |
| Preview clickability | Hit-test fixed; decode async; controls visible in detail pane |

Remaining: interactive click automation under Wayland needs a real session
(display); X11 `BKR_UI_SHOT` used for layout screenshots.

---

## 2026-11-19 (Phase 16) — Complete Music Asset Library + original preview

Expanded stage-only music mods into a full soundtrack Music Library.

### Inventory

| Metric | Value |
|---|---|
| Archives with music | 1 (`sonic2010_0.cpk`) |
| AAX total in CPK | 590 |
| Confirmed music (`bgm_*`) | **114** |
| Voice excluded (`ve_*`/`vj_*`) | 239 + 237 |
| FX variants | 36 |
| Categories | stage 46, special 33, boss 11, menu 9, map 8, jingle 7 |

Catalog: `mods/music/catalog.json` via `./tools/music_asset catalog`.

### Implementation

- Resource-based `[assets.*]` manifest (+ legacy `[tracks.*]` migration in prepare).
- Target-specific AAX conversion (1/2/N segments from original template).
- Launcher Music Library: search/filters, per-asset assign/enable/restore,
  **Original** + **Replacement** preview, Compare A/B, Prepare All.
- Original preview: extract AAX from unmodified CPK → vgmstream →
  `userdata/music_preview/` (independent of replacement cache).
- Runtime: `overrides[]` multi-archive registration via `Disc_RegisterHostOverride`
  (SNCP8P still one CPK).

### Tests

```bash
env -i HOME="$HOME" PATH="/usr/bin:/bin" \
  PYTHONPATH="$PWD/tools:$PWD/.venv-music/lib/python3.14/site-packages" \
  /usr/bin/python3.14 -m unittest music_asset_pkg.test_music_asset -v
./out/scr_music_manifest_test
```

Covered: catalog music-only, TR1 main/FX paths, original preview decode,
legacy+resource manifest load, assets save/load/duplicate reject, AAX UTF align,
CPK TOC (existing).

### Still pending user-side

Short TR1 enter for audible CRI/AX confirmation after Phase 15 alignment fix
(unchanged requirement).

---

## 2026-11-19 (Phase 15) — CRI playback diagnosis (AAX UTF alignment)

In-game replacements were still silent after Phase 14 TOC size fixes. Static
tests isolated the failure to **generated AAX container layout**, not CPK I/O.

### Static tests

| Test | Result |
|---|---|
| A — mods disabled / original CPK | PASS (baseline; original music path unchanged) |
| B — original AAX byte-identical round-trip through patcher | PASS (no-op archive; extract identical) |
| C — substitute original `bgm_stg210_fct_wav.aax` into TR1 slot + TOC sizes | PASS (archive/`Cpk` extract byte-identical; CPK at `out/p15/cpk_subst_stg210.cpk`) |
| D — generated diag AAX (pre-fix) | FAIL structurally vs originals (see below) |
| D′ — aligned diag AAX (`data_abs=88`, gap=6) | PASS structural match; cache `userdata/music_mods/f6b81e478f6da1bb/` |

### Root cause (generated AAX)

| Field | Original TR1 / other stages | `Aax.replace_segment` output |
|---|---|---|
| `@UTF` name | AAX | AAX |
| Columns | data VLDATA + lpflg UINT8 | same |
| `data_abs` (8 + data_off) | **88 (8-aligned)** | **82 (misaligned)** |
| Inter-segment VLDATA gap | 2–6 (8-align pad) | **0** |
| ADX | v4 / mode 3 / 48 kHz | same |
| vgmstream | plays | plays |
| Wii CRI | plays | **silence** |

Fix: pack with `utf.Utf.create` + `set_data_alignment(8)` instead of relying on
`replace_segment` alone (`pack_method=utf_data_alignment_8`).

### Instrumentation

- Full SHA-256 proof on CPK payload read (`payload_proof.tsv`).
- `--music-debug`: guest `bgm_*` strings, AX **stream voice peak** (host proxy for
  decode; guest CRI parser symbols are not in `tools/sdk_names.txt` / decompdecomp
  was unreachable this session).

### Not yet proven in this session

Audible TR1 playback after the alignment fix (requires one controlled launch).
Manifest is set to `_diag_tone_48k.wav` for that test. After PASS, switch back to
the user WAV and Prepare again.

### Build / test

```bash
cmake --build --preset linux-clang
env -i HOME="$HOME" PATH="/usr/bin:/bin" \
  PYTHONPATH="$PWD/tools:$PWD/.venv-music/lib/python3.14/site-packages" \
  /usr/bin/python3.14 -m unittest music_asset_pkg.test_music_asset -v
./out/SonicColorsRecomp --music-debug   # then enter TR1
```

---

## 2026-11-19 (Phase 14) — Fix CPK silence + Music Mods GUI

### Root cause of silence

Phase 13 wrote a **shorter** AAX into the original CPK slot and zero-padded the
remainder, but left TOC `FileSize` / `ExtractSize` at the **original** length.
CRI read past the valid `@UTF` container into padding → silence.

Decoded AAX audio was fine (vgmstream peak/RMS non-zero). Manifest under test was
`stg310` (not TR1). `Cry_For_Me.wav` has ~3.9 s leading silence (not the primary
cause). Silent duration-trim of oversized AAX was also unsafe.

### Fixes

1. **CPK patcher** updates TOC `FileSize`/`ExtractSize`; relocates oversized
   payloads to archive end (aligned) and updates `FileOffset`. Never truncates.
2. **WAV→AAX** no longer shrinks duration to fit a slot.
3. **Main AAX only** by default (`--no-fx` / `--with-fx` to opt in).
4. **Runtime proof**: `payload_proof.tsv` + `MusicMod_OnCpkHostRead` logs when the
   disc layer returns the replacement AAX head bytes.
5. **Launcher Music tab**: Prepare Music Mods (async), leading-silence warning,
   AAX/CPK status column, prepare log; auto-prepare on Play unchanged via
   `MusicMod_Init`.

### Diagnostic prepare (TR1 main)

| Item | Value |
|---|---|
| WAV | `mods/music/tracks/_diag_tone_48k.wav` (8 s, 48 kHz, −12 dBFS, no lead silence) |
| Replaced | `sound/Synth/bgm_stg110_rso_wav.aax` only |
| FX | original |
| Cache | `userdata/music_mods/548b397f48d6911b/` |
| TOC size | 432200 (= AAX bytes; was 7394720) |
| Original CPK SHA | `58bbf2be…` unchanged |

### Still required for full sign-off

Enter Tropical Resort Act 1 with the diag tone; confirm CRI playback + log lines
`PROOF: CPK read supplies replacement AAX`. Then swap in the user soundtrack and
Prepare again.

### Tests

```bash
env -i HOME="$HOME" PATH="/usr/bin:/bin" \
  PYTHONPATH="$PWD/tools:$PWD/.venv-music/lib/python3.14/site-packages" \
  /usr/bin/python3.14 -m unittest music_asset_pkg.test_music_asset -v
```

---

## 2026-11-19 (Phase 13) — File-based CPK/AAX music replacement

Replaced the host MixInto / AX-silence overlay with genuine asset replacement.

### Pipeline

1. WAV → 48 kHz PCM (`ffmpeg`) → ADX intro+loop (`cricodecs`) → AAX via segment replace on original template
2. Copy `sonic2010_0.cpk` → `userdata/music_mods/<hash>/`
3. In-place payload patch at TOC `FileOffset` (full CPK rebuild blocked by duplicate file IDs in SNCP8P)
4. `Disc_RegisterHostOverride("sonic2010_0.cpk", patched)` before `Disc_Open`

### TR1 paths replaced

- `sound/Synth/bgm_stg110_rso_wav.aax`
- `sound/Synth/bgm_stg110_rso_fx_wav.aax`

### Proof

| Check | Result |
|---|---|
| Original CPK SHA-256 | `58bbf2be…` unchanged |
| Patched archive | `userdata/music_mods/2965c969c835c923/sonic2010_0.cpk` |
| AAX validates (CriCodecs / vgmstream) | yes (2 segments, 48 kHz) |
| Launch redirect | `disc: host override sonic2010_0.cpk → …` |
| Host overlay | disabled (`MusicMod_MixInto` / AX silence are no-ops) |

### Tools / deps

- `tools/music_asset` + `tools/music_asset_pkg/`
- `.venv-music` + `cricodecs`, system `ffmpeg`

### Build

```bash
ninja -C build/linux-clang SonicColorsRecomp
./tools/music_asset prepare --manifest mods/music/manifest.toml --music-dir mods/music \
  --cpk game/SNCP8P/files/sonic2010_0.cpk --cache userdata/music_mods
```

---

## 2026-11-19 (Phase 12) — Custom music actually replaces stage BGM

Gameplay previously still heard the original soundtrack. Phase 10c’s architecture
was wrong on three points; all were proven with `--music-debug` on TR1.

### Exact failure points

1. **Stage ID** — disc FST `stg*` first-touch never fires (stages live in CPK).
2. **Start gate** — required AX `is_stream` rising edge; TR1 BGM is `is_stream=0`
   looping refill buffers (`span≈0x59f`–`0xfff`, `vol` high).
3. **Stop / silence** — “stream ended” debounce stopped custom audio; silence of
   only `is_stream` left the original theme audible under the host mix.

### Fix

| Piece | Change |
|---|---|
| Stage | `PPC_OVERRIDE(sprintf)` @ `0x80379820` → `MusicMod_OnStageName` from mission/orc paths + result buffer |
| Start | Mapped stage arms pending; start on AX voice activity (not stream-only). `--music-force-stage` still for pipeline tests |
| Stop | Stage clear/change only (no stream-off stop) |
| Suppress | `is_stream` **or** loud looping refill PBs when stream count is 0 |
| Mix | Unchanged once-per-batch; leading near-silence skip for tracks like Cry_For_Me |
| Debug | `--music-debug` counters + `[MUSIC][ax]` voice dumps |

### TR1 integration evidence (`out/music_p12_tr1.log`)

```text
[MUSIC] Application launched — no custom playback
[MUSIC] Current stage: stg110 (act 1, via scripts/%s_mission.lua)
[MUSIC] Original stage BGM identified: bgm_stg110_rso (stage stg110)
[MUSIC] Replacement mapping matched: tracks/stg110_Cry_For_Me__….wav
[MUSIC] Custom playback started
[MUSIC] Custom samples reaching host output (peak=659)
[MUSIC][dbg] … suppressed=668 … mixPeak=27522 …
```

AX dump during TR1 BGM: `stream=0 loop=1 fmt=2 span=59f vol=48d0` (not `is_stream`).

### Build / test

```bash
ninja -C build/linux-clang SonicColorsRecomp
./out/scr_music_manifest_test
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --mute \
  --music-mods mods/music --music-debug --nand userdata \
  --input-script /tmp/scr_gameplay.txt
```

### Files touched

`runtime/audio/music_mod.*`, `music_sprintf_hle.cpp`, `ax.cpp`, `docs/MUSIC_MODDING.md`, `docs/PROGRESS.md`

---

## 2026-11-19 (Phase 10c) — Fix custom music: cue gate + mix quality

### Root causes

| Symptom | Cause |
|---|---|
| Custom WAV at launch / over menus | Rolling MEM scan treated **static** `bgm_stg*` CPK/DOL strings as play events |
| Malformed / distorted playback | `MusicMod_MixInto` ran on the **entire** DSP staging buffer on every partial `PushDSP`, advancing the music cursor repeatedly and re-mixing old frames |

### Fix

- **Trigger**: disc path → `stgNNN` context + AX `is_stream` rising edge (not string scan).
- **Suppress**: unchanged selective silence of AX `is_stream` voices only.
- **Mix**: once per submitted audio batch; soft-clip float→s16; preload WAV when stage context appears.
- **WAV**: skip `JUNK`/`LIST`; WAVEFORMATEXTENSIBLE PCM/float GUIDs.

### Verification

| Claim | Status |
|---|---|
| No playback on manifest load / boot (log line) | Code + short boot log |
| Mix-once-per-batch | Code (`audio_out` linux/win32) |
| PCM decode golden peak (test tone) | `scr_music_manifest_test` |
| Full TR1 hear-test | **Targeted once** after this fix |

---

## 2026-11-19 (Phase 10b) — Music Mods launcher GUI

Integrated custom soundtrack management into the existing ImGui launcher (not a separate app).

### Launcher integration

| Piece | Change |
|---|---|
| Tabs | Launcher: Play / Settings / Mods / **Music**; in-game overlay: Settings / Mods / **Music** |
| UI | `runtime/ui/music_ui.*` — stage table, editor modal, WAV browse/copy, preview, remove confirm |
| Shared API | `runtime/audio/music_manifest.*` used by runtime + GUI |
| Config | `[Music] enabled` / `music_dir` in `config.ini` (already read by `music_mod.cpp`) |
| Preview | Launcher-only SDL3 stream (Linux); stops on tab leave / exit; does not touch AX |

### Layout

`mods/music/manifest.toml` + `mods/music/tracks/*.wav` (sample TR1 tone migrated under `tracks/`).

### Tests / docs

- `out/scr_music_manifest_test` — load/save/import/status/duplicates/unsafe paths
- Extended `tools/music_mod_pkg/test_music_mod.py`
- `docs/MUSIC_MODDING.md` — GUI workflow

### Verification

| Claim | Status |
|---|---|
| Music tab in launcher/overlay | Code wired |
| Manifest GUI↔runtime same file | Code (`MusicManifest_*`) |
| WAV import + validation | Code + automated test |
| Preview isolated from game AX | Code (separate SDL device) |
| TR1 GUI→launch hear custom BGM | **Targeted once after rebuild** |

---

## 2026-11-19 (Phase 10) — Custom stage music (WAV) replacement

Host-side BGM replacement without touching the ISO or extracted assets.

### Architecture (code)

| Layer | Role |
|---|---|
| Guest cues | Stage themes are CRI/Synth cues e.g. `bgm_stg110_rso` (from `sonic2010_0.cpk`) |
| Detection | Rolling MEM1/MEM2 scan for `bgm_stg*` / stop cues (`bgm_sys_*`, `bgm_zmap_*`) each frame |
| Silence | AX `is_stream` voices output silence while custom track active (SFX one-shots unchanged) |
| Playback | WAV decode → mix into `AudioOut_PushDSP` staging (SDL3); loop / fade / volume from manifest |
| Config | `mods/music/manifest.toml`; CLI `./tools/music_mod`; optional `--music-mods <dir>` |

### Deliverables

- Runtime: `runtime/audio/music_mod.*`, `wav_file.*`; hooks in `ax.cpp`, `audio_out`, `perf`/`FrameLoopWait`, `main`
- Sample: `mods/music/manifest.toml` + `tracks/tropical_resort_act1.wav` (test tone for `stg110`)
- CLI + tests: `./tools/music_mod`, `python3 tools/music_mod_pkg/test_music_mod.py`
- Docs: `docs/MUSIC_MODDING.md`

### Verification

| Claim | Status |
|---|---|
| Manifest parse / WAV inspect / stage catalog | Automated (`test_music_mod.py`, `music_mod validate`) |
| Cue IDs for all story acts | Code (CPK string table) |
| Stream silence leaves non-stream SFX | Code (AX `is_stream` gate) |
| TR1 hears custom WAV + SFX | **Not yet verified** (targeted boot→TR1 once; use sample tone) |
| Pause follows AI DMA | Code |

### Next

One targeted TR1 run with music mods (GUI or `--music-mods mods/music`): confirm logs + tone vs original + ring SFX.

---

## 2026-11-19 (Phase 9) — Boost path analysis, black-screen notes, Act 2 deferred

Testing policy: code/instrumentation first; no repeated TR1 playthroughs. Existing Phase 8/9g shots + logs treated as valid evidence.

### A — White Wisp boost (before DashPanel #1)

| Claim | Verification | Evidence |
|---|---|---|
| Capsule open → 3 White Wisps → gauge fill → B tip | Previously verified (unchanged) | Phase 8 shots; `phase9_capsule_open.png` |
| Upright + energy before `DashPanel #1` (~−6618, 283, 4583) | Targeted gameplay (Phase 9g) | `phase9_upright_before_dash.png`, `phase9g_0240`–`0280` |
| `CPluginStateBoost` enter runs on B | Code + instrumentation | `boost:` in `out/phase9_boost7.log` / `phase9_boost8.log`; type `u08=80714ba8` |
| `sn_boost` audio path invoked from enter | Code | `fn_800888CC` → `fn_8020E3CC` with `0x80714bbc` |
| Persistent gauge at plugin `+0x14/+0x18` | **Disproved (code)** | Enter writes SDA scratch float to both; zeros `+0x1C` |
| Clean run-boost + energy drain + terminate | **Not yet verified** | Idle/low-LY scripts held B without forward run; no cyan boost ball / drain curve captured |

**Call graph (code):**
- Vtable `@ 0x8075e0a4` → `fn_800888CC` (enter), `@ 0x8075e0a8` → `fn_80088CB8` (tick).
- Enter: `fn_80033A30` / `fn_80033A44` on player (`r4`); `fn_80033920(player, 0x19)` (component lookup via `player+0x28` → `fn_800260B0`); sets flag on subsystem `+0x8`; plays `sn_boost`.
- Tick: branches on plugin `+0x1C` → `fn_80088D28` (0) / `fn_80088F0C` (1), then `fn_8008926C`.
- State-0 (`fn_80088D28`): looks up component `0x1A`; may write plugin `+0x18` and set `+0x1C=1` (visible via `boost:tick`).

**Instrumentation (ready, no extra play loop):** `--boost-debug` logs `boost:enter` / `boost:tick` with `player`, `st1c`, scratch floats, B, LY. Overrides: `BoostPluginSnBoost` `800888CC`, `BoostPluginTick` `80088CB8`.

**Manual one-shot procedure (do once when convenient):**
1. From glass path, open capsule `#46` ≈(−7299.9, 342, 4583.9); wait for B tip + gauge.
2. Stop short of DashPanel #1; stay upright.
3. Hold stick **forward** (LY ≳ 0.5) + **B**; no A; do not touch dash panels.
4. Expect: cyan boost aura/ball, speed, gauge drain while held, stop when empty. Capture 2–3 shots + `boost:tick` float/state lines.
5. If fail: return to code (player component `0x19` / `fn_80088D28` energy), do not re-farm the opening.

### B — Black-screen (Dest/Iron → Spring)

| Claim | Verification | Evidence |
|---|---|---|
| Manual repro (world black, HUD stays; restart recovers) | Previously verified | Prior manual report |
| Scripted repro | Failed (Phase 8) | `out/phase8_black.log` |
| Root cause | **Not identified** | — |
| Permanent workaround / forced render reset | **Not applied** (by design) | — |

**Code notes (no gameplay this phase):**
- Set names present in `stg110_obj_00.orc` string table: `DestBox`, `IronBox`, `Spring` (DOL: `CObjIronBox`, `CObjSpring`).
- Lower-path Y≈−50 object positions near glass Z≈4583 exist in set floats (e.g. ring-line @ x≈−7779…−7699, y≈−50); exact Dest/Iron instance indices still need a proper SOBJ type decode.
- Host GX path of interest for RCA: `ClearEFB` / EFB copy in `runtime/gx/renderer.cpp` (shader-fail already logs black draws). Next RCA step: count draws + ClearEFB around spring bounce without resetting guest state.

**Limitation:** Efficient scripted stomp→spring not achieved; full exploratory search deferred.

### C — TR1 → Act 2 (`stg120`)

| Claim | Verification | Notes |
|---|---|---|
| `stg120` set data in CPK | Previously verified | `out/cpk_extract/set_dec/stg120_*.orc` |
| Normal progression TR1→stg120 | **Not yet verified** | No useful NAND progress save under `~/.local/share/SonicColorsRecomp`; full Act 1 clear is a long play session — deferred per testing-efficiency rules |

### Regression

- Build OK after adding `BoostPluginTick` override (`linux-clang` → `out/SonicColorsRecomp`).
- Phase 8 timing / Wisp collection evidence unchanged.

### Next (code-first)

1. One manual boost run-forward+B capture with `--boost-debug`, or locate gauge on component `0x19` via code.
2. Black-screen: SOBJ type→instance map for Dest/Iron/Spring; optional draw/ClearEFB counters.
3. Act 2: only after a saved clear or a short end-of-act script — not a fresh opening farm.

---

## 2026-11-19 (Phase 8) — Wisp→boost verification, timing gap, compat matrix

### White Wisp collection (verified)

Normal TR1 glass-walkway route (non-booster), `ReleaseBoxSmall #46`:

| Step | Result | Evidence |
|---|---|---|
| Capsule before hit | Intact red/yellow dome, 3 White Wisps, empty gauge | `out/shots/phase8_capsule_before.png`, `phase8_wisp_0166.png` |
| Interaction | Jump / collide opens capsule | `phase8_wisps_released.png` (0167) |
| Wisps | Three White Wisps release + home to Sonic (auto) | 0167–0127 absorb frames |
| Gauge | Magenta/purple fill appears; circular Color Power slot stays empty (expected for White) | `phase8_gauge_after_collect.png`, `phase8_collect_wisps_b_prompt.png` |
| Tutorial | Native **B** boost tip appears | `phase8_b_prompt.png` |

Input: script logical `B` → host `PAD_B` → WPAD `0x0400` (`--input-debug` in `out/phase8_wisp.log` / `phase8_boost.log`).

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --mute --60fps \
  --input-debug --input-script /tmp/scr_phase8_wisp.txt --shots 15 --shots-start 1800
```

### Boost (partial)

| Check | Result |
|---|---|
| Wii B reaches game after pickup | **Yes** (WPAD `0x0400`; continuous hold logged as single transition) |
| B tutorial after collection | **Yes** |
| Visible high-speed / aura while B held | **Yes** (e.g. `phase8_speed_fx_during_B.png`, `phase8_boost_aura.png`) |
| Clean upright boost + energy drain curve | **Not conclusive** — `DashPanel #1` (~6618,283,4583) sits just past the capsule and forces blue-ball travel that confounds spin vs boost |
| Related symbols | `CPluginStateBoost` `0x80714ba8`; `obj_pixieget` `0x8071721c`; `sn_boost` `0x80714bbc`; `CObjReleaseBoxSmall` xrefs `0x802d5e04` / `0x802d6ed0` |

Cyan Laser / Yellow Drill: not exercised.

### Black-screen bug

| Item | Status |
|---|---|
| Manual repro (prior) | Stomp Dest/Iron box → spring on **lower** path (Y≈−50); world blacks, HUD stays; restart recovers |
| Scripted repro this phase | **Not reproduced** (`out/phase8_black.log`, `out/shots/phase8blk_*.png`) |
| Root cause | **Not identified** — no permanent workaround applied |
| Likely objects | `DestBox` / `IronBox` + nearby `Spring` under glass walkway in `stg110_obj_00.orc` |

### Frame timing — secondary ~60 Hz limiter

`--perf` now reports **in-wait** vs **gap** (time from `FrameLoopWait` exit → next enter):

| Mode | Guest Hz | VI-wait | Spins | In-wait | Gap |
|---|---:|---:|---:|---:|---:|
| `--frame-rate 0` (in-stage) | ~59.7–60 | **0.00 ms** | **0** | ~5–6 ms | **~10–12 ms** |
| Period | ≈16.7 ms | | | in-wait + gap | |

Interpretation: VI EQ-spin remains fully bypassed. The ~16.7 ms period is filled by work/time **outside** that spin (main-loop update + `FrameLoopWait` post-spin path). Other `VIGetRetraceCount` call sites are OSFatal-style (wait 1–2 fields), not the frame loop. Present still ~0.01 ms. **Do not** strip further waits without proving idle time inside the gap; original 60 Hz pacing remains the correct default. 30 / 60 / uncapped modes unchanged.

Log: `out/phase8_perf_uncapped.log`. Code: `runtime/perf.cpp`.

### Full-game preparation

Created `docs/GAME_COMPATIBILITY.md` with stage inventory (`stg110`–`stg160`, `stg190`, `stg210`/`220`, later DOL IDs, Game Land `stgA10`+), Wisp/menu/save rows, and TR1 statuses. Only TR1 Act 1 exercised beyond boot/menus.

### Regression

- Build OK (`linux-clang` → `out/SonicColorsRecomp`).
- Boot → TR1 → movement / rings / HUD / audio path exercised in scripted runs.
- `--60fps` and `--frame-rate 0` still functional; default 30 not re-broken.

### Next milestone

1. Clean boost drain proof **before** `DashPanel #1` (hold B, no A, stop short of dash panels).  
2. RCA black-screen on lower-path box→spring (manual capture + GX/EFB diagnostics).  
3. Progress TR1 → `stg120` via normal play; expand compatibility matrix.

---

## 2026-11-19 (Phase 7) — White Wisp location + true VI-wait bypass

### White Wisp (Priority A — located)

Tropical Resort Act 1 (`stg110`) **does** contain White Wisp pickups.

| Field | Value |
|---|---|
| Stage | Tropical Resort Act 1 (`stg110`) |
| Object | `ReleaseBoxSmall` in `set/stg110_obj_00.orc` (CRILAYLA → BINA/SOBJ) |
| First main-path capsule | Object index **#46**, world ≈ **(-7299.9, 342.0, 4583.9)** |
| Landmark | Glass walkway **before** the WELCOME arch; ~500 units past first `PointMarker` (-7797.7, 342.6, 4583.9) |
| Appearance | Red/yellow glass capsule with **three White Wisps** inside |
| Access | Run forward from spawn along the ring line; jump into the capsule (forced dash/booster paths can skip collision) |
| Expected gauge | Capsule should fill the boost gauge; empty circular Wisp slot is normal for White (boost energy, not a Color Power) |
| Activation | **Hold** Wii **B** (Nunchuk) / script logical `B` → WPAD `0x0400`. Config: `wii_b = X` maps physical X → Wii B for manual play |

Also present: 11× `ReleaseBoxSmall` + 1× `ReleaseBoxBig` in `stg110_obj_00.orc`. `MultiBoostBox` / `BoostBox` appear in Game Land stages (`stgA10`, `stgF*`, …), not TR1. Colored `cmn_obj_rbox_pixy_*` models exist for Laser/Drill/etc.; White uses standard `cmn_obj_rboxS_SD`.

**Evidence:** `out/shots/phase7_wisp_0029.png` (capsule on glass path), `out/shots/phase7c_0045.png` (native **B** boost tutorial prompt after that area), `out/cpk_extract/set_dec/stg110_obj_00.orc`.

### White Wisp collection / boost (Priority B–C — superseded by Phase 8)

| Check | Result |
|---|---|
| Capsule rendered in gameplay | **Yes** (shot 0029) |
| Scripted path consistently collides | **Yes** in Phase 8 (jump into #46 on glass path) |
| Gauge fill after pickup | **Yes** (Phase 8) |
| Boost state (speed/FX/audio/drain) | **Partial** — see Phase 8 (DashPanel confounds clean drain proof) |
| Related symbols | `CPluginStateBoost` @ string `0x80714ba8`; `obj_pixieget` @ `0x8071721c`; `sn_boost` refs `0x80088b00`+; `CObjReleaseBoxSmallInfo` |

OSReport filter logs lines containing pixie/wisp/boost/phantom (`runtime/kernel/hle_os.cpp`).

```bash
# Reproduce: boot to TR1, approach glass path before WELCOME, jump into capsule, hold B
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --60fps \
  --input-debug --input-script /tmp/scr_phase7_wisp3.txt --shots 45 --shots-start 550
```

### Uncapped mode (Priority D)

**VI EQ-spin is genuinely bypassed** when `--frame-rate 0`:

- `FrameLoopWait` sets a thread-local flag; `VIGetRetraceCount` returns `0x7FFFFFFF` during that wait so the `blt` spin exits immediately (`runtime/perf.cpp`, `runtime/kernel/hle_os.cpp`).
- Float frame delta still published as **1 field** via `fn_803E19B0(1)` so sim dt stays defined (fast-forward at fixed 1/60s steps if guest ever exceeds 60 Hz).
- `--60fps` / default 30 unchanged (still real VI waits).

Measured (scale=3, same boot→TR1 move script, `--perf`):

| Mode | Guest updates/s | VI-wait | Spins/guest | XFB/s | Present/s | Avg guest frame |
|---|---:|---:|---:|---:|---:|---:|
| Default 30 | ~29.9 | ~9.8 ms | ~6500 | ~30 | ~30 | ~25–31 ms |
| `--60fps` | ~59.6 | ~11 ms | ~183 | ~60 | ~60 | ~7–15 ms |
| `--frame-rate 0` | ~59.7 | **0.00 ms** | **0** | ~60 | ~60 | ~7–15 ms |

**Interpretation:** VI blocking is gone, but guest∶XFB∶present remain ~60∶60∶60. Another limiter (likely outside `FrameLoopWait` — scheduler/audio/other VI users, or simply guest work + present coupling) still holds the loop near 60 Hz on this host. Not GPU-bound (present ~0.01 ms). True “hundreds of Hz” uncapped needs that secondary limiter identified; do not treat 60 Hz guest as proof of remaining VI wait.

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --perf --frame-rate 0
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --perf --60fps
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --perf
```

Logs: `out/phase7_perf_30.log`, `out/phase7_perf_60.log`, `out/phase7_perf_uncapped3.log`.

### Cyan / Yellow

Not exercised. TR1 has `EventLaserCancelCollision` / phantom-related collisions (Laser later in act per guides). Yellow Drill is later worlds. `MultiPhantomBox` not in TR1 sets.

### Regression

- Boot / TR1 / movement / rings / pause path unchanged in scripted runs.
- `--60fps` still shows non-zero VI-wait (~15 ms, ~185 spins).
- Default 30 still ~30 Hz.

### Next milestone

1. **Reliable White Wisp collision** on the glass-path capsule (avoid booster skip) → confirm gauge fill → hold-B boost (speed/FX/audio/drain).  
2. Identify the **post-VI ~60 Hz limiter** if higher uncapped rates are required.  
3. Cyan Laser when a TR1/TR2 spawn is deliberately routed.  
4. HOME RSO still deferred.

---

## 2026-11-19 (Phase 6) — Stage performance (true 60 FPS) + Wisp/boost probe

### Bottleneck (measured with `--perf`)

Four rates instrumented separately (`runtime/perf.cpp`, `--perf`):

| Rate | Source |
|---|---|
| Guest simulation | `FrameLoopWait` (`fn_800126EC`) enter/exit |
| VI | `g_viFieldCount` |
| Host XFB / present | EFB→XFB copy + `Video_Present` |
| GX sync wait | `GX_SyncGPU` block time |

**Root cause:** the game caches the VI wait interval after the first calc (`obj+0xC`). Our `FrameRateWait` patch at `80012774` only ran on recalc, so in-stage stayed on wait≈2 (or a stale wait≈1) even with `--60fps` / uncapped. Guest∶XFB∶present stayed **1∶1∶1**. Present itself is ~0.01 ms; GX-sync is ~0 in stage. Not a GPU/Present limiter.

**Fix:** every `FrameLoopWait` call republishes the configured interval via `Write32(obj+8)` + `ppc::fn_803E19B0` (`runtime/perf.cpp`). Default 30 still wait 2; `--60fps` wait 1.

### Performance table (scale=1, TR1 late / in-stage)

| Mode | Guest sim | VI rate | Host present | Notes |
|---|---:|---:|---:|---|
| Default (`frame_rate=30`) | ~30 Hz | ~60 Hz | ~30 Hz | Accurate original pacing (menus also ~30 now) |
| `--60fps` | ~60 Hz | ~60 Hz | **~60 Hz** | **Real 60 FPS achieved** (also verified at scale=3) |
| `--frame-rate 0` | ~60 Hz | ~60 Hz | ~60 Hz | Superseded by Phase 7: VI EQ-spin bypassed (0 ms wait); guest still ~60 Hz from a secondary limiter |

Present time ≪ 0.1 ms; stage guest work sits outside the VI spin and clears 60 Hz on this host (RTX 3080 + Ryzen).

### Wisp / boost

- DOL strings confirm `CPluginStateBoost`, `sn_boost`, `AIRBOOST`, `pixy_*`, `obj_laser` / `CPosturePhantomLaser` / `CPosturePhantomDrill`.
- Scripted TR1 run with repeated host **B** → WPAD **B** (`0x0400`) verified in `out/phase6_boost.log`.
- Boost gauge stayed empty; no White Wisp pickup on the scripted path → boost correctly unavailable (not a mapping failure).
- Cyan Laser / Yellow Drill: assets present in DOL; **not** exercised in TR1 this phase.
- Next: locate White Wisp capsules in TR1 (or a known early stage) and confirm gauge fill → boost state.

### Regression

- Build OK; TR1 playable at 30 and 60.
- Pause still works (Phase 5).
- Evidence: `out/phase6_perf_*.log`, `out/phase6_boost.log`, `out/shots/phase6_60fps_tr1_*.png`

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --perf --60fps
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --perf   # default 30
```

### Next milestone

1. White Wisp collection → boost activation in normal gameplay.  
2. Optional: true uncapped (skip VI EQ-spin without breaking guest VI counters).  
3. Cyan Laser / Yellow Drill when a legitimate stage spawn is available.  
4. HOME RSO (`hbm_moduleD.rso` + MEM2 dump).

---

## 2026-11-19 (Phase 5) — Frame pacing, pause, DPD, Wisps/boost status

### Frame rate

**Timing architecture (SNCP8P):** fixed VI-retrace pacing in `fn_800126EC` (`80012774`). Steady-state was `addi r0,2` (wait 2 fields ≈ 30 Hz on 60 Hz VI). Catch-up path keeps wait 1. Interval is stored at frame-object `+0x8` and published through `fn_803E19B0` → SDA global read by `fn_803E19E4` in the VI spin.

**Config (default remains original 30):**
| Value | Meaning |
|---|---|
| `frame_rate = 30` (default) | Wait 2 VI fields |
| `frame_rate = 60` / `--60fps` | **Superseded 2026-11-20:** wait=2 (spring-safe). Wait=1 only with `--guest-60`. |
| `frame_rate = 0` / `--frame-rate 0` | Wait 0 (uncapped VI spin) |

Patch: `FrameRateWait` in `tools/recomp.py` + `tools/game_addrs.txt` (`code 80012774`); publishes via `fn_803E19B0`. Status overlay: `status: N fps (target 30|60|uncapped), VI …`.

**Measurements (Tropical Resort Act 1):**
| Mode | Menus / boot | In-stage present rate |
|---|---|---|
| target 30 | ~60 fps | ~30 fps |
| target 60 (`--60fps`) | ~60 fps | ~30 fps |
| uncapped (`--frame-rate 0`), scale 1 | ~60 fps | ~30 fps |

In-stage stays ~30 fps even when uncapped at `scale=1` → host performance ceiling for the recompiled stage loop (not only the VI wait). Menus already clear 60. **True 60 Hz simulation/render in-stage is not achieved yet**; the wait option is wired and safe (does not double sim). No separate render-thread interpolation yet.

### Pause and HOME

- **In-game pause:** WPAD **PLUS** / host **START** opens CONTINUE / RESTART / EXIT. Background blur + HUD retained. Resume via CONTINUE works. Evidence: `out/shots/phase5_pause_menu_tr1.png`, `phase5_pause_resumed_tr1.png`; log `out/phase5_pause.log` (PLUS transitions under `--input-debug`).
- **HOME menu / `hbm_moduleD.rso`:** still deferred. Needs MEM2 dump after OSLink (`SCR_DUMPMEM2` / `tools/rso.py`), then optional static recomp. Host Back+Start still synthesises HOME button; RSO UI not linked.

### DPD / pointer / motion

- **`WPADControlDpd` `803A1AE0`:** HLE override; enable/disable logged; completes callback.
- **IR:** `Motion_FillStatus` only emits sensor-bar dots when `WPAD_DpdEnabled(chan)`; no fabricated pointer when DPD off. Mouse / right stick aim when enabled.
- **`WPADIsDpdEnabled`:** not in SNCP8P SEL exports; address unproven → remains `PPC_OVERRIDE_TODO` (body ready).
- Motion (accel / nunchuk) unchanged from prior HLE; no new camera overrides.

### Wisps and boost

- HUD boost gauge + 5 Wisp slots render in TR1; gauge can show a purple charge sliver.
- Scripted **B** during TR1 did not demonstrate a full boost burst (likely needs White Wisp / more gauge — not a missing HLE stub identified yet).
- Cyan Laser / Yellow Drill: not verified; no game-specific runtime stubs added (original DOL logic already present).
- Priority next: gather White Wisps in TR1 and confirm boost activation, then Cyan.

### Build and regression

- Builds: `./scripts/cmake.sh --build build/linux-clang`
- TR1 still playable (move, rings, score, timer, audio, pause).
- Logs: `out/phase5_60fps.log`, `phase5_60_scale1.log`, `phase5_uncapped.log`, `phase5_pause.log`
- Shots: `out/shots/phase5_*.png`

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --verbose
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --60fps
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --frame-rate 0 --scale 1
```

### Next milestone

1. In-stage performance (why present rate caps ~30 even uncapped) so optional 60 Hz is meaningful.  
2. Verify boost after White Wisp pickup; then Cyan Laser.  
3. HOME RSO when a MEM2 dump is available.  
4. Avoid START-mash tip UI softlocks noted in Phase 4.

---

## 2026-11-19 (Phase 4) — First playable level (Tropical Resort Act 1)

### Input (START / PLUS)

- Title **PRESS START** expects GC `PAD_BUTTON_START` (`0x1000`) and/or WPAD **PLUS** (`0x0010`).
- Mappings were already correct in `pad_hle.cpp` / `motion.cpp` (`PAD_START` → both). Phase 3 scripts only sent **A**.
- Verified under `--input-debug`: host `START`, wii `PLUS`, pad `START` fire together; A/B unchanged.
- Stick (`LY`) drives movement via Nunchuk axes in `Motion_FillStatus` and GC stick in `PADRead`.

### Save system

Already implemented via IOS `/dev/fs` → host directory (`runtime/ios/dev_fs.cpp`).

| Guest path | Host |
|---|---|
| `/title/00010000/534e4350/data/` | `<nand>/title/00010000/534e4350/data/` |

Files created by the game: `system.sav`, `game_0.sav`…`game_5.sav`, `banner.bin` (write to `/tmp/` then rename).

Default NAND root is now **`userdata/`** (repo layout when exe is in `out/`); override with `--nand`. Legacy `out/nand` still used if it exists and `userdata/` does not.

Persistence verified across relaunch (existing saves reused; no broken validation).

### Stage loading / gameplay

Normal flow after Start reached **Tropical Resort Act 1**:

1. Wisp loading icon  
2. “ACT 1 / Tropical Resort” card  
3. Stage geometry + HUD (lives, rings, timer, score, boost, red-star slots)  
4. Sonic rendered; timer running  
5. Stick movement + ring collection (rings `0→7`, score `1700` at ~1:16)

No extra DOL overlays required for this stage. DVD/ISFS loads succeeded; GX shader count ~307 in stage. Host present rate ~30 fps in-stage with default `frame_rate=30` (original); boot/UI often nearer 60 VI.

### Evidence

- `out/shots/stage_tropical_resort_act1_loading.png`
- `out/shots/stage_tropical_resort_act1.png`
- `out/shots/gameplay_tr1_start.png`
- `out/shots/gameplay_tr1_moved_rings.png`
- Logs: `out/phase4_start.log`, `out/phase4_gameplay.log`, `out/phase4_persist.log`

```bash
cd /home/volt/Decomp/SonicColorsRecomp
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --verbose
# optional: --nand userdata --input-debug --input-script …
```

Furthest verified: **playable Tropical Resort Act 1** (move + collect rings).

### Next milestone

Improve in-stage feel / completeness: optional 60 fps config, pause/HOME (RSO still deferred), camera/DPD TODOs, boost/Wisps, further acts, any remaining softlocks in empty tip UI boxes when Start is mashed.

---

## 2026-11-19 (Phase 3) — Title screen reached

### Input investigation

Controller “Current Setting” UI shows a **GameCube** pad; the game uses **both** paths:

| API | Address | Role |
|---|---|---|
| WPAD / KPAD (existing HLE) | `803A****` / `KPADReadCore 803B7F70` | Wii Remote + Nunchuk; still used for boot + menus |
| `PADRead` | `80357660` | GC `PADStatus[4]`; called from pad-manager vtable `803C02CC` (data xref `8079F684`) |
| `SIGetType` | `8038AAA0` | PAD type probe (`fn_80357B20`); SI MMIO previously always “no response” |

No `PADRead`/`PADInit` names in decomp symbols — IDs from SIGetType call graph + `PADStatus.err` at `+0xA` (−1/−2) in `803C02CC`.

**Root cause:** SI HLE reported no GC devices, so `PADRead` never delivered buttons while the menu’s current setting was GC. WPAD A alone was insufficient for reliable confirm; empty-input runs can still timeout past some screens, but scripted A via **both** WPAD and PAD advances cleanly (skips intro → title).

**Not** an SDL3 south-button mapping bug: host `PAD_A` → WPAD `0x0800` and GC `0x0100` both fire under `--input-debug`.

### Implementations

- `runtime/input/pad_hle.cpp` — `PPC_OVERRIDE(PADRead)` + `SIGetType` (chan0 = `SI_GC_CONTROLLER 0x09000000`); feeds `Gamepad_Poll`
- `runtime/input/input_debug.cpp` + `--input-debug` — transition logs for host / WPAD / PAD button masks
- `tools/game_addrs.txt` — `PADRead`, `SIGetType`; regenerates `game_addrs.inc` + recomp trampolines
- WPAD/KPAD HLE unchanged and still active

### Testing

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --verbose --mute \
  --input-debug --input-script /tmp/scr_title.txt --shots 90 --shots-start 200
```

- Navigation/confirm: A reaches game (`host A` + `wii A` + `pad A` in `out/phase3_title.log`)
- **Title screen renders:** `SONIC COLOURS` logo, Wisps, Sonic, **PRESS START**, `© SEGA`
- Audio ~60 fps / VI stable; GX ~94 shaders on title; no new fatal warnings (WC24 ENOENT unchanged)
- Title still waits for **Start** (`+` / GC Start); script used A only

### Evidence

- `out/shots/title_screen.png` / `title_screen_press_start.png`
- `out/phase3_title.log` — `PAD: HLE read active`, dual-path A transitions
- Furthest verified: **title attract (PRESS START)** after normal boot → controller setting → (skippable) intro

### Next milestone

Press **Start** on the title screen → save / main menu → load first stage. Likely blockers: Start/Plus mapping under the selected pad type, save/NAND profile, stage DVD reads, any remaining WPAD DPD TODOs for pointer UI.

---

## 2026-11-19 (Phase 2) — First native boot verified

### Build

- `perl-json` was already installed; full `linux-clang` link succeeded after fixing `SCR_MODS_TEST_DATA` in `tools/mods_test/main.cpp` and regenerating vkd3d ExternalProject stamps.
- **Executable:** `out/SonicColorsRecomp` — ELF 64-bit LSB pie, x86-64, ~273 MiB, debug info.
- Runtime libs beside exe: `libSDL3.so.0`, `libdxvk_d3d11.so.0`, `libdxvk_dxgi.so.0` (no unresolved `ldd` deps).
- `ppcrecomp` + runtime + installer all link.

### WPAD

Identified from SEL + KPAD→WPAD call graph (not BKR addresses):

| Symbol | Address | Evidence |
|---|---|---|
| `WPADInit` | `803A0060` | Sole KPAD caller; init-flag + setup |
| `WPADGetStatus` | `803A0110` | KPAD compares result to `3` (SETUP) |
| `WPADGetAccGravityUnit` | `803A1220` | KPAD `(chan,0,&acc)` after Read |
| `WPADSetSamplingCallback` | `803A1480` | Between Probe and SetConnect |
| `WPADGetDataFormat` | `803A1600` | KPAD stores into `status+0x40` after Read |
| `WPADSetDataFormat` | `803A1650` | Follows GetDataFormat |
| `WPADRead` | `803A1D80` | KPAD `(chan, status*)` then memcpy body |
| `KPADReadCore` | `803B7F70` | `fn_803B7F70`; GetStatus/Probe poll loop |

SEL-confirmed overrides (unchanged): Probe, Disconnect, Connect/Extension callbacks, motor/speaker, stream, radio.

**Still `PPC_OVERRIDE_TODO`:** `WPADIsDpdEnabled`, `WPADControlDpd`, `WPADControlExtension` (addresses not yet proven).

Input backend: existing BKR SDL3 gamepad → WPAD/KPAD HLE. Controller 0 connects at boot (`data format 4`).

### First launch (verified)

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --verbose --mute
```

Furthest **verified** milestone: **interactive pre-title menu UI** (controller “Current Setting” screen), after:

1. Native process start  
2. Entry `0x80004050`  
3. Full RVL OS / IOS / NAND / DVD init (OSReport banners)  
4. Engine heaps + NW4R G3D/EF  
5. GX display (60 fps, shaders, textures) + AX/DSP audio HLE  
6. Wii safety warning → SEGA logo → controller config menu  

Screenshots: `out/shots/shot_0001.png` (safety), `shot_0005.png` (SEGA), `shot_0010.png` (controller menu).

Not yet verified: title-screen confirm, menu navigation with input, level load, gameplay.

Non-fatal noise: WiiConnect24 `/dev/net/kd/*` ENOENT (expected without network IOS).

### RSO (`hbm_moduleD.rso`)

- HOME Button Menu module (`nw4hbm` / `homebutton::*`); ~882 KiB; 7137 export-table entries (mostly DWARF).
- DOL strings reference `hbm_module.rso` / `/hbm.arc`.
- Decomp exposes `RSOStaticLocateObject`; not required for the boot path observed so far (safety → SEGA → controller menu without linking it).
- **Approach:** keep optional; recompile from MEM2 dump when HOME menu is needed (same as BKR `hbm_part.rso`).

### Next concrete task

Drive past the controller-confirm prompt into the title screen / opening:

1. Ensure Classic/GCN pad HLE or WPAD buttons map so “CONFIRM (A)” advances (`runtime/input/gamepad.cpp`, `wpad_hle.cpp`, possibly PAD SI path).
2. Optionally identify the three remaining WPAD TODOs.
3. Re-test with `--input-script` / manual pad; capture title-screen shot.

```bash
cd /home/volt/Decomp/SonicColorsRecomp
./scripts/cmake.sh --build --preset linux-clang -j$(nproc)
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --verbose
```

---

## 2026-11-19 — Milestone 1–3 (prior session)

- Project established; BlackKnightRecomp foundation; DOL extracted/SHA-1 verified.
- 38,506 functions → 313 C++ TUs; DolRecomp 0 unknown opcodes.
- Full exe link was blocked on `perl-json` (resolved this session).
