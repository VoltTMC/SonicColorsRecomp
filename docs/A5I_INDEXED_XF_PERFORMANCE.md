# A5i — Indexed XF Performance

## Configuration

- A5a–A5e + `SCR_A5G_DIRECT_INDEXED_XF=1` + `SCR_A5H_INDEXED_XF_PLAYBACK=1`
- `SCR_A5F_DEFERRED_BP=0`
- Switch under test: `SCR_A5I_INDEXED_XF_APPLY` (default **off**)
- Vulkan RTX 3080, validation off; `SCR_A4M_PROF` / `SCR_A4N_PROF` for upload counters
- Bench: `tools/a5i_tr1_bench.sh baseline|apply|compare`

## Bottleneck ranking (measured)

| Rank | Area | Evidence (baseline end, `fifoIn≈122M`) |
|---:|---|---|
| 1 | `catMs xfIdx` (decode + Apply + notes) | **3546 ms** |
| 2 | Draw flush / descriptors | `drawFlushMs≈3008` |
| 3 | Apply self-time (`idxXfApplyMs`) | **155 ms** (guest 45 + OnXF 32 + rest) |
| 4 | Vulkan XF memcpy | **`xfMemcpyMs≈9.2`** / `xfBytes≈238 MiB` |

Guest apply is the indexed-XF-specific CPU cost worth optimising; A4n upload memcpy is not dominant.

## Matched FIFO (~121–122M `fifoIn`)

| Metric | Baseline (`APPLY=0`) | Candidate (`APPLY=1`) |
|---|---:|---:|
| Guest indexed-read count (`idxXfApply`) | 1165209 | 1095240 |
| Guest indexed-read bytes | 52.1 MiB | 48.9 MiB |
| Identical / changed | 217197 / 948012 (≈18.6%) | 210780 / 884460 (≈19.2%) |
| `idxXfGuestMs` | 45.3 | 44.1 |
| `idxXfOnXfMs` | 31.5 | **23.9** (−24%) |
| `idxXfApplyMs` | 154.7 | **137.7** (−11%) |
| `catMs xfIdx` | 3546.1 | **3331.3** (−6%) |
| µs / apply | 0.133 | 0.126 |
| Vulkan XF upload (end-of-arm; stage differs) | xfUp=37547 xfBytes≈238MiB xfMemcpyMs=9.2 | deeper run: more uploads (unmatched) |
| Title unique XFB/s | 59.30 (n=7) | 59.90 (n=7) |
| TR1 mid unique XFB/s | 10.40 (n=1) | 19.74 (n=15) — **unmatched stage** |
| Guest VI | ≈60 | ≈60 |

## Interpretation

- ≈19% of indexed payloads are byte-identical to current `xfmem`; APPLY skips `Render_OnXF`/`MarkXfDirty` for those.
- Matched `xfIdx` wall drops ~6%; OnXF self-time ~24%. Modest, repeatable at matched fifo — not an FPS headline.
- Mid unique_xfb sample counts differ (1 vs 15): apply arm progressed further in 90s; **do not** treat as matched FPS.
- A4n `0x680` + dedup unchanged; upload memcpy remains ≪ guest/`xfIdx` cost.

## Claims policy

- Guest-read CPU vs upload memcpy reported separately.
- Identical dirty-skip rate is not an FPS claim by itself.
- A4n upload strategy preserved.
