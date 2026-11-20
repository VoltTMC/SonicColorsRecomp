# Architecture decision — Sonic Colors static recompilation

Decision date: 2026-11-19.

## Candidates

| Foundation | What it is | License | Linux | REL/RSO | Runtime nature |
|---|---|---|---|---|---|
| **BlackKnightRecomp** | Complete static recomp of Sonic and the Black Knight (Wii): Python PPC→C++ tools + full Wii runtime (OS fibers, GX/D3D11+dxvk, AX HLE, IOS, SDL3) | GPL-2.0-only | Yes (native) | RSO (HOME menu) | Native HLE runtime (Dolphin as reference only) |
| **DolRecomp** | Generic GC/Wii/Wii U static recompiler (C + optional LLVM backends) | See `external/DolRecomp/LICENSE` | Yes | REL folders, DOL, RPX | CPU translation only |
| **ModernGekko-Template** | Glue: DolRecomp + ModernGekko runtime | Template; ModernGekko GPL-3.0+ (Dolphin-derived) | Yes | Via DolRecomp | Dolphin-derived core hosts recompiled modules |

## Sonic Colors executable facts (SNCP8P)

- Disc: PAL `SNCP8P`, IOS 56, ~53 files (small FST; content mostly in `.cpk`).
- **Main DOL only for gameplay code** — no REL modules on disc.
- HOME menu module: `files/sonic2010/hbm_moduleD.rso` (same pattern as Black Knight’s `hbm_part.rso`).
- Symbol SELs: `files/sonic2010.sel` (+ RVL_Default*.sel variants).
- DOL SHA-1: `8b606ee1ff822bf8288e492646cf94af374a078e`
- Entry `0x80004050` (`__start`); text ≈ 7.0 MiB at `0x800113C0`; BSS `0x807C6500`+`0x137720`.
- Compiler: CodeWarrior Wii/1.0 (same family as Black Knight).

## Decision

**Primary foundation: BlackKnightRecomp architecture** (tools + Wii runtime), adapted for SNCP8P.

Reasons:

1. **Same product shape** — Sonic Team Wii title, CW-built DOL, SEL exports, HOME-menu RSO, no RELs. Relocatable-module complexity that favours DolRecomp is not required for the main path.
2. **Proven end-to-end Wii runtime** — guest 4 GiB mapping, OS context→host fibers, GX TEV, AX HLE, IOS/DI/FS, Linux SDL3/dxvk already exist and have reached gameplay on a sibling title.
3. **Matches the project goal** — native static recompilation, not “Dolphin with a recompiled CPU module” (ModernGekko’s model).
4. **Licensing** — GPL-2.0-only is compatible with retaining BKR-derived runtime pieces and Dolphin-inspired behaviour under GPL-2.0-or-later attribution (see `docs/dolphin-dependency.md`).

**Secondary tool: DolRecomp** stays under `external/` for:

- Independent instruction-coverage cross-checks (SNCP8P: 0 unknown opcodes in DolRecomp’s decode pass).
- Future REL experiments / alternate backends.
- Not used as the game runtime host.

**Not selected as primary: ModernGekko-Template** — convenient bootstrap, but the runtime is Dolphin-derived (conflicts with “do not substitute Dolphin as the runtime”) and pulls GPL-3.0+.

## How this repo is organised

- `external/BlackKnightRecomp` — pinned upstream reference (not the build root).
- `tools/` — adapted PPC analysis/recomp pipeline (SEL bases for Colors, decomp `symbols.txt` seeding).
- `runtime/` — Wii runtime seeded from BKR; SNCP8P release tables and address map; Black Knight–specific probes stripped or disabled.
- `generated/` — SNCP8P PPC→C++ (gitignored; regenerate with `python tools/build_generated.py`).
- `game/SNCP8P/` — extracted disc (gitignored).
- `out/dolrecomp_sncp8p/` — optional DolRecomp C output for comparison (gitignored).

## Runtime layout (current)

- Executable: `out/SonicColorsRecomp` (SDL3 + DXVK on Linux).
- User data: exe dir when writable; else `$XDG_DATA_HOME/SonicColorsRecomp` (AppImage).
- NAND saves: host FS under the user dir (`runtime/ios/dev_fs.cpp`, atomic write-then-rename).
- Music mods: file-based CPK host overrides (`Disc_RegisterHostOverride`); launcher Music Library + `tools/music_asset`. No host MixInto / AX mute overlay.
- Packaging: `tools/package_linux.sh` → AppImage **without** game data.

## Known gaps (post-boot)

1. HOME menu RSO still not recompiled (optional polish).
2. Full-game stage/ability matrix incomplete — see `docs/GAME_COMPATIBILITY.md`.
3. Intermittent black-world after stomp (TR acts) — **known deferred** (H7); Phase 20 experiments reverted. Optional diag: `SCR_GX_XFB_DIAG=1`.
4. Some legacy `BKR_*` debug env names remain (UI/test aids also accept `SCR_*`).
5. Cursor AppImage can break stock `CMAKE_ROOT`; use `./scripts/cmake.sh`.

Quality inventory: `docs/QUALITY_AUDIT.md`.
