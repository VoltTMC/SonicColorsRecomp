# A4b — Native cross-platform Vulkan renderer

**Status (2026-11-24):** **Stage A complete** (device / surface / swapchain / clear-present).  
GX draw translation, EFB, SPIR-V cache, and gameplay default remain **in progress** (Stages B–F).  
Gameplay default is still **D3D11** (Linux: DXVK; Windows: native D3D11).

## 1. Original architecture

```
Wii GX FIFO → gx_fifo / gx_state → renderer.cpp (D3D11)
                                    ├─ HLSL (shadergen) → DXBC (.cso A3w/A3z)
                                    ├─ ID3D11* resources / Flush bind-cache (A3x Topo::Invalid)
                                    └─ video.cpp Present (DXGI swapchain)
Linux: D3D11 → DXVK → Vulkan → GPU
Windows: D3D11 → vendor D3D11 → GPU
```

Shared GX pieces (must stay shared): FIFO decode, BP/CP/XF regs, vertex loader, texture decode, shader keys/generation, EFB semantics.

## 2. Target architecture

```
Wii GX FIFO → shared GX translation → Gfx backend interface
                                         ├─ D3D11 backend (reference / fallback)
                                         └─ Vulkan backend (native Vk*; Linux + Windows)
                                              ├─ SPIR-V modules (A3w-extended cache)
                                              └─ VkPipelineCache (device-specific)
```

## 3. Stage A delivered

| Piece | Location / CLI |
|---|---|
| Backend selection | `runtime/gx/gfx_backend.{h,cpp}` — `--renderer d3d11\|vulkan`, `SCR_RENDERER=` |
| Vulkan device + swapchain | `runtime/gx/vk/vk_device.{h,cpp}` — dynamic loader (no Vulkan SDK required at runtime) |
| List GPUs | `--list-vulkan-devices` |
| Clear + present smoke | `--vk-smoke [N]` (default 60 frames) |
| Device preference | `SCR_VK_DEVICE` substring (e.g. `3080`, `RAPHAEL`) |
| Image count | Prefer **3** images when supported (A4a BufferCount lesson); mailbox/immediate when VSync off |

Proven (this machine):

- `--list-vulkan-devices` → RADV RAPHAEL_MENDOCINO (`1002:164e`) + RTX 3080 (`10de:2206`)
- `tools/run_amd_clean.sh ./out/SonicColorsRecomp --vk-smoke 30` → native RADV, mailbox, **not DXVK**
- `tools/run_nvidia_clean.sh ./out/SonicColorsRecomp --vk-smoke 30` → native NVIDIA Vulkan

`--renderer=vulkan` for full gameplay currently **FATAL**s with a clear Stage A message (avoids a silent broken path).

## 4. Coupling still on D3D11

`runtime/gx/renderer.cpp` still owns ~180+ `g_device`/`g_ctx` call sites. Stages B–C move Flush / EFB / textures / Present onto Vulkan while keeping GX decode shared.

## 5. SPIR-V / pipeline cache (Stage D — design)

- DXBC `.cso` remains the D3D11 portable cache.
- Vulkan needs a parallel SPIR-V cache (schema + generator id + SPIR-V env).
- `VkPipelineCache` is **device-specific** (vendor/device/driver UUID) — never ship RTX blobs as universal for RADV.

Compiler path under evaluation: DXC HLSL→SPIR-V and/or adapted generator + `glslangValidator` (present on host). Not wired for gameplay yet.

## 6. Stages remaining

| Stage | Goal |
|---|---|
| B | Vertex/index/textures/descriptors + GX Flush → Vulkan draws |
| C | EFB colour/depth, copies, TR1 visual parity vs D3D11 |
| D | SPIR-V compile + auto-load; VkPipelineCache |
| E | Profile vs A4a D3D11/DXVK baselines (610M + 3080, scales 1–3) |
| F | Default Vulkan on Linux + Windows; D3D11 explicit fallback |

## 7. Build notes

- Vulkan **headers** via FetchContent (Linux + Windows).
- **Loader**: Linux `SDL_Vulkan_*`; Windows `vulkan-1.dll` (driver).
- End users do not need the Vulkan SDK — only a working GPU driver with Vulkan.

## 8. Correctness anchors (must survive)

- A3x `Topo::Invalid` topology invalidation
- Full XF uploads until independently validated
- A3w/A3z DXBC path for D3D11
- A3v remains benched/off
- A2f EFB exact-match before declaring Stage C done
