# A5j — Indexed XF Dispatch Profile

## Path

```
RunCommands 0x20/28/30/38
  → [optional consecutive run]
  → LoadIndexedXF → GX_ApplyIndexedXf
        → Render_Flush          ← timed as flushMs (A5j)
        → guest BE convert      ← guestMs
        → Render_OnXF           ← onXfMs
        → (applyMs excludes Flush)
  → FIFO/opcode notes
catMs xfIdx = inclusive case wall
```

## Exclusive breakdown (measured)

Example (DISPATCH=1, A5g+A5h+A5i, `fifoIn≈80M`, RTX 3080):

| Stage | ms | Share of inclusive xfIdx |
|---|---:|---:|
| Inclusive `xfIdx` | 2077 | 100% |
| **`flushMs` (non-empty Flush)** | **2033** | **≈98%** |
| `applyMs` (excl. Flush) | 64 | ≈3% |
| `dispatchMs` (loop − work − notes) | 23 | ≈1% |
| `noteMs` | 2 | ≪1% |

Empty Flush skips (`flushSkip`) remain high under A5h PLAYBACK; the residual `flushCall` path submits pending geometry and dominates inclusive idx time.

## Profiling overhead

Pre-A5j: per-op `std::mutex` in Note* inflated inclusive timing. A5j uses **relaxed atomics** for hot idx/opcode notes and **batched** FIFO tallies under `SCR_A5J_INDEXED_XF_DISPATCH`.

## Origins

| Origin | Counter |
|---|---|
| Top-level FIFO | `fifoIdxTop` (≈0 with A5g direct) |
| DL playback | `fifoIdxDl` (`0x20`/`0x28`/`0x30`) |
| A5g typed | `idxXf` / included in `idxXfApply` |
