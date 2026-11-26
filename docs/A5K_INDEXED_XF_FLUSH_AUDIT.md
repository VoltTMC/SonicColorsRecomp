# A5k — Indexed-XF `Render_Flush` audit

## Call path

```
FIFO/DL 0x20/28/30/38  or  typed XfIdx (A5g)
        → GX_ApplyIndexedXf
            → (A5h) if PLAYBACK && !Render_HasPendingDraw(): skip Flush
            → else Render_SetFlushOriginIndexedXf(true); Render_Flush(); …
            → guest RAM → g_gx.xfmem; Render_OnXF (unless A5i identical skip)
```

`Render_Flush` → `Flush()` → `FlushVulkan()` (if active) or D3D11 `Flush` body.

Other Flush callers (draw topology change, BP consumers, frame/XFB, explicit) share the same drain contract.

## Why the flush is required

Pending host geometry does **not** snapshot XF memory. Both backends set:

```text
batch.xfmem = reinterpret_cast<const float*>(g_gx.xfmem);  // live pointer
```

at flush/submit time. Vertex positions/normals are transformed using that pointer (and derived VS constants built at the same moment). Mutating `g_gx.xfmem` while indices remain queued would make previously submitted guest draws observe the new matrices.

Sequence:

1. Draw opcodes append verts/indices (`s_verts` / `s_indices`); XF still “old”.
2. Indexed-XF arrives while `Render_HasPendingDraw()`.
3. **Mandatory** non-empty Flush drains the batch with current XF / BP / tex / shader keys.
4. Indexed payload writes `g_gx.xfmem`; `Render_OnXF` marks dirty ranges.
5. Later draws consume the updated XF.

Earlier geometry must keep the pre-update XF (and contemporaneous BP/tex/shader state). The flush captures that by rendering before the write.

## What pending geometry references

| Resource | Lifetime relative to Flush |
|---|---|
| `s_verts` / `s_indices` | Host vectors; cleared after drain |
| `g_gx.xfmem` | Live; **no** per-batch copy |
| VS/PS keys + constants | Built at Flush from current BP/XF |
| Textures | Decoded/bound at Flush (D3D fingerprint cache; Vulkan `s_texCache` by key) |
| Shader pipelines | Cached by key |

An XF-only snapshot would still leave BP, TEV, texture, and viewport coupling incorrect if those change before a deferred drain. Full durable state is out of A5k scope.

## Empty vs non-empty

A5h already skips Flush when no pending indices. After the first drain in a consecutive indexed-XF run, subsequent Apply calls are empty and skip. **Non-empty** flushes are the A5j ~98% cost: one barrier per “geometry then indexed XF” edge, not per opcode.

## Alternatives evaluated

| Approach | Verdict |
|---|---|
| A. XF snapshot at queue time | Unsafe alone (live pointer + other state) |
| B. Defer drain after guest XF write | Unsafe without durable XF+VS+BP+tex |
| C. Coalesce consecutive idx drains | Already achieved via A5h empty skip after first drain |
| D. Optimise mandatory Flush body | **Chosen** — Vulkan skip redundant `GXTex_Decode` on tex-cache hit |

## Legacy fallback

`SCR_A5K_INDEXED_XF_FLUSH_OPT=0` (default): always decode RAM textures in `FlushVulkan` (A5j behaviour). Opt-in only skips decode when `VkGx_TexCacheHas(key)`; UploadTex already returned the cached view and discarded pixels.
