# A4d — Native Vulkan Stability & Synchronisation (NVIDIA scope)

**Status (2026-11-24):** NVIDIA RTX 3080 A4d stability pass **complete** for the tested real-GX workloads.  
**AMD/RADV:** **unverified and deferred** (explicitly out of this iteration’s test scope).  
**D3D11:** remains default and independent. Vulkan remains opt-in (`--renderer=vulkan` / `SCR_RENDERER=vulkan`).

Starting commit: `1c62eef` (A4c Stage B).

## Updated test scope

| Item | Value |
|---|---|
| GPU | NVIDIA GeForce RTX 3080 (`device=2206`) |
| Driver / API | NVIDIA 620.73.4 / Vulkan 1.4.357 |
| Launch | `tools/run_nvidia_clean.sh` + `SCR_VK_DEVICE=3080` |
| Validation launch | `tools/run_nvidia_vk_validation.sh` (local `out/vvl` Khronos layer) |
| AMD/RADV | **Deferred / unverified** — do not treat as fixed |

## How counts are collected

| Metric | Source | Meaning |
|---|---|---|
| `frames=` in `vulkan gx:` logs | `g_framesPresented++` after successful `VkGx_PresentXfb` | Successful XFB present calls (not unique visual FPS) |
| `draws=` / `verts=` / `idx=` | `VkGxStats` incremented in `VkGx_Draw` | Submitted `vkCmdDrawIndexed` batches |
| `unique_xfb` (status line) | Perf path | Unique XFB copies/sec estimate — **not** claimed as visual parity |
| `presentSerial` | `Vk_PresentSerial()` | Swapchain present counter inside `vk_device` |

Submitted draws ≠ presented frames ≠ unique visual frames.

## Synchronisation model (post-A4d)

```
GX thread:  Draw → record into frame CB (VB/IB/dynUBO/descriptors)
Present:    hold s_mu → flush draw CB (submit + fence wait)
            → dedicated s_presentCmd: EFB→TRANSFER_SRC, blit, PRESENT_SRC
            → acquire (per-frame imageAvailable[fi] + inFlight[fi])
            → submit wait imageAvailable[fi], signal renderFinished[imageIndex]
            → present wait renderFinished[imageIndex]
            → WaitLastSubmit (inFlight fence for that submit)
            → reset descriptor pool / dynUBO / staging cursors
```

### Objects

| Object | Lifetime |
|---|---|
| `imageAvailable[3]` | Per frame-in-flight; waited by graphics submit |
| `renderFinished[N]` | **Per swapchain image** (validation-required); waited by present |
| `inFlight[3]` | Per frame-in-flight submit fence |
| `s_presentCmd` | Dedicated present CB (not shared with GX draw recording) |
| Dyn UBO / staging rings | Host-coherent; cursors reset after submit fence retirement |
| Descriptor sets | One fresh set per draw; pool reset only after GPU retirement |

### Defects found under Khronos validation (and fixed)

1. **PS UBO offset misaligned** — `vsOff + sizeof(VSConstants)` (160) was not a multiple of `minUniformBufferOffsetAlignment` (64). Both VS and PS offsets now 256-aligned.
2. **`renderFinished` reused while still in use by swapchain** — was keyed by frame slot; now keyed by **swapchain image index**.
3. Earlier A4d (pre-validation) fixes retained: swapchain `TRANSFER_DST`, present holds GX mutex, dedicated present CB, per-draw descriptors, staging ring.

## Validation-layer status

- System package `vulkan-validation-layers` was not installed.
- A4d uses a **local** Khronos layer staged at `out/vvl/` (from host UE Vulkan redistributable) via `VK_LAYER_PATH`.
- `tools/run_nvidia_vk_validation.sh` sets `SCR_VK_VALIDATION=1`, enables `VK_LAYER_KHRONOS_validation`, and disables unrelated implicit layers.
- Log proof: `vulkan: KHronos validation layer + debug_utils active` and `validation=true`.

### Results (RTX 3080)

| Run | Validation | XFB presents | Draw batches | Validation errors |
|---|---|---|---|---|
| `a4d_vk_nvidia_val2` (~90 s) | **ON** | ≥901 | ≥62k | **0** |
| restart ×3 (`a4d_vk_nvidia_restart{1,2,3}`) | **ON** | ≥301 each | ≥20k each | **0** |
| `a4d_vk_nvidia_sustained` (~180 s) | off (throughput) | **6001** | **4.12M** | n/a (no present failures) |

Prior validation-on run (`a4d_vk_nvidia_val1`) caught the UBO/semaphore defects above before the fixes.

## Remaining `vkDeviceWaitIdle` / `vkQueueWaitIdle`

| Site | Remains? | Reason |
|---|---|---|
| After XFB present | **No** — replaced by `Vk_WaitLastSubmit()` | Submit fence retires blit CB; rings only used by already-waited draw CBs |
| Mid-frame VB/IB / staging wrap | Fence wait only | Same |
| Swapchain destroy / `Vk_Shutdown` / failed paths | **Yes** | Tear-down must drain presentation engine and all queues |
| `vkQueueWaitIdle` | **Not used** | — |

## Resource lifetime audit (summary)

- Command buffers: reset only after fence wait (`EnsureFrameRecording`, present CB reset after prior submit retired).
- Semaphores: acquire per-frame; present-wait per-image (see above).
- Rings: wrap drains CB with fence before overwrite; host-coherent memory (no explicit flush required).
- Descriptors: no update-after-bind of a bound set; pool reset post-retirement.
- EFB/swapchain layouts: attachment ↔ transfer ↔ present barriers on the present CB.
- Shutdown: device wait idle before destroying swapchain/device.

## Performance notes

- Validation-on title ≈11–12 unique_xfb/s (layer overhead).
- Validation-off sustained ≈30 unique_xfb/s at scale 1 (same class as prior A4c NVIDIA runs; not a parity claim).
- Replacing post-present `DeviceWaitIdle` with submit-fence wait removes a full-device stall per XFB; no claim of higher unique visual FPS beyond measured unique_xfb.

## D3D11 regression

`tools/run_nvidia_clean.sh` + `--renderer d3d11`: backend `d3d11`, DXVK path healthy, ~60 unique_xfb / present_calls on title (`a4d_d3d11_nvidia.log`). Default selection unchanged.

## Builds / tests

| Check | Result |
|---|---|
| Linux `SonicColorsRecomp` | OK |
| MinGW `out-windows/SonicColorsRecomp.exe` | OK |
| `scr_topo_invalid_test` | OK |
| Native Windows Vulkan GPU | **UNVERIFIED** |
| AMD/RADV | **UNVERIFIED / deferred** |

## Explicitly not claimed

- Visual parity with D3D11
- Full TEV / lighting / EFB-copy / overlay *(TEV/multi-tex in A4e; EFB-copy/mips in A4f; TR1 entry/XF ring in A4g — parity incomplete)*
- Vulkan as default renderer
- AMD reliability

## Related

- `docs/A4F_EFB_COPIES_AND_TEXTURE_COMPLETENESS.md` (EFB-copy / mip successor; preserves A4d sync)
- `docs/A4E_GX_TEV_AND_MULTITEXTURE_PARITY.md` (TEV/multi-tex; preserves A4d sync)
- `docs/A4B_NATIVE_VULKAN_RENDERER.md`
- `docs/A4C_VULKAN_GX_DRAW_PIPELINE.md`
- `tools/run_nvidia_clean.sh`
- `tools/run_nvidia_vk_validation.sh`
