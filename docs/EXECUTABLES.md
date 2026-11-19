# SNCP8P executable inventory

Source disc: `Sonic2010 [SNCP8P].iso` (never modified).
Verified extraction: `game/SNCP8P/` via `wit EXTRACT`.
DOL SHA-1 matches decomp `config/SNCP8P/config.yml`: `8b606ee1ff822bf8288e492646cf94af374a078e`.

## Boot / system

| Path | Role | Size |
|---|---|---|
| `sys/boot.bin` | Disc header / game ID `SNCP8P` | 1.1K |
| `sys/bi2.bin` | BI2 | 8.0K |
| `sys/apploader.img` | Apploader | 247K |
| `sys/fst.bin` | File system table | — |
| `sys/main.dol` | **Primary Broadway executable** | 7.9M |

## Main DOL layout

| Section | File off | Guest addr | Size |
|---|---|---|---|
| text0 (`.init`) | `0x100` | `0x80004000` | `0x2720` |
| text1 (`.text`) | `0x2820` | `0x800113C0` | `0x701240` |
| data0–7 | (see DOL header) | `0x80006720` … `0x808F45C0` | various |
| BSS | — | `0x807C6500` | `0x137720` |
| Entry | — | `0x80004050` (`__start`) | — |

SEL section bases (for `sonic2010.sel` export values):

- sec 1 → `0x80004000`
- sec 2 → `0x800113C0`

## Dynamically loaded modules

| Path | Role |
|---|---|
| `files/sonic2010/hbm_moduleD.rso` | Wii HOME menu module (linked at runtime into MEM2) |
| *(none)* | **No `.rel` modules** on this disc |

## Symbol tables

| Path | Notes |
|---|---|
| `files/sonic2010.sel` | 216 exports (SDK/OS/GX/WPAD names) |
| `files/sonic2010/RVL_Default{D,DO,P}.sel` | Variant SELs (different bases; not used for main DOL) |

## Analysis results (this project)

- Functions discovered: **38,506** (38,846 known seeds from decomp+SEL; 199 jump tables).
- Recompiled to C++: **38,506** functions → **313** source files under `generated/` (~85 MiB).
- DolRecomp cross-check: **0 unknown** opcodes over 1,838,680 decoded instructions (Gekko profile; titles DB warning only).

## Runtime interfaces expected

Revolution OS (threads, alarms, interrupts), VI, GX FIFO, AX/DSP, IOS (`/dev/di`, `/dev/fs`, `/dev/es`, …), WPAD/KPAD, DVD/CPK file access via disc FS. Hedgehog engine + CRIware audio likely inside the DOL (names visible in decomp symbols / strings); no separate REL for them.
