# A3x — Graphics rendering regression investigation

**Date:** 2026-11-23  
**Goal:** Restore correct original-frame rendering (title, menus, Tropical Resort Act 1 environment / player / HUD). Not a 60+60 or A3v milestone.

## Reproduction (RTX 3080)

- Host: `tools/run_nvidia_clean.sh` → DXVK reports **NVIDIA GeForce RTX 3080**.
- Script: `out/a3x_tr1_shot.txt` (same timing as `tools/a2g_tr1_enter.txt`) + `--nand out/test_nand`.
- Corruption: stretched / exploded triangles, ghosted HUD digits, near-white stage entry. Present from first gameplay XFBs in TR1; persists across camera motion; affects environment, player, and HUD geometry.

## Shader cache isolation

| Config | Result |
|---|---|
| Full A3w portable DXBC cache | Corrupted **before** fix; clean **after** fix |
| Empty `out/shadercache` (no preload) | Same corruption **before** fix; clean **after** fix (MAD≈0.6 vs full-cache fixed frame) |

**Conclusion:** A3w offline/preload was **not** the cause. Cache left intact.

## Last known-good / first known-bad

| | Commit | Notes |
|---|---|---|
| **Last good** | `8991926` (A3r) | Pre-A3s `runtime/gx/renderer.cpp` → TR1 env/player/HUD correct |
| **First bad** | `055a6dc` (A3s) | Flush bind-cache + partial XF upload |

Later A3t (`c57de28`) extended the same bind cache (OM/CB/tex/fill) and inherited the topo sentinel bug. A3u/A3v/A3w experimental paths remain default-off and were not required to reproduce.

## Root cause

A3s introduced a Flush-path D3D11 bind cache that skips `IASetPrimitiveTopology` when `s_topo == s_boundTopo`.

`InvalidateHostBindCache()` (and the A3s reset path) used **`s_boundTopo = Topo::Tri`** as the “cleared” sentinel. `Tri` is also the **common** GX draw topology.

Sequence:

1. Live GX `Flush` draws a triangle list; cache stores `s_boundTopo = Tri`.
2. `DrawQuad` (EFB clear / copy / present util) binds **`D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP`** without updating `s_boundTopo` (or after an invalidate that sets Tri).
3. Next GX `Flush` with `s_topo == Tri` sees `s_topo == s_boundTopo` and **skips** topology rebind.
4. Triangle-list index data is interpreted as a strip → stretched / exploded geometry and malformed HUD quads.

Partial `UpdateSubresource` XF boxes were also non-portable on DXVK and are kept disabled (full 0x680 upload when dirty), but they were **not** sufficient alone to explain the A3s break once the topo sentinel was understood: forcing full XF + “invalidate every Flush” still failed while the sentinel remained `Tri`.

## Fix

Files: `runtime/gx/renderer.cpp`, `runtime/gx/gx_state.h`, `runtime/gx/video.cpp`.

1. `enum class Topo { …, Invalid }` with `static_assert` that `Invalid` ≠ Tri/Line/Point.
2. Bind-cache init / `InvalidateHostBindCache()` use **`Topo::Invalid`**.
3. Keep full XF SRV upload when dirty (no D3D11_BOX partial path).
4. Invalidate bind cache from util draws, OM unbind, EFB copy, and `Video_Present` / overlay so external D3D11 mutations cannot leave stale Flush skips.
5. Optional diagnostic: `SCR_A3T_BIND_CACHE=0` still forces invalidate each Flush.

## Experimental features (unchanged defaults)

| Feature | Default |
|---|---|
| A3v / A3u player composite | **off** (A3v benched) |
| Pose interp / A3b–A3d mid | **off** |
| Decoupled present / FIFO capture | **off** |
| A3w shader cache | **on** (preserved) |
| A3s/A3t Flush bind cache | **on**, with corrected Invalid sentinel |

## Validation

- Title / stage-select UI / TR1 gameplay: correct after fix (see `out/a3x_shots/topo_fix_*.png`).
- Full vs empty portable cache: equivalent TR1 frames (MAD ≪ 1 on matched shot).
- Linux RTX 3080: **verified**.
- MinGW Windows build: see PROGRESS / commit notes.
- Native Windows GPU: **UNVERIFIED**.

## Related

- A3w: `docs/A3W_OFFLINE_SHADER_PRECOMPILATION.md`
- A3s/A3t: `docs/A3S_CACHED_AND_ASYNC_GX_REPLAY.md`, `docs/A3T_D3D11_SUBMISSION_OPTIMISATION.md`
