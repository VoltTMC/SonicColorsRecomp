# A5h — Display-List Indexed XF Playback

## Problem

After A5g, top-level Pos/Nrm Indx SDK calls bypass FIFO encode/decode, but **display-list-resident** indexed XF (`0x20`/`0x28`, and rarely `0x30`/`0x38`) still runs through `RunCommands` → `LoadIndexedXF` → `GX_ApplyIndexedXf`. That path still paid a full `Render_Flush()` per op even when no geometry was pending.

## Opt-in

`SCR_A5H_INDEXED_XF_PLAYBACK=1` (default **off**).

Does not change A5g (`SCR_A5G_DIRECT_INDEXED_XF`) semantics or defaults.

## Implemented optimisations

1. **Empty Flush skip** inside `GX_ApplyIndexedXf` when `!Render_HasPendingDraw()` (same emptiness rule as A5f BP defer). Guest-memory read, XF write, PoseInterp, and `Render_OnXF` still run in order.
2. **Consecutive indexed-XF decode** in `RunCommands`: when enabled, a run of `0x20`/`0x28`/`0x30`/`0x38` is consumed in one dispatch loop (one `GX_ApplyIndexedXf` per command — order preserved). When disabled, behaviour matches A5g (single-op case).

## Explicitly not done

- No display-list caching / predecoded caches.
- No bypass of the encoded command stream.
- Recording remains `PPC_ORIGINAL` (direct adapters still fall back while `GX_FifoRecordingToRAM()`).
- Nested DLs unchanged (`CallDisplayList` → nested `RunCommands`).
- No change to BP specials / konst latch / token / finish / EFB / TLUT.

## Profiling

With `SCR_A5A_PROF` / A5h switches on:

- `fifoIdxOp20/28/30/38` — decode counts by opcode (top + DL).
- `fifoIdxTop` / `fifoIdxDl` — origin split (`inDisplayList`).
- `idxXfFlushCall` / `idxXfFlushSkip` — Flush vs skip in Apply.

Nested DL bytes remain in `dlBytesDecoded`; top-level decode wall does not double-count nested time (`decodeNs` vs `decodeNestedNs`).

## Correctness anchors

- `scr_a5h_indexed_xf_playback_test` — pack/apply equivalence, OOB, consecutive DL streams, BP interleave, nest ordering.
- Direct / FIFO / DL state compared on packed streams (byte XF mem where applicable).
