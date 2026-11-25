# A4k — Tropical Resort Visual Sign-off & Rendering Consistency

**Status (2026-11-25):** Expanded TR1 scene coverage vs D3D11; fog/lighting **audited** for configs actually used outdoors (no generator rewrite); A4j CW cull/sky coverage **preserved**; D3D `--shots` capture alignment fixed (`SCR_SHOTS_FULL=1`); VK outdoor unique_xfb still ~4.9–5.2 (sync EFB-copy bound). Full game / all-camera pixel parity **not** claimed. AMD/RADV **deferred**. Native Windows Vulkan **unverified**. I1 Windows input **out of scope**.

**Baseline:** A4j `e87f431` (CW front-face + `GfxCullHwMode`; late_half ~0.5%). First A4k land `04861f7`; follow-up capture-parity fix on `main`.

## 1. A4j starting state (preserved)

| Item | A4j after fix |
|---|---|
| Front face | `VK_FRONT_FACE_CLOCKWISE` |
| Cull | Normal Back/Front via `GfxCullHwMode` |
| late_half blackish (VK) | ~0.5% |
| Sky top-third blackish | ~0.9% |
| Magenta clear coverage | ~2.1% |
| Planet / background | Restored |

A4k did **not** change cull, blend, write-mask, EFB-copy Y, XF ring, or fog-range `gl_FragCoord.x`. `scr_cull_winding_test` still passes.

## 2. Expanded scene capture workflow

| Item | Value |
|---|---|
| Script | `tools/a4k_tr1_capture.sh` (`d3d11` / `vulkan` / `compare` / `mae`) |
| Input | `tools/a4k_tr1_explore.txt` (opening → walkway → outdoor look / second camera) |
| Launch | `tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 …` |
| Fog hist | `SCR_A4K_FOG_STATS=1` → `A4k-fog` log lines |
| Outputs | `out/a4k_vk_tr1/`, `out/a4k_d3d_tr1/`, `out/a4k_compare/` |
| Preserved | `out/a4j_*`, `out/a4i_*` untouched |

Landmarks covered (deterministic input; **not** raw-XFB-index locked across backends):

1. Stage opening / banner  
2. Opening platform  
3. Early walkway  
4. First substantial outdoor scenery  
5. Distant background / planet  
6. Fog-visible distant geometry  
7. Near and distant lit materials  
8. Transparent / blended environmental draws (via normal GX)  
9. Sonic against outdoor env  
10. HUD / UI during gameplay  
11. Later Act 1 locations along explore script  
12. Second camera orientation on background geometry  

Per-run `meta.txt` records renderer, scale, shot cadence, script, GPU (`SCR_VK_DEVICE=3080` for Vulkan).

**Capture caveat (resolved for resolution):** Default D3D `SaveTextureBMP` **2× downscaled** TAS dumps (historical 320×240 from 640×480 XFB). That was **capture alignment**, not a renderer half-res bug. A4k sets `SCR_SHOTS_FULL=1` so D3D BMPs match native XFB size (typically **640×480**, same as Vulkan EFB shots). Cross-backend ordinal MAE is still **not** pixel-parity proof (VK ~5 unique_xfb vs D3D ~60 → phase mismatch). Prefer regional blackish metrics and same-backend sequences.

## 3. Fog audit

### Shared path

- `BuildPSKey` encodes `fogType = fsel | proj<<3 | range<<4` (identical for D3D11 and Vulkan).
- HLSL (`shadergen.cpp`) and GLSL (`shadergen_glsl.cpp`) share linear / exp / range formulas.
- A4i: range adjustment uses `gl_FragCoord.x` (GLSL) ↔ `i.pos.x` (HLSL). **Preserved.**

### TR1 outdoor histogram (`SCR_A4K_FOG_STATS`)

Observed on both backends after `stg110` (counts grow similarly):

| fogType | Meaning | Prevalence |
|---|---|---|
| `0x00` | No fog (`fsel < 2` forced off) | High (UI / near / unfogged) |
| `0x02` | Linear fog, no range bit | Dominant outdoor |
| `0x0a` | Linear + range (`bit4`) | Rare (~738 draws in long runs) |
| Other fsel | — | Not seen in these TR1 runs |

### Controlled / regression

- `scr_fog_light_parity_test`: linear, range (`gl_FragCoord.x`), exp modes, spot/diffuse lighting string parity — **PASS**.
- No evidence that remaining outdoor residuals are fog-formula bugs after A4j cull fix.
- **Unsupported claim:** full GX fog matrix (all fsel/proj/range combos) — only TR1-seen modes audited.

## 4. Lighting audit

- Shared `GenLight` / channel packing in VSKey for HLSL and GLSL (A4g finding stands).
- TR1 outdoor materials use standard colour channels + diffuse/spot patterns exercised by `scr_fog_light_parity_test`.
- No separate Vulkan lighting path; no rewrite.
- Sign-off: **configs used by tested TR1 scenes** — not every GX lighting mode.

## 5. Post-processing investigation

Traced differences called “post” in prior notes:

| Candidate | Finding |
|---|---|
| Missing GX copy/filter stage | No evidence of a skipped dedicated post pass beyond existing EFB-copy / XFB path |
| EFB/XFB conversion | A4f/A4i copy + RAM fingerprint retained; no A4h full-EFB async queue |
| Colour / alpha | Blend/write-mask from A4j retained |
| Capture / resolution | **Fixed:** `SCR_SHOTS_FULL=1` disables intentional 2× D3D BMP downscale so compares use native XFB |
| Presentation timing | VK unique_xfb ~5 vs D3D ~60 → UI/HUD frames not phase-matched |

**Fixes:** D3D screenshot dump parity only. Did **not** reintroduce device-wide idle waits or full-EFB queues.

## 6. UI / temporal consistency

- HUD continues through normal GX 2D draws (A4g).
- Observed “UI timing” gaps vs D3D are consistent with **renderer scheduling + capture cadence**, not a missing overlay compositor.
- Guest sim / audio timing unchanged.
- **I1** (Windows short-tap / latency) is a separate task — **not** claimed fixed by A4k.

## 7–8. Screenshot & temporal comparisons

| Set | Shots | Outdoor (blackish &lt;5%) late_half | late sky top-third |
|---|---:|---:|---:|
| Vulkan `out/a4k_vk_tr1/` | 49 | **0.60%** | **1.25%** |
| D3D11 full-res `out/a4k_d3d_tr1/` (640×480) | 145 | ~1.6% outdoor filter* | late sky ~1.4% (all-shot late_half ~2.5%) |

\*D3D explore run is longer / more varied cameras than A4j’s short late outdoor window (~0.2%); not a regression of the A4j cull fix on Vulkan.

Same-resolution outdoor pairs (`out/a4k_compare/fullres_side_*.png`): best ordinal mid-outdoor MAE ~47 (still phase-mismatched cameras). Sky TL on VK remains blue (~57,99,158); planet/sky fill present. Do not treat ordinal MAE as fog/lighting failure.

Temporal: sequences of outdoor shots show stable sky TL ~`(57,99,158)` class blues on VK; HUD draws present in-stage. Frame-locked UI animation parity vs D3D **not** established (FPS gap).

## 9. Blackish / regional metrics (unchanged definitions)

RGB sum &lt; 24; late_half = second half of shot list; sky = top third; geom = bottom two thirds.

| Metric | A4k VK outdoor late | A4j VK ref | A4j D3D ref |
|---|---:|---:|---:|
| late_half blackish | ~0.6% | ~0.5% | ~0.2% |
| sky top-third | ~1.2% | ~0.9% | — |
| geom bot two-thirds | ~0.3% (VK all-shot geom) | — | — |

Regions reported independently (no new aggregate that hides sky regression).

## 10. Performance (RTX 3080, validation off)

| Config | unique_xfb (approx) |
|---|---|
| Vulkan TR1 outdoor | **~4.9–5.2** |
| D3D11 TR1 outdoor | **~60** |

Vulkan GX counters (post-`stg110` sample): `efbCopy` all **sync** (`inline/sync=0/N`); pipe/shader hit rates high after warm-up; `xfHW` saturates ring high-water; `spv fail=0`, `unsup=0`.

**Largest bottleneck:** synchronous EFB-copy fences (A4f/A4h retained for stability). No broad perf optimisation in A4k (correctness first).

## 11. Vulkan validation

- Title run under `tools/run_nvidia_vk_validation.sh` + `SCR_VK_DEVICE=3080`: **no** `Validation Error` / `DEVICE_LOST` in captured log (run time-limited; validation slows nav).
- Outdoor validation-labelled capture: use `a4k_tr1_capture.sh vulkan-val` when a long labelled run is needed; keep metrics separate from normal-speed FPS.

## 12. Regression results

| Test | Result |
|---|---|
| `scr_topo_invalid_test` | PASS |
| `scr_shader_cache_test` | PASS |
| `scr_efb_copy_tex_test` | PASS |
| `scr_cull_winding_test` | PASS |
| `scr_fog_light_parity_test` | PASS (new) |
| `scr_input_edges_test` | PASS (I1; unchanged) |

## 13. Builds

- Linux: `out/SonicColorsRecomp` OK  
- MinGW: `out-windows/SonicColorsRecomp.exe` OK  
- D3D11 remains default / independent; Linux D3D11/DXVK still used for reference captures  

## 14. Remaining visual differences

| Class | Notes |
|---|---|
| Capture alignment | Resolution fixed via `SCR_SHOTS_FULL`; phase mismatch remains |
| Temporal / presentation | VK ~5 unique_xfb vs D3D ~60 |
| Fog / lighting | Formula parity for TR1 modes; **not** every GX mode; no pixel-locked outdoor MAE proof |
| Post-processing | No missing stage found; residuals mostly temporal |
| UI composition | GX path OK; animation phase not locked |
| Unknown | Fine material/TEV deltas possible under misaligned cameras |

## 15. Explicit deferred / out of scope

- AMD/RADV: **deferred, unverified**  
- Native Windows Vulkan GPU: **unverified**  
- Vulkan as default renderer: **no**  
- I1 Windows input reliability: **not covered by A4k**  
- Broad EFB-copy async / full-EFB queue: **not reintroduced**  

## 16. What is visually signed off

**Signed off for tested Tropical Resort Act 1 outdoor coverage on RTX 3080 native Vulkan:**

- Sky / planet / background fill (A4j cull) remains intact under expanded cameras.  
- Fog types `0x00` / `0x02` / rare `0x0a` share D3D11 encoding + generator formulas.  
- Lighting channel generation parity for exercised TR1-like keys.  
- late_half / sky blackish stay near A4j reference (no sky-void regression).  

**Not signed off:** full stage pixel parity, all fog/lighting GX modes, frame-locked UI timing vs D3D, RADV, native Win Vulkan.
