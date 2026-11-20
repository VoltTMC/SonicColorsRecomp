# Phase 19 — Quality audit

Authoritative inventory of defects and polish work for SonicColorsRecomp.
Status values: `FIXED`, `OPEN`, `WONTFIX`, `DEFERRED`.

Last updated: 2026-11-20 (continuous improvement; spiral gates removed; stick deadzone).

Baseline: Phase 19 commit `72ae9de`. Phase 20 commits (`388ee48`…`de7a72e`) and WIP are preserved on `backup/pre-stability-recovery-de7a72e` and `stash@{0}` (“WIP black-screen investigation…”).

---

## Critical

| ID | Issue | Evidence | Status | Notes |
|---|---|---|---|---|
| C1 | NAND `Flush()` cleared `m_dirty` after a failed `rename`, so a later `Close` would skip retry and leave the previous on-disk save | Code review `runtime/ios/dev_fs.cpp` | **FIXED** | Keep dirty on write/rename failure; create parent dirs; remove temp on failure |
| C2 | Destructive overwrite of user saves | Code review | N/A | Host NAND under user dir only; write-then-rename preserved |

---

## High

| ID | Issue | Evidence | Status | Notes |
|---|---|---|---|---|
| H1 | `.gitignore` `/mods/` hid `mods/music/catalog.json` from VCS | Git status | **FIXED** | Allow `mods/music/` + catalog/README; ignore tracks/manifest |
| H2 | README status still “gameplay next” | README vs PROGRESS | **FIXED** | Updated to reflect TR1 + Music Library |
| H3 | Launcher with `BKR_UI_SHOT` and no window returned `true` → fell into the game | `launcher.cpp` | **FIXED** | Abort (`false`) when shot env is set |
| H4 | Music tool / cache paths fragile when cwd ≠ repo root | `music_mod.cpp`, `music_ui.cpp` | **FIXED** | Walk exe/cwd parents; `SCR_MUSIC_ASSET`; writable fallback under `Platform_UserDir` |
| H5 | Packaging script still BlackKnight-branded; missing `data/linux` | `tools/package_linux.sh` | **FIXED** | Renamed to SonicColorsRecomp; desktop/svg; optional shadercache |
| H6 | User-facing crash / fatal dialogs said “BlackKnightRecomp” | crash/log/install_ui | **FIXED** | Sonic Colors branding |
| H7 | Black world after stomp (TR1/TR3; HUD remains) + post-black soft-lock | GAME_COMPATIBILITY | **DEFERRED** | **Known, unresolved, intentionally deferred.** Phase 20 RCA/fix attempts (XFB latch, EFB ramHash, copy-clear Z, PE sync, guest-60 gate, indexed-XF expand, NaN skip) **reverted** — they destabilised the build. Do not resume without a separate, scoped plan. Phase 19 optional diag only: `SCR_GX_XFB_DIAG=1` (zero-draw XFB log). |
| H8 | In-game music replacement audibility (TR1) | Prior phases + smoke | **PART** | `tools/music_pipeline_smoke.sh` PASS (TR1 prepare+proof). In-game listen still unverified |

---

## Medium

| ID | Issue | Evidence | Status | Notes |
|---|---|---|---|---|
| M1 | Config.ini header still BKR comment | config.cpp | **FIXED** | |
| M2 | Music Library cache always under cwd | music_ui.cpp | **FIXED** | Fallback to user dir |
| M3 | HTTP User-Agent BKR | linux/win32 http | **FIXED** | |
| M4 | AppImage / install path discovery incomplete for music catalog | default dir | **FIXED** | Prefer `catalog.json` existence |
| M5 | First-run / release docs incomplete | README | **PART** | Quick start updated; full packaging still needs CI shader keys |
| M6 | Win32 window title still Black Knight | win32/window.cpp | **FIXED** | Title + class → `SonicColorsRecomp` / “Sonic Colors - Recompiled” |
| M8 | GameBanana browser used Black Knight game id 6511 | gamebanana.h | **FIXED** | → Sonic Colors **6243** |
| M7 | No automated test for NAND flush failure path | tests | **FIXED** | `Fs_WriteFileAtomic` + `scr_nand_fs_test` (create/replace/read-only parent/rename-onto-dir) |

---

## Low

| ID | Issue | Evidence | Status | Notes |
|---|---|---|---|---|
| L1 | Placeholder AppImage SVG icon | new asset | **FIXED** | Simple brand mark; replace with final art later |
| L2 | Minor launcher page spacing inconsistencies | visual audit | **DEFERRED** | Music Library already polished in P17–18 |
| L3 | Residual BKR identifiers in comments / debug env names | grep | **FIXED** | All getenv sites prefer `SCR_*` with `BKR_*` fallback |
| M9 | No save backup / restore UX | settings System | **FIXED** | Open folder + backup + restore latest; `scr_nand_backup_test` |

---

## Launcher (Phase 17–18 baseline, P19 verification)

| Area | Result |
|---|---|
| Resize / DPI / blur | Per-frame swapchain sync (P18). P19: no regression change except shot abort |
| Min window | 800×560 |
| Music catalog | 114 assets via `catalog.json` |
| Preview / export / prepare | Present; unit tests cover pipeline |
| Settings persistence | Config_Save + Platform_ReplaceFile |
| Navigation | Unchanged; no new depth |

---

## Game runtime

| Area | Result |
|---|---|
| Boot → TR1 | Previously verified; not re-played this phase |
| Saves | Atomic flush hardened (C1) |
| Audio / input | No P19 behavioural change |
| 30 / 60 FPS / uncapped | 30 = wait=2; 60 / `--guest-60` = wait=1 + spring gate; uncapped + optional `host_fps_cap` |
| Black-screen (H7) | **DEFERRED** — accepted known bug; Phase 20 experiments reverted |
| AP1 spiral (stg510) | **DEFERRED** — experimental gates removed; do not resume |

---

## Engineering

| Area | Result |
|---|---|
| Build | Incremental `linux-clang` after P19 edits |
| Music unit tests | `tools/music_asset_pkg` suite |
| Generated PPC | Untouched |
| Matching decomp | Untouched |

---

## Remaining priority order

1. **H8** — Prepare path verified (`music_pipeline_smoke.sh`); still need in-game TR1 listen with prepared CPK.
2. Linux/Windows feature parity — keep `docs/WINDOWS_FEATURE_PARITY.md` honest; expand `[native]` coverage.
3. Launcher polish, performance, and PC QoL (deadzone done; more input/audio UX).
4. Full-game matrix beyond TR1 (compatibility doc).
5. Optional: final AppImage art, CI packaging.
6. **H7** / **AP1 spiral** — deferred; not active investigation targets.

Do not mark H8 fixed without evidence. Do not resume H7 or AP1 spiral without an explicit new plan.
