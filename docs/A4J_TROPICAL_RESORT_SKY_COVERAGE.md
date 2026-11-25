# A4j — Tropical Resort Sky Coverage & Background Draw Correctness

**Status (2026-11-25):** Root cause of TR1 uncovered sky **identified and fixed**. Vulkan late_half blackish moves from A4i ~21.3% to **~0.5%** (D3D reference ~0.2–0.3%). Planet/starfield/sky fill restored across tested camera views. Full outdoor lighting/fog parity is **not** claimed.

**Baseline:** A4i `8cc4aa6` (magenta-clear = uncovered EFB; fog-range X; EFB RAM-hash).

## 1. A4i starting state

| Item | A4i |
|---|---|
| Sky voids | Uncovered EFB clear (~50% magenta in top-third) |
| late_half VK / D3D | ~21.3% / ~0.3% |
| Sky top-third blackish | ~57.7% |
| Geom late blackish | ~3.2% |
| Planet / stars | Missing on Vulkan |
| Candidate bg draws | Submitted, but pixels stayed clear |

## 2. Matched-scene capture

- Script: `tools/a4j_tr1_capture.sh` (`d3d11` / `vulkan` / `compare`)
- Input: `tools/a2g_tr1_enter.txt`, `--shots 40 --shots-start 1800`
- Launch: `tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 …`
- Outputs: `out/a4j_vk_tr1/`, `out/a4j_d3d_tr1/`, `out/a4j_compare/`
- Magenta diagnostic: `SCR_A4I_MAGENTA_CLEAR=1` (optional; default rendering unchanged)

## 3. Candidate background draws

Post-`stg110` outdoor draws (fog / large tex / no Z-write) share:

- `projType=0` or ortho post, `flipY=+1`, full 640×480 viewport/scissor
- Mix of `GfxCull::Back` (GX cull=1) and Front/None
- Far world verts and large RAM textures (`efb=0`)
- Same batches submitted on D3D11 and Vulkan

## 4. D3D11 / Vulkan submission

Both backends receive the same `GfxDrawBatch` (shared Flush). Draw counts and GX state match; divergence was in Vulkan **raster front-face / cull interpretation**, not missing draws.

## 5. Transform / clip-space

- Shared projection + `clip.xy *= flip`; GLSL keeps `clip.y = -clip.y` for Vulkan NDC Y **position** parity (A4e/A4h).
- Clip-space bounds of candidate draws were not the primary void mechanism once cull was corrected.
- XF SSBO ring (A4g) left intact; no mid-CB overwrite / vertex-explosion regression.

## 6. Raster coverage (root cause)

**A4g set `VK_FRONT_FACE_COUNTER_CLOCKWISE`** to “compensate” for `clip.y = -clip.y`, with normal Back→BACK mapping.

**Effect:** With clockwise GX/D3D fronts still reaching the rasterizer as CW, CCW-as-front made **cull-back discard the intended front faces**. Sky/background (and other solid meshes) never shaded → pixels retained EFB clear (magenta probe).

D3D11 uses `FrontCounterClockwise = FALSE` (CW front) + normal cull. Vulkan must match that.

Probes already ruled out fog / global no-cull / no-depth / no-alpha-test / no-EFB-tex as the primary void cause (A4i).

## 7. XF / matrix audit

No XF version/range defect found for the sky batches. Ring drain/recycle and high-water behaviour preserved. Fix does not touch XF upload.

## 8. Planet / background object

After the cull fix, the TR1 planet, starfield, and distant fill render in matched outdoor views. `NO_FOG` was never required for coverage.

## 9. EFB-copy dependency

Sky fill does **not** depend on a broken EFB-copy for the clear region. A4i RAM fingerprint + CPU decode on hash miss retained. In-frame full-EFB queue path **not** reintroduced.

## 10. Exact correction

1. **Vulkan front-face = `VK_FRONT_FACE_CLOCKWISE`** (match D3D11).
2. **Cull map:** `GfxCull::Back → BACK`, `Front → FRONT` via `GfxCullHwMode` (no swap).
3. Keep GLSL `clip.y = -clip.y` for position parity.
4. Secondary: GX BP 0x41 blend factors + color write mask on Vulkan (parity with D3D `GetBlendState`).
5. Helpers: `GfxCullFromGx` / `GfxCullHwMode` in `gfx_draw.h`; regression `scr_cull_winding_test`.

## 11. Before / after metrics

Unchanged definition: RGB sum &lt; 24; `late_half` = mean of second half of shot list.

| Metric | A4i VK | A4j VK | A4j D3D |
|---|---:|---:|---:|
| late_half blackish | ~21.3% | **~0.5%** | ~0.2% |
| sky top-third blackish | ~57.7% | **~0.9%** | ~0.5% |
| geom bot two-thirds | ~3.2% | **~0.3%** | ~0.0% |
| sky magenta (clear probe) | ~50–52% | **~2.1%** | n/a |

Blend-only intermediate (before cull fix): late_half ~14.8%, sky ~39.6%, magenta still ~52% — confirmed cull was the coverage killer.

## 12. Screenshots

| Path | Role |
|---|---|
| `out/a4j_vk_tr1/` | Vulkan TR1 after fix |
| `out/a4j_d3d_tr1/` | D3D11 reference (late outdoor) |
| `out/a4j_compare/vk_*.png` | Converted VK frames |
| `out/a4j_compare/d3d_*.png` | Converted D3D frames |
| `out/a4j_compare/cullfix_late.png` | Planet + sky after CW fix |
| `out/a4j_compare/mag_after_cull.png` | Magenta probe after fix |
| `out/a4i_*` | Preserved A4i baseline |

## 13. Validation

- Title + validation launcher: **no** `Validation Error` / `DEVICE_LOST` in tested run.
- `scr_topo_invalid_test`, `scr_shader_cache_test`, `scr_efb_copy_tex_test`, `scr_cull_winding_test`: OK.
- XF ring behaviour unchanged.

## 14. Performance (RTX 3080)

| Config | unique_xfb (approx) |
|---|---|
| Vulkan TR1 (no val) | ~4.9–5.2 (A4i class; EFB-copy fence bound) |
| D3D11 TR1 | ~30–40 in capture window |

Correctness prioritized; no device-wide idle waits added.

## 15. Remaining visual differences

- Outdoor fog / lighting: **A4k** audited TR1 fogTypes `0x00`/`0x02`/`0x0a` + generator parity; pixel-locked MAE still not claimed — see `A4K_TROPICAL_RESORT_VISUAL_SIGNOFF.md`.
- Occasional post/UI timing differences vs D3D (A4k: largely capture/FPS phase, not missing overlay).
- Early-shot blackish spikes during fades/loads (exclude from late_half claims).
- AMD/RADV: **deferred, unverified**.
- Native Windows Vulkan GPU: **unverified**.

## 16. Preserve

D3D11 remains default and independent. A4d–A4i sync, XF ring, EFB-copy Y `{-1,-1,1,1}`, fog-range `gl_FragCoord.x`, RAM fingerprint retained.
