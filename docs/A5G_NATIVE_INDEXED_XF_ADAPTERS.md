# A5g — Native Indexed XF Adapters

## Goal

Migrate the highest-value indexed-XF SDK entry points from FIFO opcode transport to typed native adapters, preserving `GX_ApplyIndexedXf` semantics (including A3 pose/ownership hooks).

Opt-in: `SCR_A5G_DIRECT_INDEXED_XF=1` (default **off**).

## Migrated SDK functions (SNCP8P)

| Function | Address | FIFO op | CP array | XF dest | Size |
|---|---|---|---:|---|---:|
| `GXLoadPosMtxIndx` | `0x8036EE60` | `0x20` | 12 | `id * 4` | 12 |
| `GXLoadNrmMtxIndx3x3` | `0x8036EEF0` | `0x28` | 13 | `0x400 + id * 3` | 9 |

Verified against recomp `fn_8036EE60` / `fn_8036EEF0` (FIFO write of opcode + packed word).

Not migrated: TexMtx/Light indexed opcodes `0x30`/`0x38` — no matching simple SNCP8P SDK wrappers found in the transform neighbourhood; remain FIFO.

## Path

1. SDK override (unless DL recording / enqueue fail) → `EnqueueXfIdx`.
2. Typed ring ordered by `afterFifoW` vs gather consume.
3. `ApplyXfIdx` → `GX_ApplyIndexedXf` (shared with FIFO `LoadIndexedXF`).
4. Guest array read at **apply** time from `CP_ARRAY_BASE/STRIDE` (matches FIFO ordering vs prior CP writes).

## Fallbacks

- `SCR_A5G_DIRECT_INDEXED_XF` unset/0 → original PPC.
- `GX_FifoRecordingToRAM()` → `PPC_ORIGINAL` (DL record).
- Typed ring full → original PPC (`directFallback++`).

## Special konst

`0xE0–0xE7` unchanged (A5f immediate OnBP latch). Not deferred or coalesced by A5g.

## Tests / benches

- `scr_a5g_indexed_xf_test`
- `tools/a5g_tr1_bench.sh` — `baseline|indexed|combined|compare`
