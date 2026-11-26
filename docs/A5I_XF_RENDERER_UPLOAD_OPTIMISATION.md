# A5i — XF Renderer Upload Path

## A4n baseline (preserved)

Vulkan XF SSBO:

- Payload **`0x680` floats** (D3D parity); full **16 KiB slot bind** + zero-pad.
- Ring slot memcpy + hash/memcmp **dedup** (`SCR_A4M_XF_DEDUP`, default on).
- `SCR_A4N_XF_FULL=1` restores legacy `0x1000` for A/B only.
- Descriptor cache / async present remain **opt-in / off** (hang risk).

D3D11:

- Full `UpdateSubresource` of `0x680` floats when `s_xfDirty` (partial box unsafe on DXVK).
- Dirty range tracked for profiling; not used for partial GPU updates.

## Interaction with A5g/A5h/A5i

| Layer | Effect on uploads |
|---|---|
| A5g typed Indx | Same `GX_ApplyIndexedXf` → same dirty marks |
| A5h playback Flush skip | Does not clear dirty; uploads still at real draws |
| A5i `SCR_A5I_INDEXED_XF_APPLY` | Skips `MarkXfDirty` when payload **identical** → fewer dirty→upload cycles when matrices repeat |

A5i does **not** change SSBO layout, slot size, dedup algorithm, or A4n defaults.

## Why no new upload rewrite

Profiled TR1 (A5h playback deep run): `xfMemcpyMs≈19` vs `catMs xfIdx≈8757` and `drawFlushMs≈6k`. Upload volume (~0.5–1 GiB/explore with A4n) is already far below the A4m ~45 GiB class. Replacing A4n would not address the dominant guest-apply CPU cost.

## Implemented upload-side effect

Identical-payload dirty skip (guest path) is the only A5i change that reduces renderer upload pressure. Vulkan dedup remains the last line of defence for byte-identical full snapshots across draws.
