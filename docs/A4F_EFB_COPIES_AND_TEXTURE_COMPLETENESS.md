# A4f — EFB-to-Texture Copies & Complete Texture Handling

**Status (2026-11-25):** A4f **implemented** on the opt-in Vulkan path for title, deep menu, and Tropical Resort–class GX workloads on **NVIDIA RTX 3080**. D3D11 remains the default and correctness reference. This is **not** full GX or scene visual parity.

**Starting baseline:** A4e at `fb39843` (TEV GLSL/SPIR-V + multi-tex; EFB-copy logged unsupported; mip uploads limited).

## 1. A4e starting state and known limitations

A4e delivered shared `VSKey`/`PSKey` → GLSL → SPIR-V, eight texture maps, wrap/filter samplers, NDC Y fix, and clean NVIDIA validation. Remaining gaps called out for A4f:

- Vulkan EFB→texture copies unsupported (`NoteUnsupported`).
- Mipmap uploads limited to base level on Vulkan.
- Deeper menus / Tropical Resort Act 1 not fully exercised on Vulkan.

## 2. GX EFB copy state audit

Shared path (unchanged state machine):

- BP copy triggers → `DoEFBCopy()` in `runtime/gx/renderer.cpp`.
- Destination EA, source rect (`sx/sy/sw/sh`), format, intensity, depth-source, half-scale.
- D3D11: util `pscopy` HLSL (format/intensity/depth/half box filter) → cached RTV/SRV keyed by `EfbCopyKey(destAddr)`.
- Vulkan A4e: rejected / white fallback when sampling copy destinations.

Observed title/TR1-class workloads: thousands of EFB copies per minute (`efbCopy` counter); `unsup=0` after A4f.

## 3. Shared and backend-specific copy behaviour

| Layer | Role |
|---|---|
| Shared GX | Decode copy request; FlushVulkan builds `GfxEfbCopyRequest` |
| D3D11 | `pscopy` util draw into cached texture |
| Vulkan A4f | `VkGx_CopyEfbToTexture` — snap EFB colour/depth → util GLSL (`kCopyFragGlsl`, D3D `pscopy` semantics) → cache |

No second GX state machine.

## 4. Implemented copy formats and conversion semantics

Vulkan util frag matches D3D11 `pscopy`:

- Colour formats: R4/R8, RA4/RA8, RGB565 (α=1), A8, R8/G8/B8, RG8/GB8, default RGBA.
- Intensity (BT.601 studio Y + 16/255) when requested for fmt &lt; 4.
- Depth source: 24-bit packing into channel layouts (Z4/Z8/Z8M/Z8L/Z16/Z16L/Z24X8).
- Half-scale: destination size `sw/2 × sh/2` + 4-tap box filter (`iparams.w`).

Unsupported configs: log exact fmt/copy fields (counter `unsup`); title/TR1 runs reported **0**.

## 5. Scaling and copy-region handling

- Source UV from EFB rect over 640×528 logical EFB.
- Destination size respects half-scale; scaled by `internal_scale` for the Vulkan image.
- Snapshots isolate copy reads from later EFB attachment writes.

## 6. Texture upload formats

- RAM textures: `GXTex_Decode` → RGBA8 staging (same as D3D path), including CI/palette via TLUT.
- Vulkan image: `R8G8B8A8_UNORM`, sampled + transfer-dst.
- EFB-copy textures: bind via `GfxTexBind.efbCopyAddr` / `VkGx_LookupEfbCopy` (no RGBA decode).

## 7. Mipmap upload and sampling behaviour

- FlushVulkan uploads guest-supplied levels (`s_vkTexMip[8][16]`), dimensions via `GXTex_Size` offsets.
- No synthetic mip generation when the game provides levels.
- GX min_filter: levels when `minF >= 2` and `maxLod > 0`; `minLinear` / `mipFilter` mapped for near/lin × near-mip/lin-mip.
- Sampler LOD: when `levels > 1`, `maxLod` covers uploaded range (nearest or linear mip mode).

## 8. Texture cache identity and invalidation

- Ordinary tex cache key: addr|fmt|w|h|hasMips|map.
- EFB-copy cache: `EfbCopyKey(destAddr)`; gen id bumped each copy; overwrite reallocates when size changes.
- Guest CPU texture at a former copy addr → `VkGx_InvalidateEfbCopy`.
- SPIR-V / pipeline caches remain separate from texture identity (no DXBC reuse).

## 9. Resource lifetime and image-layout transitions

Preserves A4d: frame fences, staging ring drain, per-draw descriptors, 256-byte UBO align, no present-time `DeviceWaitIdle`.

Copy path: colour/depth attachment → transfer → shader-read snaps; dest colour-attachment → shader-read; scoped barriers only. Frame-slot fence wait around copy submit (not device-wide idle).

## 10. Controlled test results

`scr_efb_copy_tex_test` (`tools/efb_copy_tex_test/`):

- CPU reference for colour/depth conversion tables (pscopy-equivalent).
- Half-scale dest size; mip dimension chains for `minF` gating.
- VS GLSL with colour+alpha lighting compiles via `glslangValidator` (guards against HLSL `int3`/`float3` leak into GLSL).

Also: `scr_topo_invalid_test`, `scr_shader_cache_test` OK.

## 11. Title and menu results

RTX 3080, `tools/run_nvidia_vk_validation.sh`, `SCR_VK_DEVICE=3080`, scale 1:

| Metric | Title (~601 XFB) |
|---|---|
| Validation Error | **0** |
| SPIR-V fail | **0** (cold ~14 ok) |
| efbCopy / unsup | **8985 / 0** |
| Pipelines | **14** unique |
| unique_xfb | ~9–11 /s (validation on) |

EFB copies execute (no unsupported spam). A4e TEV/multi-tex intact.

## 12. Tropical Resort Act 1 / deep play

Input: `tools/a2g_tr1_enter.txt` + `out/test_nand` (TR-unlocked save). Validation on.

| Metric | Long run (e.g. frames≈1801–2101) |
|---|---|
| Validation / SPIR-V fail | **0 / 0** |
| efbCopy / unsup | **≥27k / 0** |
| mipUp | **40** |
| texUp | **73** |
| Draws / verts | ~0.9M–1.2M draws; tens of millions of verts |
| unique_xfb | ~9–10 /s |

`SCR_A2G_NATURAL_TRACE=1` (passed **inside** `run_nvidia_vk_validation.sh` via `env …`, because the wrapper uses `env -i`) confirmed natural EFB lifetime tracing. Explicit `A2g-Stage: entered 'stg110'` was **not** observed in the timed A4f validation runs (input script advances slowly under validation); workload shape (mip uploads + mass geometry after menu mash) matches deep stage/menu 3D content. **A4g** later reached `stg110` on Vulkan without validation (`tools/a4g_tr1_capture.sh`) — see `docs/A4G_TROPICAL_RESORT_VULKAN_PARITY.md`. **Not** claimed pixel-identical TR1 parity.

## 13. D3D11 versus Vulkan screenshot comparisons

- Settings: scale 1, `--shots 30 --shots-start 30`.
- Vulkan: `out/a4f_vk_shot_*.bmp` (640×480 EFB dump).
- D3D11: cwd `shot_*.bmp` (320×240 present path).
- Compare: bilinear-resize VK → D3D size; MAE on RGB.
- Safety-class frames **0003–0011**: MAE **≈3.10** (matches A4e ≈3.1 baseline after NDC Y fix).
- Later frames ~6.8 (page/timing skew). Frame 0001 high (~44) — boot timing mismatch.
- Samples: `out/a4f_compare/`.

Residual classes: existing TEV/raster precision, letterbox/resolution path difference, timing — not attributed solely to EFB-copy (copies now present on both backends).

## 14. Vulkan validation results

Khronos validation **ON** for title and long TR1-class runs: **0** `Validation Error`. No `DEVICE_LOST` in tested NVIDIA runs.

## 15. Performance measurements (RTX 3080)

| Config | unique_xfb |
|---|---|
| Vulkan + validation, title | ~9–11 /s (A4e class ~11–12) |
| Vulkan + validation, deep play | ~9–10 /s |
| D3D11 default, title | **~60** /s |

EFB copy uses per-copy fence wait on the frame slot (correctness-first). No guest timing/audio changes. No major validation-on regression vs A4e class.

## 16. Remaining unsupported / incomplete

- Full fog / lighting edge-case parity beyond generator coverage.
- Overlay completeness.
- Exact TR1 visual parity vs D3D11 (not claimed).
- Some exotic depth-copy consumers / cross-frame feedback edge cases may still differ.
- Vulkan not default.

## 17. Explicitly deferred

- **AMD/RADV:** deferred / unverified.
- **Native Windows Vulkan GPU:** unverified (MinGW build only).

## Related

- `docs/A4E_GX_TEV_AND_MULTITEXTURE_PARITY.md`, `A4D_VULKAN_STABILITY_AND_RADV.md`, `A4C_VULKAN_GX_DRAW_PIPELINE.md`, `A4B_NATIVE_VULKAN_RENDERER.md`
- Tools: `run_nvidia_clean.sh`, `run_nvidia_vk_validation.sh`, `scr_efb_copy_tex_test`
