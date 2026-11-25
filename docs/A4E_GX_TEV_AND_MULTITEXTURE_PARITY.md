# A4e — GX TEV Shader Generation & Multi-Texture Parity

**Status (2026-11-24):** A4e **implemented** on the opt-in Vulkan path for title-class GX workloads on **NVIDIA RTX 3080**. D3D11 remains the default and correctness reference. This is **not** full GX visual parity.

**Starting baseline:** A4d at `c21c09a` (validation-clean NVIDIA sync); tree HEAD before A4e work included v0.4.0 metadata (`6e01cd5`).

## 1. Starting architecture and A4d baseline

Shared GX FIFO decode → `Flush()` → D3D11 (full HLSL TEV via `VSKey`/`PSKey`) or Vulkan Stage B (fixed embedded SPIR-V, tex map 0 only).

A4d preserved: Khronos validation, per-image `renderFinished`, 256-byte dyn UBO alignment, fence retirement, `Vk_WaitLastSubmit`, no `vkQueueWaitIdle`.

## 2. GX state audit (pre-A4e)

| Area | Shared / D3D11 | Vulkan Stage B (A4c) |
|---|---|---|
| BP / XF / TEV stages | Full `BuildPSKey` / `BuildVSKey` | Keys built but unused |
| TEV combiners / konst / ras / swap | HLSL `GeneratePixelShader` | Fixed modulate frag |
| Multi-texture maps | 8 SRVs from `usedMaps` | tex0 only |
| Texgen / dual-tex / lighting | Full VS gen | Simplified embedded VS |
| Indirect texturing | In HLSL path | Not used |
| EFB→texture | D3D path | `NoteUnsupported` (deferred) |
| Descriptors | D3D bind | 1 combined sampler + 16-byte PS flags |

**IR choice:** Reuse existing `VSKey` / `PSKey` / `PSConstants` / `VSConstants` as the backend-independent TEV representation (no second GX state machine).

## 3. Backend-independent TEV representation

`GfxDrawBatch` (`runtime/gx/gfx_draw.h`) now carries:

- `vsKey` / `psKey` / `hasShaderKeys`
- `vs` / `ps` constant buffers
- `tex[8]` + `usedMaps` (per-map RGBA decode, wrap/filter)

`FlushVulkan()` fills these from the same `BuildVSKey` / `BuildPSKey` / `FillPSConstants` / `GXTex_Decode` path used for D3D11 semantics.

## 4. Vulkan shader-generation architecture

```
VSKey/PSKey → Generate*ShaderGLSL (shadergen_glsl.*)
           → VkSpv_CompileGlsl (glslangValidator + disk cache under shadercache_vk/)
           → VkShaderModule cache (keyed by key hash)
           → graphics pipeline cache (PipeKey = hashes + topo/cull/blend/depth)
```

- GLSL 450; descriptor layout: XF SSBO(0), VS UBO(1), PS UBO(2), `sampler2D tex[8]`(3).
- Vulkan NDC Y negation after GX projection (`clip.y = -clip.y`) to match D3D11 EFB orientation.
- DXBC / A3w cache is **not** reused for SPIR-V.
- Requires `glslangValidator` on PATH (or `SCR_GLSLANG`) for cache misses.

## 5. Supported TEV operations and stage limits

Ported from the D3D11 HLSL generator: up to hardware stage counts in `PSKey` (16 colour/alpha combiners, 8 texgens, ind stages, konst, ras, swap tables, bias/scale/clamp, compare ops, fog/z-tex fields present in key/UBO).

**Not claimed complete:** every fog/z-tex/indirect edge case under all game materials; dual MRT alpha export (D3D depth-copy helper) omitted on Vulkan.

## 6. Multi-texture descriptor and sampling

- Eight combined image samplers per draw; unused maps → 1×1 white + nearest clamp.
- Per-stage map/coord selection comes from generated TEV (not forced to unit 0).
- Sampler cache from GX wrap/filter bits (clamp/repeat/mirror × nearest/linear).
- RAM textures via `GXTex_Decode`; EFB-copy textures completed in **A4f** (`docs/A4F_EFB_COPIES_AND_TEXTURE_COMPLETENESS.md`).

## 7. Texture-coordinate generation

Full VS GLSL texgen port: matrix / emboss / colour sources, dual-tex post-matrix, projective divide, per-stage selection in PS.

## 8. Cache design

| Cache | Key | Notes |
|---|---|---|
| Disk SPIR-V | Source hash of GLSL (`shadercache_vk/*.spv`) | Separate from DXBC |
| Module | `VSKey`/`PSKey` bytes hash | In-memory |
| Pipeline | module hashes + raster state | Avoids collisions across TEV configs |

Stats: `shaderCacheHits/Misses`, `spvCompileOk/Fail`, pipeline hit/miss (logged every 300 XFB).

## 9. Controlled / game tests (RTX 3080)

Launch: `tools/run_nvidia_vk_validation.sh` + `SCR_VK_DEVICE=3080` + `--skip-launcher --renderer vulkan --scale 1`.

| Case | Result |
|---|---|
| Title boot / health & safety UI | Textured TEV draws; screenshots via `VkGx_SaveEfbBmp` |
| SPIR-V compile | Cold: **12 ok / 0 fail**; warm: disk+module hits |
| Pipelines | **14** unique; hit rate ≫ miss |
| Draws / XFB | e.g. frames=301 draws≈21k; sustained frames=601 draws≈42k |
| Unsupported (A4e) | `efb-copy-to-texture` (expected then; fixed in A4f) |
| Khronos validation | **0** `Validation Error` in A4e title runs |

## 10. D3D11 versus Vulkan screenshots

Settings: scale 1, `--shots 30 --shots-start 30`; Vulkan BMPs under `out/a4e_vk_shot_*.bmp`; D3D under `shot_*.bmp`; copies in `out/a4e_compare/`.

After Vulkan Y-fix, resized MAE(title safety frame ≈shot 0010) **≈3.1** (was ≈16 with vertical flip). Residual: resolution/letterbox, EFB-copy gaps, timing of multi-page safety screens, precision.

**Materially improved** vs A4c tex0-only modulate; **not** pixel-identical parity.

## 11. Game scene validation

| Scene | Vulkan A4e |
|---|---|
| Title / safety screens | Renders with multi-tex TEV; compared to D3D11 |
| Main menus / TR1 | Not fully walked in this milestone; deferred deeper runs |

## 12. Performance (RTX 3080, scale 1, validation ON)

- unique_xfb ≈11–12 /s (validation overhead; same class as A4d val-on).
- First-frame shader compile pays glslangValidator; later hits disk cache.
- A4d sync model unchanged (no device-idle on present).

## 13. Remaining unsupported GX features

- Complete lighting/fog parity beyond what the generator already emits
- Overlay completeness
- EFB→texture / multi-mip: see **A4f**
- AMD/RADV; native Windows Vulkan GPU

## 14. Known visual differences / bugs

- MAE ~3 on compared title frames after Y-fix (not zero).
- `--shots` D3D path writes cwd `shot_*.bmp`; Vulkan writes `out/a4f_vk_shot_*.bmp` (was `a4e_vk_shot_*` in A4e).

## 15. Explicitly unverified

- **AMD/RADV:** deferred / unverified.
- **Native Windows Vulkan GPU:** unverified (MinGW build only).

## Related

- `docs/A4F_EFB_COPIES_AND_TEXTURE_COMPLETENESS.md` (successor EFB-copy / mip work)
- `docs/A4D_VULKAN_STABILITY_AND_RADV.md`, `docs/A4C_VULKAN_GX_DRAW_PIPELINE.md`, `docs/A4B_NATIVE_VULKAN_RENDERER.md`
- Tools: `run_nvidia_clean.sh`, `run_nvidia_vk_validation.sh`
