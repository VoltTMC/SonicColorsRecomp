# Phase 19 — Quality audit

Authoritative inventory of defects and polish work for SonicColorsRecomp.
Status values: `FIXED`, `OPEN`, `WONTFIX`, `DEFERRED`.

Last updated: 2026-11-19 (Phase 19).

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
| H7 | Black world after stomp→spring (HUD remains) | GAME_COMPATIBILITY | **OPEN** | Intermittent; no RCA. Diagnostics: `SCR_GX_XFB_DIAG=1` logs zero-draw XFB presents. No framebuffer-reset workaround |
| H8 | In-game music replacement audibility (TR1) still user-side unverified this phase | Prior phases | **OPEN** | Pipeline/align-8 fixed earlier; not re-audited in P19 |

---

## Medium

| ID | Issue | Evidence | Status | Notes |
|---|---|---|---|---|
| M1 | Config.ini header still BKR comment | config.cpp | **FIXED** | |
| M2 | Music Library cache always under cwd | music_ui.cpp | **FIXED** | Fallback to user dir |
| M3 | HTTP User-Agent BKR | linux/win32 http | **FIXED** | |
| M4 | AppImage / install path discovery incomplete for music catalog | default dir | **FIXED** | Prefer `catalog.json` existence |
| M5 | First-run / release docs incomplete | README | **PART** | Quick start updated; full packaging still needs CI shader keys |
| M6 | Win32 window title still Black Knight | win32/window.cpp | **DEFERRED** | Linux is priority; class name left for ABI stability |
| M7 | No automated test for NAND flush failure path | tests | **OPEN** | Unit harness would need FS mock |

---

## Low

| ID | Issue | Evidence | Status | Notes |
|---|---|---|---|---|
| L1 | Placeholder AppImage SVG icon | new asset | **FIXED** | Simple brand mark; replace with final art later |
| L2 | Minor launcher page spacing inconsistencies | visual audit | **DEFERRED** | Music Library already polished in P17–18 |
| L3 | Residual BKR identifiers in comments / Win32 class names | grep | **DEFERRED** | Non-user-facing |

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
| 30 / 60 FPS / uncapped | Preserved in config |
| Black-screen | OPEN (H7) + optional XFB diag |

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

1. **H7** — Black-screen RCA with `SCR_GX_XFB_DIAG` + GX capture (code/diagnostics first).
2. **H8** — Confirm file-based music replacement audible in TR1 with prepared CPK.
3. **M7** — NAND flush failure regression test.
4. Full-game matrix beyond TR1 (compatibility doc).
5. Optional: final AppImage art, Win32 branding, CI packaging.

Do not mark H7/H8 fixed without evidence.
