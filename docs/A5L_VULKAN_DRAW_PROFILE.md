# A5l — Vulkan draw path profile

## Path (shared → Vulkan)

```
Render_Flush / FlushVulkan
  → build GfxDrawBatch (live xfmem ptr, VS/PS keys, tex binds)
  → VkGx_Draw(batch)
       1. validate / size check
       2. Hash VSKey/PSKey → GetOrCreateShaderModules
       3. UploadTex × used maps (+ sampler)
       4. uploadXfSlot (A4n 0x680 + A4m dedup) if dirty
       5. AllocDrawDescriptors (dyn UBO memcpy + UpdateDescriptorSets)
       6. memcpy VB/IB into per-frame rings
       7. BeginEfbPass
       8. GetOrCreatePipe(PipeKey)
       9. Bind pipeline / VB / IB / descriptor / viewport / scissor
      10. vkCmdDrawIndexed
  → (present) End CB → QueueSubmit → fences / swapchain (A4d lifecycle)
```

Mid-draw `flushFrame` (ring/descriptor exhaustion): EndCB → Submit → **CPU WaitForFences** → ResetFrameDescriptors. Counted as `drawFlushWaitNs` / `midFrameFlushes`.

## Exclusive CPU stages (`VkGxStats`)

| Counter | Meaning |
|---|---|
| `drawEntryNs` | Whole `VkGx_Draw` wall |
| `drawShaderNs` | Key hash + shader module lookup/create |
| `drawXfNs` | XF slot upload / dedup |
| `drawTexNs` | Texture cache lookup / upload |
| `drawDescNs` | UBO pack + descriptor alloc/update / sticky |
| `drawCopyNs` | Host VB/IB memcpy |
| `drawPipeNs` | Pipeline lookup/create |
| `drawRecordNsEx` | Bind + DrawIndexed recording |
| `drawFlushWaitNs` | Mid-draw fence waits (sync, not GPU exec) |
| `presentDrawFlushNs` / `presentWaitNs` | Present-path waits (A4m) |
| `gpuEfbCopyNs` / `gpuPresentNs` | GPU timestamps when `SCR_A4M_GPU_TS=1` |

GPU execution is **not** folded into `drawEntryNs`. Nested `descAllocNs`/`descUpdateNs`/`xfMemcpyNs` remain subsets of the stages above.

## Work frequency

| Once per draw (legacy) | Once per state change / frame |
|---|---|
| Descriptor update (fresh set) | Pipeline create (cache miss) |
| Dyn UBO memcpy | Shader module create |
| VB/IB memcpy | Texture upload (cache miss) |
| Bind pipeline/VB/IB/desc/VP/scissor | Mid-frame flush on ring exhaust |
| DrawIndexed | Present submit + retire |

## Historical experiments (do not re-enable blindly)

| Experiment | Outcome |
|---|---|
| `SCR_A4N_DESC_CACHE` (map of xf+views) | TR1 load/outdoor hangs — **default off** |
| `SCR_A4M_PRESENT_CHAIN` async | Hangs — default off |
| A4h unbounded EFB queue | DEVICE_LOST |
| A4n `0x680` + full-slot XF bind | Shipped safe |

A5l sticky reuse is **last-draw only** within the current CB; invalidated on descriptor reset and EFB retire — not the A4n map cache.
