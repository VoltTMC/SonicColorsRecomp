# A4h — Tropical Resort Outdoor Rendering & Vulkan Visual Parity

**Status (2026-11-25):** Material outdoor improvement — **EFB-copy NDC Y** fixed so TR1 world is upright with HUD. Region snap transfers. XF ring diagnostics. **Full TR1 outdoor parity not claimed** (sky voids, residual mesh holes, fog/lighting visual gaps).

**Baseline:** A4g `3db912d` (XF SSBO ring, depth-compare, cull/winding, stg110 entry).

## 1. A4g baseline & remaining defects

| Defect | A4g state | A4h |
|---|---|---|
| XF vertex explosion | Fixed (ring) | Preserved + drain recycles arenas |
| World upside-down vs HUD | Open | **Fixed** (EFB-copy util Y) |
| EFB-copy “windows” | Flipped / wrong | Orientation corrected; some residual artefacts |
| Sky / black voids | Open | Still open (~19% blackish vs D3D ~0.3%) |
| Fast in-frame copies | Fence per copy | Inline queue → DEVICE_LOST; kept sync + **region** blit |
| Mesh holes | Open | Partially improved with orientation; residual gaps |

## 2. Capture procedure

```bash
tools/a4h_tr1_capture.sh d3d11
tools/a4h_tr1_capture.sh vulkan
tools/a4h_tr1_capture.sh compare
# SCR_* after env -i wrappers:
tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 SCR_A2G_NATURAL_TRACE=1 \
  SCR_VK_SHOT_PREFIX=out/a4h_vk_shot \
  ./out/SonicColorsRecomp --skip-launcher --renderer vulkan --scale 1 --no-vsync \
  --nand out/test_nand --input-script tools/a2g_tr1_enter.txt --mute --music-debug \
  --shots 40 --shots-start 1800
```

Timeout no longer aborts post-copy of screenshots (`set +e` around `timeout`).

## 3. Scene matching

- Same input script + NAND; post-`stg110` shots preferred.
- Absolute XFB indices still skew (D3D ~30–60 unique_xfb vs VK ~5).
- Equivalence: stage marker + landmark (Act 1 banner, opening platform, early walkway) + timer HUD.
- Outputs: `out/a4h_d3d_tr1/`, `out/a4h_vk_tr1/`, `out/a4h_compare/`.

## 4. Outdoor orientation / sky

**Finding:** Full-frame vertical flip of A4g shots made the **world** look correct while inverting the HUD → visible outdoor meshes were largely **Y-flipped EFB-copy content**, not a global VS `clip.y` bug.

**Cause:** Copy util used D3D rect `{-1,1,1,-1}` (Y+ up). Vulkan NDC Y+ is down → dest textures flipped vs D3D.

**Fix:** Vulkan copy push-constant rect `{-1,-1,1,1}`. A4e `clip.y = -clip.y` and A4g cull/CCW unchanged.

**Sky:** Still a solid black void on Vulkan outdoor captures while D3D fills sky (~0.3% blackish late_half). Not explained by copy Y alone — likely missing/culled skydome or background pass. Left open.

**A4i/A4j follow-up:** Magenta-clear probe showed voids were **uncovered EFB clear**. A4j fixed Vulkan CW front-face cull — sky/planet restored (`docs/A4J_TROPICAL_RESORT_SKY_COVERAGE.md`).

## 5. XF SSBO ring audit

- Ring 16 MiB / 16 KiB slots preserved.
- `DrainFrameSlot` now resets desc/dynUbo/XF arenas after fence (no wrap into live ranges).
- Stats: `xfUploads`, `xfRingHighWater`, `xfRingWraps`, `xfRingBytes`.
- TR1: `xfHW` reaches capacity under per-copy drains; no return of A4g vertex explosion.
- Stress: sustained stg110, millions of verts, 0 DEVICE_LOST on sync path.

## 6. EFB-copy ordering & in-frame

| Approach | Result |
|---|---|
| Inline copies in draw CB (no wait) | **DEVICE_LOST** (TDR from thousands of full-EFB blits/CB) — **A4l** reintroduces *bounded* region-snap inline copies with soft-submit budget; see `A4L_VULKAN_EFB_COPY_SYNCHRONISATION_AND_PERFORMANCE.md` |
| Fence-synced + **region** color/depth snap | Stable; GX order preserved via drain-before-snap |
| Vulkan copy Y rect | Orientation match vs D3D |

Per-copy `vkDeviceWaitIdle` not used; per-copy fence wait retained. “Fast in-frame” without TDR remains future work (e.g. semaphore-chained submits or lighter snaps).

## 7. Residual mesh defects

| Class | Notes |
|---|---|
| Orientation | Resolved for EFB-sampled outdoor content |
| Sky / void holes | Open |
| Character “shell” / depth on some intro frames | Residual; needs dedicated mesh RCA |
| Topology | `Topo::Invalid` / `scr_topo_invalid_test` unchanged |

## 8. Lighting / fog audit

Shared GLSL lighting/fog left unchanged (no evidence they caused the upside-down world). Outdoor fog still not visually confirmed (harsh edge into black sky dominates).

## 9–11. Captures & metrics

| Path | Role |
|---|---|
| `out/a4h_vk_tr1/` | Vulkan TR1 shots + `run.log` + `meta.txt` |
| `out/a4h_d3d_tr1/` | D3D11 TR1 shots + log |
| `out/a4h_compare/` | PNG exports (`vk_0055.png` upright outdoor, etc.) |

**Blackish metric (unchanged definition: RGB sum &lt; 24):**

| Set | late_half mean |
|---|---|
| A4g in-stage ref | ~16–28% |
| A4h Vulkan late_half | **~19.3%** |
| A4h D3D11 late_half | **~0.3%** |

Numeric blackish did not drop below A4g because **sky voids** still dominate. Qualitative outdoor orientation is materially better (world upright with HUD).

## 12. Validation

Title + Khronos validation: **0** Validation Error / DEVICE_LOST.  
TR1 visual (no val): stg110, sync copies, 0 DEVICE_LOST.  
Inline experiment: DEVICE_LOST — documented, reverted.

## 13. Performance (RTX 3080)

| Config | unique_xfb |
|---|---|
| D3D11 TR1 | ~30–60 |
| Vulkan TR1 (sync region copies) | ~5 |
| Title + validation | ~9–11 class |

## 14. Remaining / unsupported

- Sky / background fill on Vulkan outdoor TR1.
- True in-frame copies without TDR.
- Residual mesh holes; fog/lighting outdoor sign-off.
- Full TR1 visual parity.

## 15–16. Deferred

- **AMD/RADV:** deferred / unverified.
- **Native Windows Vulkan GPU:** unverified.
- Vulkan **not** default.

## Related

- `docs/A4G_TROPICAL_RESORT_VULKAN_PARITY.md`, `A4F_…`, `A4E_…`
- `tools/a4h_tr1_capture.sh`
