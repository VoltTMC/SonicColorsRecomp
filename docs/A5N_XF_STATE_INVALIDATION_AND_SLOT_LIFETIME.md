# A5n — XF writers, invalidation, and slot lifetime

## Production writers (all mark dirty)

| Path | File | Marks dirty |
|---|---|---|
| FIFO `LoadXF` | `gx_fifo.cpp` | `Render_OnXF(..., false)` |
| FIFO / DL `LoadIndexedXF` → `ApplyIndexedXf` | `gx_fifo.cpp` | `Render_OnXF` unless A5i identical skip |
| Direct A5g / gfx command `LoadXF` | `gfx_direct.cpp` | `Render_OnXF` |
| `Render_ForceXfDirty` | `renderer.cpp` | full `0..0x680` + generation bump |
| `Render_ResetHostDrawState` | `renderer.cpp` | full + generation bump |
| Renderer init / recreate paths that force XF dirty | `renderer.cpp` | full range |

`MarkXfDirtyRange(addr,count)`:

- Always sets `s_xfDirty` and increments `s_xfGeneration` (uint64, no wrap concern for process lifetime).
- For `addr < 0x680`, unions `[addr, addr+count)` into `s_xfDirtyBegin/End`.
- For `addr ≥ 0x680`, dirty flag only (float payload unchanged; range may stay `0/0`).

`0/0` dirty encoding is **ambiguous** (initial full need vs high-addr-only). Upload path treats invalid/empty range as **full hash+memcmp fallback** under A5n — never assume float equality from `0/0`.

## Non-production / debug writers

`runtime/debug/a3a_draw_attr.cpp` and `a3b_camera_interp.cpp` can write `g_gx.xfmem` without `MarkXfDirtyRange`. These are capture/debug tools, not the game GX path. A5n dirty-range equality assumes production writers; unknown writers must use the full-compare fallback (or mark dirty).

## Hash / exact-compare semantics

- **A4n (opt off):** `Hash64Bytes` over full payload; on hash match, `memcmp` full payload. Hash alone never binds a slot. After upload, fingerprint the slot bytes (second hash when opt off).
- **A5n (opt on):** If `xfDirtyEnd > xfDirtyBegin` and range ⊆ floatN, `memcmp` that range first as a **fast reject**. Range mismatch ⇒ full-slot `memcpy` without hashing. Range match ⇒ **full-payload `memcmp`** before reuse (incomplete dirty marks must not bind stale XF). If range invalid/`0/0`, full `memcmp` only (no hash). Exact compares use a **CPU shadow** of the last uploaded payload (`s_xfLastCpu`, 16 KiB) — never the host-visible XF ring — so dedup does not contend with GPU sampling. Dirty-range equality alone is never treated as proof of full payload equality.

## Slot lifetime

- Each miss allocates a new 16 KiB-aligned bump slot; prior slots remain intact until the ring is reset.
- Mid-frame XF exhaustion: `flushFrame(Xf)` waits the draw fence, then full-resets XF/desc (and VB/IB).
- A5m geom-soft reclaim: after geom capacity wait, VB/IB reset but **XF last-slot metadata kept** (same retired CB ⇒ ranges idle).
- Present / drain / shutdown: clear `s_xfUploaded`, last hash/off/bytes/gen.
- Canonical `g_gx.xfmem` lifetime ≠ uploaded slot lifetime. Generation on a slot records what was uploaded; generation equality is **not** used alone as byte proof when dirty-range metadata is missing.

## A4n compatibility

GPU-visible layout remains full `0x680` floats in a 16 KiB slot with zero pad. A5n does not switch to partial GPU patches. Descriptor key still uses `s_xfBindOffset`. A5l sticky desc and A5m rings are independent switches.
