# A5j — Display-List Indexed XF Playback

## Changes

Under `SCR_A5J_INDEXED_XF_DISPATCH=1`, DL and top-level FIFO share consecutive `0x20/28/30/38` consume plus batched opcode notes. Recording remains `PPC_ORIGINAL`. No DL cache.

A5h PLAYBACK still owns empty Flush skip.

## Finding

DL-resident idx traffic is large (`fifoIdxDl`), but after A5h empty-skip the inclusive DL idx wall is dominated by **non-empty Flush** when draws are pending between idx commands — not by opcode classification or note batching.

## Semantics preserved

- Original command order and stream
- Nested DLs
- Guest reads at playback time
- Fallback: `DISPATCH=0` uses per-op NoteFifo + NoteOpcode (atomics)
