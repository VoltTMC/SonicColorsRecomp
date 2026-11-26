# A5j — Indexed XF Performance

## Configuration

- A5a–A5e + A5g + A5h PLAYBACK + A5i APPLY; A5f off
- `SCR_A5J_INDEXED_XF_DISPATCH` under test (default **off**)
- Vulkan RTX 3080; validation off
- Bench: `tools/a5j_tr1_bench.sh a5i|dispatch|compare`

## Dominant bottleneck

**Non-empty `Render_Flush` inside `GX_ApplyIndexedXf`** (`flushMs`), required when `HasPendingDraw` so prior geometry observes prior XF state. Not safe to skip. A5h already skips empty Flush.

## Matched results (~120M `fifoIn`, prior pair)

| Metric | A5i (`DISPATCH=0`) | A5j (`DISPATCH=1`) |
|---|---:|---:|
| `catMs xfIdx` | 3360.8 | 3314.8 (−1.4%) |
| `applyMs` | 145.4 | 137.2 |
| Title unique XFB/s (end-of-run means) | ≈58.5 (n=8) | ≈58.6 (n=8) |
| Mid unique XFB/s | unmatched across runs — not used as FPS | |

Deep-run pair (~360–370M): xfIdx within ~1% noise; no material DISPATCH FPS claim.

## Flush instrumentation run (`DISPATCH=1`, `fifoIn≈80M`)

| Metric | Value |
|---|---:|
| `xfIdxInclusiveMs` | 2077 |
| `flushMs` | **2033** |
| `applyMs` / `guestMs` / `onXfMs` | 64 / 23 / 10 |
| `dispatchMs` / `noteMs` | 23 / 2 |
| `flushCall` / `flushSkip` | 15638 / 500764 |
| Identical rate | ≈25% |
| Guest VI | ≈60 |

## Implemented optimisations

1. Atomic hot Notes (`NoteOpcode`, idx Apply/Flush/Fifo) — reduces PROF overhead always when counters are on.
2. `SCR_A5J_INDEXED_XF_DISPATCH=1` — consecutive consume + batched FIFO tallies + exclusive dispatch/note/flush counters.

## Claims policy

- Do not claim FPS from DISPATCH alone; matched xfIdx win is small.
- Do not skip non-empty Flush.
- Next evidence-based target: draw/Flush interaction with indexed XF (ordering-preserving), not note/dispatch micro-opts.
