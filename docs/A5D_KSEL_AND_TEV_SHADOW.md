# A5d — KSEL and TEV shadow notes

## Hardware

TEV_KSEL registers: BP `0xF6`–`0xFD` (8 words). Consumed as `PSKey.ksel[8]` in `BuildPSKey` and by D3D11/Vulkan shadergen (konst select + swap tables).

## Guest `__gx` (SNCP8P)

| Offset | Content |
|---|---|
| `+0x170` | KSEL0 shadow (BP `0xF6`) — includes four swap-table pairs |
| `+0x174` | IND_IMASK (BP `0x0F`) |
| `+0x5FC` | dirty-state flags (`GXSetTevSwapMode` ORs `0x3`) |

## A5d adapter

`GXSetTevSwapMode@8036D2B0` (`SCR_A5D_DIRECT_KSEL=1`):

- Updates `gx+0x170` swap bitfields for stage 0–3 (3-bit ras/tex selects).
- Enqueues typed BP word (reg id preserved in high byte).
- Sets `gx+0x5FC |= 3`.
- Clears dirty `+0x2`.
- Recording → `PPC_ORIGINAL`.

Shader impact: `PSKey.ksel[0]` changes → pipeline/shader cache key changes via existing `BuildPSKey`. No separate KSEL state machine.

## Not adapted this milestone

- `GXSetTevKColorSel` / `GXSetTevKAlphaSel` when they only dirty and rely on `__GXSetDirtyState` to flush multiple KSEL words.
- Those remain on FIFO/dirty-flush until a dedicated dirty-state adapter is proven.

## Coherency

Same rules as A5c: guest `__gx` + `g_gx.bp` via `GX_ApplyBpWord` + `Render_OnBP` → shared semantic state → D3D11/Vulkan.
