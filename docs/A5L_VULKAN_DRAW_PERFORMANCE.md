# A5l — Vulkan draw performance

## Primary settings

| Setting | Value |
|---|---|
| Renderer | Vulkan, `SCR_VK_DEVICE=3080` |
| Scale / VSync | 1 / off |
| Validation | **off** (perf) |
| A5f | `0` |
| A5g–A5j | `1` (same as A5k baseline) |
| A5k flush opt | `0` or `1` (interaction matrix) |
| A5l draw opt | `0` baseline / `1` candidate |
| A4n XF | default `0x680` |
| A4n desc cache | `0` |
| PROF | `SCR_A4M_PROF=1` `SCR_A4N_PROF=1` + A5 PROF |

## Matched results (RTX 3080)

`tools/a5l_tr1_bench.sh` 90s TR1, Vulkan, A5g–A5j on, A5f off, A5k flush opt **on** for primary pair. Compare near **≈280k draws** (not end-of-run). Guest VI ≈60. Validation off.

| Metric | A5k baseline (A5l=0) | A5l candidate (A5l=1) |
|---|---:|---:|
| Draws | 280667 | 282826 |
| drawEntryMs | 8079.5 | **5800 (−28%)** |
| µs/draw | ≈28.8 | ≈20.5 |
| shader / xf / tex Ms | 2859 / 3634 / 85 | 2040 / 2816 / 65 |
| desc / copy / pipe / rec Ms | 215 / 73 / 819 / 272 | **71 / 77 / 474 / 148** |
| stickyFull / partial / miss | 0 / 0 / 280667 | **26824 / 223839 / 32163** |
| descUpdates | 280667 | **32163 (−89%)** |
| Sticky hit rate | 0% | **≈89%** (full+partial) |
| bind pipe/vp/sc skips | 0 | high (logged per present) |
| midFrameFlushes / flushWaitMs | 0 / 0 | 0 / 0 (at this point) |
| A5k submitMs (Flush→Draw) | ≈8130 | ≈5839 |
| Title unique XFB/s | ≈56.6 (n=6) | ≈57.0 (n=7) |
| TR1 mid unique XFB/s | unmatched n | unmatched n |
| Guest VI | ≈60 | ≈60 |

A5k flush **off** + A5l **off** at ≈80M fifo: `texDecMs≈3898`, `submitMs≈3158` — decode still dominates without A5k; A5l targets post-decode Draw CPU.

Long 90s drawopt run reached `draws≈660k` with `midFlush=25` `flushWaitMs≈94` late — **no hang / DEVICE_LOST** in this window. A4n desc-map cache remained off.

Stage timers always accumulate under `SCR_A4N_PROF` (both arms) — overhead common to baseline and candidate.

## Dominant bottleneck (pre-opt)

Exclusive split with A5k on: `drawXfNs` + `drawShaderNs` + `drawPipeNs` + per-draw `descUpdates` and bind recording. A5l cuts descriptor updates (~89%) and bind traffic; XF/shader remain large shares of remaining `drawEntryNs`.

## Remaining bottlenecks

- XF upload/hash path (`drawXfNs`).
- Shader module / pipeline lookup spikes during load.
- VB/IB host memcpy every draw.
- Mid-frame ring flushes under heavy outdoor load.
- Present-path waits (A4m chain still default off).
