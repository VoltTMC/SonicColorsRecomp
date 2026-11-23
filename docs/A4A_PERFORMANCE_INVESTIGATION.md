# A4a — Integrated GPU performance investigation

**Date:** 2026-11-23  
**Scope:** Normal (original-frame) rendering path only. A3v remains benched/off. A3w/A3x preserved.

## Hardware and drivers

| Role | Device | Driver / backend |
|---|---|---|
| **Integrated GPU (primary)** | AMD Raphael iGPU — `AMD Ryzen 5 7600X 6-Core Processor (RADV RAPHAEL_MENDOCINO)` / Radeon 610M (`1002:164e`) | Mesa **26.3.1** radv; DXVK via D3D11 |
| Discrete (comparison) | NVIDIA GeForce RTX 3080 | NVIDIA 620.73.4; DXVK; `tools/run_nvidia_clean.sh` |

Clean AMD runner: `tools/run_amd_clean.sh` (`env -i` + `VK_ICD_FILENAMES=radeon_icd.json`). Do **not** use NVIDIA ICD vars on AMD.

Confirm adapter log line: `D3D11 adapter: … (RADV RAPHAEL_MENDOCINO)` vs `NVIDIA GeForce RTX 3080`.

## Display / config under test

- Display refresh: **120 Hz**
- VSync: **off** + tearing allowed
- Guest target: `--60fps` (sim paced to ~60 Hz when host can keep up)
- Default `internal_scale = 3` → EFB **1920×1584** (matches the reported ~37 FPS case)
- Warmed portable DXBC cache (A3z): 330 VS + 6793 PS / 640 `.cso`
- A3v / player mid / decoupled present: **off**

## Methodology

Deterministic TR1 entry: `tools/a2g_tr1_enter.txt` + `out/test_nand`.  
Metrics from `--perf` / status lines after `stg110`:

- `unique_xfb` = unique guest frames (primary FPS metric)
- `simulation_hz` = guest sim steps
- `present_calls` / Present wall + A4a `present_split`
- `SCR_FRAME_PROFILE=1` → Flush CPU / Map / XF / bind stats per XFB

Do not mix startup preload time with gameplay FPS.

## Baseline (warm cache)

### Title / menus (iGPU)

| Scene | Scale | unique_xfb | Notes |
|---|---|---|---|
| Title | 1–3 | **~60** | VI-wait bound; Present ≪ 1 ms when light |
| Title cold cache | 3 | **~56** | Runtime compile hitch (`cmp=5/8`); still playable |

### Tropical Resort Act 1 (after `stg110`)

| GPU | Scale | unique_xfb (avg) | Present() ms | Flush CPU ms/xfb | Draws/xfb |
|---|---|---|---|---|---|
| **RADV 610M** | **3** | **~37–38** (min ~33) | **~10.5–11** | **~2.4–2.5** | ~700–1200+ |
| RADV 610M | 3 (busier script window) | ~32–35 | ~10.5 | ~4.4 | ~1800+ |
| **RTX 3080** | **3** | **~59.8–60** | **~0.05** | **~2.1** | ~670 |

Frame budget at 37 FPS ≈ **27 ms**. Approximate split on iGPU scale 3:

1. **Present() / GPU drain ~10.5 ms** (blit GPU timestamp only ~0.2 ms — queue wait, not blit cost)
2. **Guest + GX decode + other gap ~12–15 ms**
3. **Flush CPU submit ~2.5 ms** (prep ~0.9, Map ~0.8, XF ~0.16)

## Bottleneck classification

**Primary: GPU-bound (B) at `internal_scale=3` on Radeon 610M**, with mixed CPU guest work (F).

Evidence:

- Same Flush CPU (~2.1–2.5 ms) on RTX 3080 and 610M, but only RTX holds 60 unique_xfb.
- Present() blocks ~10+ ms on iGPU with VSync off → GPU backlog.
- Title (70 draws/xfb) hits 60 on iGPU; TR1 (1000+ draws, 9× pixels vs scale 1) does not.
- GX-sync ≈ 0; not sync-mutex dominated.
- Shader cache hits: full warm cache → `cmp=0/0` mem-miss 0 after preload.

**60 FPS at scale 3 on this iGPU is not achievable** after removing avoidable Present-queue stalls: remaining cost is fill/draw rate on a 2-CU RDNA2 iGPU sharing system memory.

## Implemented changes

| Change | Effect |
|---|---|
| `SCR_FRAME_PROFILE=1` + `present_split` logging | Optional normal-path Flush/Present breakdown (off by default) |
| Swap chain **BufferCount=3**, **MaxFrameLatency=3** (`SCR_SWAP_BUFFERS` / `SCR_MAX_FRAME_LATENCY`) | Avoids pathological Present stalls with BufferCount=2 on this iGPU (observed Present() ~80–120 ms and failed stage entry). Steady TR1 FPS still ~37–38 (GPU fill limited). |
| Skip backbuffer Clear when blit has **no letterbox bars** | Avoids a full-window clear when the game fills the client area |
| `tools/run_amd_clean.sh` | Clean RADV-only launches |

A3x `Topo::Invalid` bind-cache sentinel unchanged. Full XF `UpdateSubresource` retained (no partial-box regression).

## Before / after (iGPU TR1 scale 3, warm cache)

| Config | Result |
|---|---|
| Pre-A4a (BufferCount=2, prior builds) | ~37 unique_xfb (user report + re-measured) |
| BufferCount=2 + MaxLatency=3 (A4a probe) | Load path Present() ~87–120 ms; TR1 entry unreliable in scripted runs |
| **A4a default BufferCount=3 + MaxLatency=3** | Stable entry; **~37–38 unique_xfb**; Present() ~10.5 ms |

Clear-skip did not move TR1 FPS (GPU work dominated by EFB draws, not the present clear).

## Guidance for integrated GPUs

- Prefer **`internal_scale` 1 or 2** for playable 60-class rates on weak iGPUs; scale 3 is a discrete-GPU quality target.
- Keep the packaged shader cache (A3z); cold cache adds compile hitches.
- Use `tools/run_amd_clean.sh` / `tools/run_nvidia_clean.sh` so the intended ICD is selected.
- Profile with `SCR_FRAME_PROFILE=1` and `--perf --no-vsync` when investigating.

## Correctness

- TR1 `stg110` reached on iGPU (scale 3) and RTX 3080 (scale 3).
- Warm cache preload 330/6793; cold cache fallback intact.
- A3v off; A3x topology asserts present.
- `scr_shader_cache_test` OK; Linux + MinGW builds OK.
- Native Windows GPU: **UNVERIFIED**.

## Remaining limits

- Radeon 610M cannot sustain 60 unique_xfb at 1920×1584 GX load.
- Further gains need less GPU work (lower internal scale, or future batching/overdraw reductions that preserve GX semantics) — not more Present queue depth.
- Experimental mid-frame paths (A3v) remain out of scope.
