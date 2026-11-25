# A5c — BP shadow-state design

## Authoritative representations

| Layer | Location | Role |
|---|---|---|
| Guest `__gx` / GXData | SNCP8P `lbl_807FA960` (size `0x600`), pointer `__GXData` @ `808FC248` | Game-readable SDK shadow; dirty flag @ `+0x2` |
| Host BP array | `g_gx.bp[0x100]` | Hardware-facing BP after FIFO decode or typed apply |
| Shared semantic state | `PSKey` / `VSKey` / `GfxDrawBatch` via `Render_OnBP` | Backend-independent draw state |
| Backend derived | D3D11/Vulkan PSO, blend, depth | Consumes `GfxDrawBatch` only |

Guest `__gx` remains authoritative for anything the game can inspect. Host `g_gx.bp` must never skip a write using a stale/unknown shadow value.

## SNCP8P `__gx` fields used by A5c adapters

| Offset | Content | BP reg |
|---|---|---|
| `+0x002` | dirty flags (`u16`, cleared after BP write) | — |
| `+0x180 + 4*stage` | TEV colour env (full BP word incl. reg id) | `0xC0+2*stage` |
| `+0x1C0 + 4*stage` | TEV alpha env | `0xC1+2*stage` |
| `+0x220` | CMODE0 / blend + colour/alpha update | `0x41` |
| `+0x228` | ZMODE | `0x40` |

## Validity

- Each BP register has a validity bit; generation counter bumps on invalidate.
- First apply after invalidate **re-establishes** validity (even if value equals stored bits).
- `GX_Init` calls `GxShadow_InvalidateAll`.
- Identical ordinary skips require: ordinary class + valid shadow + `nv == old`.

## Classification (from `LoadBP` / `Render_OnBP`)

| Class | Registers | Skip identical? |
|---|---|---|
| Ordinary | all others in `0x00–0xFF` | yes, if shadow valid |
| Special | `0xE0–0xE7` (TEV colour/konst latch) | **never** |
| Trigger | `0x45`, `0x47`, `0x48`, `0x52`, `0x65` | **never** |
| Unknown | out of range | **never** |

Source: SNCP8P side effects in `GX_ApplyBpWord` / `Render_OnBP` (finish/token, EFB copy, TLUT, konst restore).

## Coherency paths

1. Legacy FIFO `0x61` → `GX_ApplyBpWord`
2. Typed direct `GfxNative_EnqueueBp` → drain → `GX_ApplyBpWord`
3. Display-list playback → same FIFO path
4. Display-list **recording** → `PPC_ORIGINAL` (no immediate host apply)
5. Guest `__gx` updated by adapters before/with enqueue success; original path on fallback

## API

- `runtime/gx/gx_shadow.h` — classify, validity, `GX_ApplyBpWord`, per-reg profile
- Env: `SCR_A5C_BP_PROFILE=1` for `A5c-bpprof-*` logs
