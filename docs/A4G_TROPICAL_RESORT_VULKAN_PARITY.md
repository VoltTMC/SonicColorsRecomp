# A4g — Tropical Resort Act 1 Vulkan Rendering Parity

**Status (2026-11-25):** Stage entry + capture workflow **established**. Material TR1 improvement via **versioned XF SSBO ring**. Depth-compare + cull/winding fixes shipped. **Full TR1 visual parity is not claimed** — residual upside-down/missing-sky/EFB-window defects remain vs D3D11.

**Starting baseline:** A4f `c7f4220` (EFB copies, mips, TEV).

## 1. A4f starting point

Vulkan: TEV GLSL/SPIR-V, 8 maps, EFB→texture (`pscopy` semantics), multi-mip uploads, A4d sync. Title MAE≈3.1. TR1 `stg110` marker missing under validation-slowed input.

## 2. Stage-entry procedure

**Root cause of missing A4f `stg110`:** `tools/run_nvidia_*` use `env -i` (strips `SCR_A2G_*`), and validation drops unique_xfb to ~10/s so guest-timed `a2g_tr1_enter.txt` misses the stage in short timeouts.

**Harness:** `tools/a4g_tr1_capture.sh` `{d3d11|vulkan|vulkan-val}`

```bash
tools/a4g_tr1_capture.sh d3d11
tools/a4g_tr1_capture.sh vulkan   # SCR_VK_DEVICE via wrapper (default 3080)

# Pass SCR_* *after* the wrapper:
tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 SCR_A2G_NATURAL_TRACE=1 \
  ./out/SonicColorsRecomp --skip-launcher --renderer vulkan --scale 1 --no-vsync \
  --nand out/test_nand --input-script tools/a2g_tr1_enter.txt --mute --music-debug
```

Requires `out/test_nand` from `tools/setup_a2g_test_nand.sh` (TR-unlocked save).

## 3. Evidence Tropical Resort Act 1 is reached

| Backend | Evidence |
|---|---|
| D3D11 ~60 unique_xfb | `A2g-Stage: entered 'stg110'` + `[MUSIC] stage context 'stg110'` |
| Vulkan (no val) | Same markers; Act 1 HUD (lives/rings/timer/score/boost/stars); `out/a4g_vk_shot_*.bmp` |
| Vulkan+validation | Reachable with long timeout; A4f short runs missed marker due to slowdown |

## 4. D3D11 reference capture workflow

`tools/a4g_tr1_capture.sh d3d11` → `out/a4g_d3d_tr1/`.  
Settings: scale 1, `--shots 60 --shots-start 1900`, same input/NAND.

**Note:** Absolute XFB indices skew across backends when menu speed differs. Prefer post-`stg110` shots. Same-index MAE without stage alignment is misleading.

## 5. GX lighting audit

| Feature | TR1 used? | D3D11 | Vulkan | Notes |
|---|---|---|---|---|
| Channel lighting / atten / spot | Yes | Full HLSL | GLSL port (A4e) | Generator present; residual gaps elsewhere |
| Materials / ambient | Yes | FillVSConstants | Same | |

## 6. GX fog audit

| Feature | TR1 used? | D3D11 | Vulkan | Notes |
|---|---|---|---|---|
| Fog fsel/proj/range | Likely outdoors | HLSL in PSKey | Matching GLSL | Cannot fully verify until world orientation/sky correct |

## 7. Overlay / UI audit

| Finding | Detail |
|---|---|
| HUD | Renders correctly in-stage on Vulkan |
| A4f “overlay” gap | Normal GX 2D draws — not a missing separate overlay path for TR1 HUD |
| Remaining | Title logos/bloom differ; world voids + orientation remain the parity gap |

## 8. Controlled tests

- `scr_efb_copy_tex_test` — format/mip + GX Z-mode bit encoding.
- `scr_topo_invalid_test`, `scr_shader_cache_test` OK.

## 9–10. Same-scene comparisons / metrics

| Capture | Path |
|---|---|
| Vulkan TR1 | `out/a4g_vk_tr1/`, `out/a4g_vk_shot_*.bmp` |
| D3D11 TR1 | `out/a4g_d3d_tr1/` |
| Compare samples | `out/a4g_compare/` |

**In-stage blackish pixel % (near-black RGB sum &lt; 24):**

| Build | Approx blackish |
|---|---|
| Pre–XF-ring (depth/cull only) | ~70–87% |
| Post–XF-ring | ~16–28% |

Title regression: safety MAE class unchanged from A4f (~3.1) when compared on title shots.

**Remaining visual classes:** world Y/orientation anomalies vs HUD; missing sky/distant fill; EFB-copy “windows”; residual stretch on some meshes. **Not** material pixel parity with D3D11 gameplay.

## 11. Implemented this milestone

1. **Versioned XF SSBO ring (primary TR1 fix)** — single host-visible XF buffer was overwritten while earlier draws in the same CB still referenced it → skeletal/env matrix corruption (“vertex explosion”). Ring of 16 KiB slots in a 16 MiB buffer; descriptors bind `offset`+`range`; reset after fence flush / present.
2. **GX depth compare** → Vulkan pipelines (`GfxDrawBatch.depthCompare` / `PipeKey`).
3. **Cull / front-face** for NDC Y flip: `FRONT_FACE_COUNTER_CLOCKWISE` + normal Back/Front map.
4. EFB-copy util **push constants**; fence-synced copies retained (inline batching → `DEVICE_LOST`).
5. Capture harness `tools/a4g_tr1_capture.sh`.

Negative-viewport experiment (instead of `clip.y` negate) did **not** clearly resolve orientation; retained A4e `clip.y = -clip.y`.

## 12. Vulkan validation

Title (val ON): **0** Validation Error, **0** DEVICE_LOST.  
TR1 visual (val OFF): **0** DEVICE_LOST; stg110 confirmed.  
Inline multi-copy experiment: DEVICE_LOST — reverted.

## 13. Performance (RTX 3080)

| Config | unique_xfb |
|---|---|
| D3D11 TR1/title | ~60 |
| Vulkan TR1 (no val) | ~5–6 in-stage (EFB-copy fence cost) |
| Vulkan title + val | ~9–11 (A4f class) |

## 14. Gameplay / sustained

Stage loads and runs on Vulkan with HUD + substantial world geometry after XF ring. Sustained millions of verts without device loss on fence-sync copy path. Visual correctness still incomplete.

## 15. Remaining unsupported / incomplete

- Correct world orientation / sky / full outdoor TR1 vs D3D11.
- Fog/lighting visual confirmation on outdoor TR1.
- EFB-copy throughput (per-copy fence) for interactive Vulkan TR1.
- Safe in-frame copy batching without GPU timeout.
- Full visual parity claim.

## 16–17. Deferred

- **AMD/RADV:** deferred / unverified.
- **Native Windows Vulkan GPU:** unverified (MinGW build only).
- Vulkan **not** default.

## Related

- `docs/A4F_EFB_COPIES_AND_TEXTURE_COMPLETENESS.md`, `A4E_…`, `A4D_…`
- `tools/a4g_tr1_capture.sh`, `tools/a2g_tr1_enter.txt`
