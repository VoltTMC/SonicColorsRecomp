# A5l — Vulkan draw optimisation

## Switch

`SCR_A5L_VULKAN_DRAW_OPT=1` — default **off**. Legacy per-draw descriptor update + always-bind path remains.

Independent of `SCR_A4N_DESC_CACHE` (still default off; not required).

## What it does

### 1. Sticky in-CB descriptor reuse

While the same command buffer is recording:

- **Full hit:** identical XF bind offset + 8 views/samplers + identical VS/PS constants → reuse last `VkDescriptorSet` and dynamic UBO offsets (no UBO memcpy, no `vkUpdateDescriptorSets`).
- **Partial hit:** same XF+textures, different constants → new dyn-UBO slots + memcpy; **reuse** the set (descriptors store buffer+range; offsets select slots).
- **Miss:** allocate next frame set and full update; become the new sticky.

### 2. Bind skip

Skip `vkCmdBindPipeline` / `SetViewport` / `SetScissor` when identical to the last recorded values in this CB.

VB/IB offsets usually change every draw → always rebound.

## Invalidation

Sticky + bind state cleared on:

- `ResetFrameDescriptors` (present retire, mid-draw flush)
- `RetireEfbCopyEntries` (destroyed EFB views must not linger in sticky keys)
- Device shutdown / GX recreate

## What it does **not** do

- Does not skip mandatory A5k geometry flushes.
- Does not change `GfxDrawBatch` meaning or D3D11.
- Does not enable A4n descriptor map cache or async present.
- Does not defer or reorder draws.
- Does not skip VB/IB copies (geometry differs).

## Lifetime

Reused sets remain from the current frame pool until reset after GPU wait. Sticky never outlives the pool reset. Partial reuse is valid because dynamic UBO offsets are bind-time.

## Fallback

Opt off → every draw updates a fresh set (A4n default behaviour).
