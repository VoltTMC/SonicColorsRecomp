# External dependency pins

Recorded 2026-11-19. Paths under `external/` are reference checkouts (not merged into the
main source tree indiscriminately).

| Project | Path | Commit | License | Role |
|---|---|---|---|---|
| BlackKnightRecomp | `external/BlackKnightRecomp` | `497c6526bc068c515937481a61dab83538f4f166` (2026-11-16) | GPL-2.0-only | Primary foundation: PPC→C++ tools, Wii runtime (GX/AX/IOS/OS), Linux backend |
| DolRecomp | `external/DolRecomp` | `02e27aca1c408cfcb2a4079a3a5b5ab3ae39d340` (2026-10-10) | see LICENSE | Secondary recompiler reference; REL/C/LLVM backends; instruction-coverage cross-check |
| ModernGekko-Template | `external/ModernGekko-Template` | `c2553a99584b2045dbe46a70e1570994dae8beef` (2026-09-16) | template; pulls GPL-3.0 ModernGekko | Evaluated; not selected as primary runtime (Dolphin-derived core) |
| DolRecomp tomlc17 | `external/DolRecomp/extern/tomlc17` | `8d3766ddafa6515417f4885ae6878ef69fac401e` | see submodule | DolRecomp config parser |

## Build-time downloads (from BlackKnightRecomp CMake, when full runtime builds)

Pinned in upstream CMakeLists / our adapted CMakeLists:

- nod `v2.0.0-alpha.12` (MIT) — disc install
- SDL3 `3.4.18` (zlib) — Linux window/input/audio (system SDL3 3.4.20 also acceptable when linked)
- dxvk-native `v3.1.1` (zlib / LGPL headers) — D3D11 on Vulkan
- vkd3d-shader (Wine vkd3d) — HLSL→DXBC

## Linux → Windows cross (MinGW-w64)

See `docs/windows-cross.md`. Preset `windows-mingw` uses `x86_64-w64-mingw32-g++`,
outputs to `out-windows/`, and packages with `tools/package_windows_cross.sh`. MinGW links
the prebuilt `nod.dll` (not MSVC `nod_static.lib`).

## Local references (not vendored)

- Decompilation (read-only): `/home/volt/Decomp/sonic2010-sncp8p` — symbols, splits, DOL hash
- Original ISO (never modified): `/home/volt/Decomp/sonic2010-sncp8p/orig/SNCP8P/Sonic2010 [SNCP8P].iso` (also `/home/volt/Documents/Sonic2010 [SNCP8P].iso`)
- DOL SHA-1: `8b606ee1ff822bf8288e492646cf94af374a078e`
