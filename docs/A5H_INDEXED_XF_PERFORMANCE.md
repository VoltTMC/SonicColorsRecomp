# A5h — Indexed XF Performance

## Methodology

- Workloads: title phase + Tropical Resort Act 1 explore (`tools/a4k_tr1_explore.txt`).
- Host: NVIDIA RTX 3080 via `tools/run_nvidia_clean.sh` + `SCR_VK_DEVICE=3080`.
- Vulkan performance profiling; **validation disabled** (not a validation claim).
- A5a–A5e + A5g adapters **on**; `SCR_A5F_DEFERRED_BP=0` (isolate indexed XF / playback).
- Compare **matched elapsed time** and **matched FIFO progress** (`fifoIn`). Do not treat raw end-of-run unique_xfb alone as FPS parity when stage progress differs.

Bench: `tools/a5h_tr1_bench.sh a5g|playback|ext|both|compare`.

## Measured results (2026-11-26, 90s TR1 explore)

Both arms: A5a–A5e + `SCR_A5G_DIRECT_INDEXED_XF=1`, A5f off. Candidate: `SCR_A5H_INDEXED_XF_PLAYBACK=1`.

| Metric | A5g baseline (`PLAYBACK=0`) | A5h playback (`PLAYBACK=1`) |
|---|---:|---:|
| Opcode 0x20 (`fifoIdxOp20`) | 11798 @ end | 742933 @ end / 11748 @ matched fifo |
| Opcode 0x28 | 11798 | 741895 / 11748 matched |
| Opcode 0x30 | 11798 (DL-resident) | 741203 / 11748 matched |
| Opcode 0x38 | 0 | 0 |
| DL indexed-XF (`fifoIdxDl`) | 35394 | 2226031 / 35244 matched |
| Top indexed-XF (`fifoIdxTop`) | 0 | 0 |
| Direct adapter ops (`idxXf`) | 836 | 1222819 / 832 matched |
| FIFO fallback / `dlRec` | 0 / 0 | 0 / 0 |
| `catMs xfIdx` | 179.1 @ `fifoIn≈24.9M` | 177.6 @ matched ≈24.9M; 8757 @ end `fifoIn≈365M` |
| `idxXfFlushSkip` / `flushCall` | 0 / 36230 | 34964 / 1112 matched (≈97% skip); 3379885 / 68965 end (≈98% skip) |
| Title unique XFB/s (mean ≥50) | 59.58 (n=6) | 58.27 (n=7) |
| TR1 mid-band unique XFB/s (3–25) | 15.20 (n=1) | 18.58 (n=12) |
| Guest VI rate | ≈60 (unchanged pacing) | ≈60 |

### Matched FIFO progress (~24.9M `fifoIn`)

| | A5g | A5h playback |
|---|---:|---:|
| `xfIdx` ms | 179.1 | 177.6 |
| `fifoIdx` ops | 35394 | 35244 |
| ms / 1000 fifoIdx | 5.060 | 5.039 |
| Flush skip ratio | 0% | ≈97% |

Early/title matched window: **xfIdx wall nearly unchanged** (decode + guest read + apply still dominate); empty Flush skip is confirmed by counters but small in this band.

### End-of-run notes (not a matched-FPS claim)

The A5g arm remained near title/early load (`fifoIn≈25M`); the playback arm progressed deep into TR1 (`fifoIn≈365M`) within the same 90s wall. Mid-band unique_xfb sample counts differ (1 vs 12). Treat mid unique_xfb as **workload progress**, not a GPU FPS win from A5h alone. Per-op `xfIdx` cost at end ≈3.93 ms/kops vs ≈5.06 early baseline — suggestive under heavier DL idx traffic, not a controlled matched claim.

## Claims policy

- Primary A5h claim: **empty Flush skip fires on ≈97–98% of `GX_ApplyIndexedXf` calls** when PLAYBACK=1; consecutive idx decode enabled; `0x30` DL traffic measured and left on FIFO (no SDK adapter).
- Do **not** claim title/TR1 FPS improvement from unmatched stage progress.
- D3D11 remains default; A5h switches default **off**. D3D11 smoke: backend init OK on RTX 3080 (vkd3d).

## Remaining bottlenecks

- Guest-memory indexed reads and XF upload path.
- Special konst BP `0xE0–E7` (intentionally immediate).
- Draw/CP submission (out of scope).
- DL-resident `0x30` (no SDK adapter; playback opts only).
