# A5m — Buffer lifetime and reclamation

## Rules (unchanged core)

1. Host-visible VB/IB ranges written while recording a CB must not be overwritten until that CB’s fence signals.
2. Mid-frame reclaim of the same buffer therefore **requires** submit + fence wait (cannot skip the wait).
3. XF SSBO slots and descriptor sets bound in that CB are likewise retired by the same wait.
4. Present path / `DrainFrameSlot` reset shared arenas after GPU completion.
5. No `vkDeviceWaitIdle` / `vkQueueWaitIdle` on the hot path.

## Legacy mid-frame flush

Submit → wait → `ResetFrameDescriptors` + XF ring rewind + `vbPos=ibPos=0`.

Correct, but discards sticky A5l descriptor state and forces XF re-upload after a **geometry-only** capacity miss.

## A5m geom-soft reclaim (`SCR_A5M_VULKAN_GEOMETRY_STREAM_OPT=1`)

When the flush reason is **Geom** only:

1. Submit + fence wait (same lifetime guarantee).
2. `vbPos = ibPos = 0` — reuse geometry rings from the start.
3. **Keep** `s_xfRingPos`, dynUbo cursor, descriptor pool cursor.
4. Invalidate A5l bind sticky (new CB); descriptor *sets* remain allocated until a full reset.

XF / Desc flushes still use the full reset (arenas themselves exhausted).

## Larger rings

With the opt enabled **before** `VkGx_Init`, each slot’s VB and IB are **32 MiB** instead of **16 MiB** (3 frames → 192 MiB geometry vs 96 MiB). Reduces geom flush frequency; memory stays bounded. Opt after init does not resize existing buffers.

## Fallback

`SCR_A5M_VULKAN_GEOMETRY_STREAM_OPT=0` (default): 16 MiB rings; every mid-frame flush full-resets (A5l-era behaviour).
