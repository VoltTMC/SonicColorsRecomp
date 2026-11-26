# A5k — Ordering preservation

## Guest-visible order

1. Pending draws observe XF/BP/tex state at Flush time (pre-indexed write).
2. Indexed XF updates `g_gx.xfmem` at the original command point.
3. Subsequent draws observe post-update XF.
4. Display-list recording remains `PPC_ORIGINAL`; playback uses the same `GX_ApplyIndexedXf`.

A5k does **not** reorder or skip non-empty geometry drains.

## Barrier rules

| Situation | Behaviour |
|---|---|
| No pending indices + A5h PLAYBACK | Empty Flush skip (unchanged) |
| Pending indices + indexed XF | Full Flush, then XF write (unchanged) |
| Consecutive indexed XF after drain | Empty skip (A5h); no second drain |
| Interrupted by draw / BP / other consumers | Next pending geometry uses post-interrupt state; next idx with pending drains again |

## Opt path (`SCR_A5K_INDEXED_XF_FLUSH_OPT=1`)

- Still performs the non-empty Flush at the same point.
- Vulkan only: if RAM texture key is already in `s_texCache`, skip `GXTex_Decode` for that map.
- Semantic bind result matches legacy UploadTex cache-hit (same VkImageView).
- Cache miss, EFB-copy path, and D3D11 path unchanged.
- Switch default **off**.

## Cases that keep the legacy Flush body

- Opt disabled.
- Texture key not in Vulkan cache.
- EFB-copy binds (`VkGx_LookupEfbCopy`).
- D3D11 backend (GetTexture already fingerprints; no decode-skip change).

## Resource lifetime

Skipping decode does not extend or shorten Vk image lifetime. Cache entries remain until `VkGx` shutdown / cache clear. Pending geometry still drains before XF mutate; no deferred queue of snapshots.

## Test contract

`scr_a5k_indexed_xf_flush_order_test` models live XF pointers, mandatory drain before mutate, empty-skip after drain, and tex-cache skip equivalence under the opt flag.
