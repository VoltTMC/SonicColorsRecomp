# A5m — Geometry stream performance

## Settings

| Setting | Value |
|---|---|
| Renderer | Vulkan RTX 3080 |
| A5f | 0 |
| A5g–A5k | 1 |
| A5l draw opt | 1 (primary) / 0 (interaction) |
| A5m geom opt | 0 baseline / 1 candidate |
| Validation | off (perf) |

## Matched results (RTX 3080)

`tools/a5m_tr1_bench.sh` 90s TR1, Vulkan, A5g–A5k + A5l on, A5f off. Validation off.

### Near ≈276–277k draws (pre-pressure)

| Metric | A5l (A5m=0) | A5m (A5m=1) |
|---|---:|---:|
| Draws | 277436 | 276152 |
| vb/ib MiB copied | 1006 / 65 | 1004 / 65 |
| copyMs | 38.6 | 37.9 |
| arena MiB | 16 | **32** |
| peak Vb/Ib MiB | 4.3 / 0.3 | 4.3 / 0.3 |
| flushGeom / waitMs | 0 / 0 | 0 / 0 |
| drawEntryMs | 2580 | 2237 |

### End of 90s window (sustained outdoor pressure)

| Metric | A5l (A5m=0) | A5m (A5m=1) |
|---|---:|---:|
| vb/ib MiB copied (cum) | ≈24428 / 1319 | ≈23987 / 1295 |
| copyMs | 860 | 853 |
| peak Vb MiB | **16.0** (full) | **23.4** (<32) |
| flushGeom | **625** (all full) | **0** |
| flushXf / flushDesc | 0 / 0 | 0 / 0 |
| geomSoft / geomFull | 0 / 625 | 0 / 0 |
| flushWaitMs | **620.6** | **0** |
| Title unique XFB/s | ≈58.9 (n=10) | ≈59.6 (n=9) |
| Guest VI | ≈60 | ≈60 |

**Interpretation:** Copies unchanged (still host→mapped memcpy). Mid-frame flushes in this workload were **100% geometry capacity** (`flushXf=flushDesc=0`). Doubling the ring to 32 MiB kept peak occupancy (~23 MiB) under capacity → **eliminated geom flushes and fence waits** for this run. Soft reclaim was not exercised (no overflow); it remains the fallback when 32 MiB is insufficient. Soft path still waits before reuse — it only preserves XF/desc bump state.

No FPS claim from unmatched mid unique_xfb. Validation not claimed.

## Remaining bottlenecks (A5n candidates)

- XF upload/hash (`drawXfNs` still large in A5l-present).
- Shader/pipeline lookup spikes.
- Presentation waits.
- Inevitable VB/IB memcpy bandwidth.
