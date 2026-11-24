# A4c — Native Vulkan GX Draw Pipeline (Stage B)

**Status (2026-11-24):** Stage B **partial** — real GX `HostVertex` geometry is submitted through native Vulkan (`vkCmdDrawIndexed`). D3D11 remains the **default** gameplay renderer. Title/menus/TR1 are **not** at visual parity yet.

## Architecture

```
Wii GX FIFO → shared GX decode (unchanged)
           → Flush() builds GfxDrawBatch (gfx_draw.h)
           → VkGx_Draw → Vulkan EFB + pipelines + present blit
```

D3D11 path is unchanged when `--renderer=d3d11` / default.

## Backend interface

| Piece | Role |
|---|---|
| `gfx_backend.{h,cpp}` | `d3d11` \| `vulkan` selection (`--renderer`, `SCR_RENDERER`) |
| `gfx_draw.h` | Backend-independent batch: topology, viewport/scissor, verts/idx, XF, `VSConstants`, tex0 |
| `vk/vk_device.*` | Stage A device/swapchain + Stage B accessors / acquire+present helpers |
| `vk/vk_gx.*` | EFB, ring VB/IB, SPIR-V modules, pipeline cache, `DrawIndexed`, XFB blit |

## Flush integration

`Flush()` early-outs to `FlushVulkan()` when `VkGx_IsActive()`:

1. Viewport / scissor from XF+BP (same math as D3D11).
2. Full XF upload (`xfmem` SSBO) when dirty.
3. `FillVSConstants` → VS UBO (projection, depth, materials).
4. Optional tex0: `GXTex_Decode` → Vulkan image cache (RAM textures).
5. `VkGx_Draw` → bind pipeline + VB/IB + descriptors → `vkCmdDrawIndexed`.
6. Sets `s_boundTopo = Topo::Invalid` (A3x sentinel preserved).

`DoEFBCopy` XFB → `VkGx_PresentXfb` (EFB blit to swapchain). EFB→texture copies are **not implemented** (rate-limited `VkGx_NoteUnsupported`).

## Buffers

- Per-frame host-visible arenas (3 frames × 16 MiB VB + 16 MiB IB).
- Wrap → submit current CB, advance frame, wait fence.
- Present currently `vkDeviceWaitIdle` for correctness (optimise later).

Diagnostics: draws, verts, indices, upload bytes, pipeline hit/miss, tex uploads, unsupported ops.

## Shaders

- **Not** DXBC reinterpreted as SPIR-V.
- Embedded GLSL→SPIR-V (`glslangValidator -V`) in `vk/vk_spv_embed.h` / `vk/shaders/*.glsl`.
- **4 modules:** `gx.vert`, `gx.frag`, `blit.vert`, `blit.frag`.
- VS: position matrix from XF SSBO + row-major proj + depth/flip (D3D11-aligned).
- FS: vertex colour × optional tex0 (Stage B; full TEV later).
- Separate from A3w DXBC cache (`shadercache/*.cso` untouched).

## Pipelines

Deterministic key: `{topo, cull, blend, depthTest, depthWrite}`. Cached; created on first use (title observed ~3–7 unique PSOs). Dynamic viewport/scissor.

## Supported (Stage B)

| Op | Status |
|---|---|
| GX triangle/line/point indexed draws | **Implemented** |
| XF position transform + projection | **Implemented** |
| Viewport / scissor / cull / basic blend / depth | **Implemented** |
| EFB colour+depth RT + clear | **Implemented** |
| XFB present (blit to swapchain) | **Implemented** |
| RAM texture map0 sample | **Partial** |
| Full TEV / lighting / multi-tex / fog | **Not implemented** |
| EFB→texture copies | **Not implemented** |
| Overlay / ImGui on Vulkan | **Not implemented** |
| Full SPIR-V shadergen parity | **Not implemented** |

## Correctness tests

- `scr_topo_invalid_test` — `Topo::Invalid` ≠ Tri/Line/Point; invalidate-after-utility transitions.
- Runtime: NVIDIA + RADV title boots with non-zero `draws` / `vkCmdDrawIndexed` stats.

## Platform verification

| Platform | Result |
|---|---|
| Linux RTX 3080 (native VK) | A4d: validation-clean real GX; sustained 6k+ XFB / millions of draws; **partial** visuals |
| Linux RADV / AMD | A4d: **unverified / deferred** — see `A4D_VULKAN_STABILITY_AND_RADV.md` |
| MinGW Windows link | OK (`out-windows/SonicColorsRecomp.exe`) |
| Native Windows Vulkan GPU | **UNVERIFIED** |

## Remaining before Vulkan default

Full TEV SPIR-V (or DXC HLSL→SPIR-V), EFB copies, multi-tex/samplers, lighting, fog, overlay, async timestamps without `DeviceWaitIdle`, TR1 visual parity vs D3D11, A4a-class benchmarks labelled honestly.

## Related

- `docs/A4B_NATIVE_VULKAN_RENDERER.md` — Stage A bootstrap
- A3x `Topo::Invalid` — must not regress
- A3w DXBC cache — independent of SPIR-V
