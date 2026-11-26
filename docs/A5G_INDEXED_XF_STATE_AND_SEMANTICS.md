# A5g — Indexed XF State and Semantics

## FIFO command

Opcode `0x20` / `0x28` / `0x30` / `0x38` + BE32 word:

| Field | Bits | Meaning |
|---|---|---|
| index | 31:16 | Row index into CP array |
| size−1 | 15:12 | Word count minus one |
| addr | 11:0 | XF memory destination |

Array slot: `12 + ((op >> 3) & 3)` → Pos=12, Nrm=13, Tex=14, Light=15.

## Apply

```
guestEa = CP_ARRAY_BASE[array] + index * CP_ARRAY_STRIDE[array]
for i in 0..size-1: xfmem[(addr+i)&0xFFF] = BE32(guestEa + 4*i)
Render_OnXF(addr, size, indexed=true)
```

Shared entry: `GX_ApplyIndexedXf`. Also runs A2b capture, PoseInterp override, A3u/A3h ownership when enabled.

## Direct vs Imm XF

| | Imm (`0x10`) | Indexed (`0x20`/`0x28`) |
|---|---|---|
| Payload source | SDK pointer (snapshot at enqueue) | Guest CP array (read at apply) |
| Typed kind | `Xf` | `XfIdx` |
| `Render_OnXF` indexed flag | false | true |

## Ordering

Typed cmds drain when `afterFifoW <= fifoBytesConsumed`. CP `SetArrayBase`/`Stride` via FIFO therefore apply before a subsequent direct Indx enqueue that observed a later gather watermark.

## Display lists

Recording: adapters call `PPC_ORIGINAL` so FIFO bytes enter the DL. Playback: FIFO `LoadIndexedXF` only (no direct path inside DL bodies).
