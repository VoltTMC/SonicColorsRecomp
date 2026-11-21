# NAND / save audit (A2g-StageCoverage)

Read-only audit of host NAND + game save paths used by Sonic Colors Recomp.
Original decompilation (`sonic2010-sncp8p`) was consulted for title-data layout only; it was not modified.

## 1. NAND root selection

| Priority | Mechanism | Notes |
|----------|-----------|-------|
| 1 | `--nand DIR` | CLI override for this process |
| 2 | `SCR_TEST_NAND_DIR` | Env override when `--nand` absent |
| 3 | Default | `nand_paths::DefaultRoot()` → `../userdata` next to `out/`, else `<exe>/nand` |

Logged at boot: `NAND root: <path> (ISOLATED test override …)` or `(default / user path)`.
Auto save-backup on launch is **skipped** when an override is active so tests never touch backup trees under the user NAND.

## 2. ISFS → host path

`runtime/ios/dev_fs.cpp`: guest NAND paths (`/title/…`, `/shared2/…`, …) strip the leading `/` and resolve under `IOS_NandRoot()`.
`NAND_InitSystemFiles()` creates `shared2/sys`, title data dir, `sys`, `tmp`, `meta`, regenerates `SYSCONF`, and seeds `setting.txt` if missing.

## 3. Save files

Title data (Europe `SNCP8P`): `title/00010000/534e4350/data/`

| File | Role |
|------|------|
| `game_0.sav` … `game_5.sav` | Per-slot game progress (114688 bytes observed) |
| `system.sav` | System / options blob |
| `banner.bin` | Banner |

Creates/writes go through the same ISFS file HLE (`NandFile` load/flush). Flush issues host writes; optional autosave backup hooks debounce after NAND flush when enabled in config (not for `--nand` test runs).

## 4–8. Presence, validity, progression

The recomp does **not** invent checksums or patch progression flags. The unmodified guest validates and interprets save contents.
A save that previously reached Tropical Resort (verified in-tree `userdata/…/game_0.sav`) loads and allows TR1 entry through normal menus.
Title/menu can idle without a save; stage select for TR1 needs a slot with that progression unlocked.

## 9–10. Clean NAND / persistence

A clean/empty NAND starts with no `game_*.sav`; the game’s new-game flow creates them via ISFS.
Saves persist across restarts because they are ordinary host files under the active NAND root.

## Isolated test fixture

```bash
./tools/setup_a2g_test_nand.sh
# Copies SCR_A2G_SAVE_SRC (default: userdata/…/data) → out/test_nand/ (gitignored)
# Never writes userdata/
```

Reset: re-run the setup script (rm+copy). Verify isolation with `sha256sum` of `userdata/…/game_0.sav` + `system.sav` before/after a `--nand out/test_nand` run.

**Fixture method:** copy of a verified real save that already unlocks Tropical Resort — not a hand-crafted flag dump.
