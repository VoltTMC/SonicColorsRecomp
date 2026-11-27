# A5n — Vulkan XF upload performance

## Switch

`SCR_A5N_VULKAN_XF_UPLOAD_OPT=1` — dirty-range fast-reject + full exact `memcmp` confirm against a CPU shadow; **no content hashing** on the A5n hot path. **Default off.**

Independent of `SCR_A5L_*`, `SCR_A5M_*`, `SCR_A5I_*` (no hidden coupling). Disabling restores the A4n hash+memcmp+post-hash behaviour (exact compares still use the CPU shadow).

## What A5n avoids vs does not

| Work | Effect |
|---|---|
| Full-payload hash (pre- and post-copy) | Avoided entirely under opt (dominant A4n cost) |
| Full-payload memcmp on dirty-range **miss** | Replaced by small-range memcmp then upload |
| Full-payload memcmp on dirty-range **hit** | Still required before reuse |
| GPU-visible upload bytes | Unchanged when content actually differs (still full slot) |
| A4n full-slot bind / shader layout | Unchanged |
| Descriptor updates | Unchanged (A5l owns sticky reuse) |

Rejected candidates:

1. Dirty-range equality alone as full-payload proof → TR1 input-script hang (watchdog).
2. Exact `memcmp` against the host-visible XF ring → title unique XFB collapsed (~18 vs ~60) from CPU/GPU read contention; fixed with `s_xfLastCpu` shadow.

## Bench recipe

```bash
tools/a5n_tr1_bench.sh baseline   # A5l+A5m on, A5n=0
tools/a5n_tr1_bench.sh xfopt      # A5l+A5m on, A5n=1
tools/a5n_tr1_bench.sh compare
```

Matched settings: A5a–A5e/A5g–A5k on, A5f deferred BP off, A5i apply on, scale 1, no vsync, RTX 3080 clean runner.

## Results (RTX 3080, 90s TR1 script, A5l=1 A5m=1)

Matched ≈1.27M draws (nearest present samples):

| Metric | A5n=0 | A5n=1 |
|---|---:|---:|
| Draws (lookups) | 1 283 400 | 1 268 127 |
| Hash calls / MiB / ms | 192 501 / 1222 / **7415** | **0 / 0 / 0** |
| Memcmp calls / MiB / ms | 3846 / 24.4 / 0.7 | 118 453 / 422.7 / 3.8 |
| GPU-visible uploads / MiB | 95 863 / 608.5 | 112 640 / 715.0 |
| Dirty-range hits / misses | 0 / 0 | 4378 / 109 697 |
| `drawXfNs` (ms) | **7498.5** | **94.1** (−98.7%) |
| `drawEntryNs` (ms) | **11239.6** | **7772.4** (−30.8%) |
| XF share of draw entry | 66.7% | 1.2% |
| Title unique XFB/s (mean) | 59.4 | 58.9 |
| TR1 mid unique XFB/s | 10.7 (n=1) | 20.4 (n=2) |
| Guest VI | ≈60 | ≈60 |
| End-window fifoIn | 409M | 532M (further progress in 90s) |

Upload bytes rose slightly at matched draws (more distinct XF payloads in the xfopt sample’s stage mix). Hash work avoided is the primary CPU win; memcpy bytes are not reduced (A4n full-slot retained).

End-of-run (unmatched duration progress): xfopt completed more draws/FIFO — do not treat end-window FPS as a matched comparison.

## Memory

+16 KiB CPU shadow of the last uploaded XF slot (`s_xfLastCpu`). No additional XF ring capacity. No unbounded payload cache. Generation is a uint64 counter only.

## Remaining bottlenecks

After A5n, matched draw-entry time is dominated by shader/pipe (`shMs`/`pipeMs`), VB/IB copies (`copyMs`), and descriptor work — not XF. A5m soft reclaim still unproven when rings do not overflow. AMD/RADV and native Windows Vulkan **UNVERIFIED**. Validation not claimed for these runs.
