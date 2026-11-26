# A5k — Indexed-XF flush performance

## Switches (primary bench)

| Switch | Value |
|---|---|
| `SCR_A5A_DIRECT` … `SCR_A5E_DIRECT_TEXOBJ` | `1` |
| `SCR_A5F_DEFERRED_BP` | `0` |
| `SCR_A5G_DIRECT_INDEXED_XF` | `1` |
| `SCR_A5H_INDEXED_XF_PLAYBACK` | `1` |
| `SCR_A5I_INDEXED_XF_APPLY` | `1` |
| `SCR_A5J_INDEXED_XF_DISPATCH` | `1` |
| `SCR_A5K_INDEXED_XF_FLUSH_OPT` | `0` baseline / `1` candidate |
| Renderer | Vulkan (`--renderer vulkan`) |
| `SCR_A4N` XF upload | default (0x680) |

## Instrumentation (exclusive CPU)

`GfxNative` A5k counters (atomics; no nested double-count of Apply vs Flush wall):

- `flushIdxOrigin` / `flushOtherOrigin`
- `flushEmpty` / `flushNonEmpty` / `flushBatches`
- `flushVerts` / `flushIndices` / `flushMaxVerts` / `flushMaxIndices`
- `flushTexDecodeNs` / `flushTexDecodeSkip` / `flushTexDecodeMaps`
- `flushPrepNs` / `flushSubmitNs` / `flushBodyNs`
- `flushVk` / `flushD3d`

Logged as `A5k-<tag>: …`. Indexed-XF inclusive time remains `catMs xfIdx`; Apply excl. Flush remains `A5i flushMs` / `applyMs`.

## What A5k changes

- **Does not** eliminate mandatory non-empty flushes (ordering).
- **Does** skip redundant Vulkan CPU tex decode when the cache already holds the key (work UploadTex previously discarded).
- Memory: no extra XF snapshots; transient `s_vkTexMip` unused on skip.

## Matched results (RTX 3080)

`tools/a5k_tr1_bench.sh` 90s TR1 explore, Vulkan, A5g–A5j on, A5f off. Compare near `fifoIn≈80M` (not end-of-run — flushopt progressed further). Guest VI ≈60 throughout; do not treat unmatched mid unique_xfb as an FPS claim.

| Metric | A5j baseline (opt=0) | A5k candidate (opt=1) |
|---|---:|---:|
| FIFO input | ≈80.7M | ≈79.2M |
| Indexed-XF apply | ≈519k | ≈502k |
| Flush idx origin / A5h flushCall | 15730 | 15201 |
| Empty / non-empty (all origins) | 1.66M / 279k | 1.62M / 275k |
| Flushes eliminated | **0** (barrier retained) | **0** |
| Tex decode skip / maps decoded | 0 / 222k | **219k / 58** |
| Flush texDec / body / submit ms | 3361 / 6168 / 2711 | **3.0 / 2729 / 2647** |
| Indexed-XF flushMs (Apply wall) | 1999 | **1276** (−36%) |
| Indexed-XF inclusive xfIdx ms | 2039 | **1348** (−34%) |
| Apply excl. flush (applyMs) | 62 | 63 |
| Title unique XFB/s (mean) | ≈58.5 (n=8) | ≈58.5 (n=10) |
| TR1 mid unique XFB/s | ≈19.3 (n=11) | ≈20.7 (n=11) unmatched progress |
| Guest VI | ≈60 | ≈60 |

Per non-empty flush at ~80M: body ≈22 µs (opt=0) vs ≈10 µs (opt=1). Tex-decode share of Flush body collapses under opt=1; submit/prep dominate remaining cost. End-of-window flushopt processed more FIFO (`≈443M` vs `≈381M`) with `texSkip≈1.88M` — consistent with cheaper Flush, not with fewer barriers.

Instrumentation overhead: opt=0 path still accumulates A5k counters whenever PROF is on (same as A5j PROF suite); no per-op mutex.

## Remaining bottlenecks

- Non-empty Flush still required on every geometry→idx edge (`flushCall` ≈ idx barriers).
- Remaining Flush CPU: `VkGx_Draw` record/submit, prep (keys/constants), rare cache-miss decode, A4n XF upload.
- D3D11 not helped by this opt (already fingerprint-cached).
