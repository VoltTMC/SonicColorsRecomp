# A4i — Sky/Background Reconstruction & EFB Copy Ordering

**Status (2026-11-25):** Root cause of TR1 sky voids **narrowed with evidence**; two Vulkan correctness fixes shipped. **Sky voids not yet closed** — blackish metric unchanged vs A4h. Full TR1 outdoor parity **not** claimed.

**Baseline:** A4h `f2baf63` (EFB-copy NDC Y `{-1,-1,1,1}`, XF ring, upright outdoor+HUD).

## 1. A4h starting state

| Item | A4h |
|---|---|
| TR1 entry (Vulkan) | Yes (`stg110`) |
| World vs HUD orientation | Fixed (EFB-copy Y) |
| XF SSBO ring | Intact |
| late_half blackish VK / D3D | ~19.3% / ~0.3% |
| Sky / planet / stars | Missing on VK |
| Inline full-EFB queue copies | DEVICE_LOST; reverted |

## 2. Sky/background path audit

Matched A4h/A4i captures: D3D shows planet + starfield; Vulkan shows clear-colored voids in the same screen regions.

`SCR_A4I_DRAW_LOG` (post-`stg110` outdoor-ish frames) shows candidate distant/large-tex draws:

- RAM textures (`efb=0`), often 256²/512²
- `projType=0`, `flipY=+1`, full viewport/scissor 640×480
- Mix of `cull=Back(1)/Front(2)/None(0)`
- Often `fogType=2` (linear fog, no range bit in logged samples)
- Large world-space verts (e.g. `(-4762,5226,7123)`) sharing `pmtx=0` with nearby visible draws

## 3. Root cause (established) + open mechanism

### Established

**Sky void pixels are the EFB clear color, not drawn black geometry.**

`SCR_A4I_MAGENTA_CLEAR=1`: outdoor sky top-third ≈ **50% magenta**, ≈0% black. Therefore those pixels are never covered by a successful shade.

Category: **draw never covers the pixel** (missing / off-screen / degenerate after transform — not “fogged to black” as the primary void mechanism).

### Ruled out as primary void cause (probes)

| Probe | Result |
|---|---|
| `--dbg-nocull --dbg-nodepth` | Sky still void; late_half worse (~40%) |
| `SCR_A4I_NO_FOG=1` | Sky still void (~20% late_half) |
| `SCR_A4I_NO_ALPHA_TEST=1` + magenta | Sky still mostly magenta (~46%) |
| `SCR_A4I_FAR_DEPTH_ALWAYS=1` | Sky still void |
| `SCR_A4I_NO_EFB_TEX=1` | Scene collapses (~96% blackish) — EFB-copy tex are load-bearing for world, not the sky-void explanation |

### Still open (next mechanism to pin)

Why candidate background draws do not cover clear pixels on Vulkan while D3D fills planet/stars:

- Vertex transform / clip after Vulkan `clip.y = -clip.y` for those batches
- XF SSBO contents for specific submissions (ring must stay correct — do not regress A4g)
- Alpha/TEV producing zero coverage without hitting the alpha-test kill path we forced off
- Draw ordering relative to clears (less likely given magenta persistence)

Fog GLSL previously used `inClipFinal.x` for range adjust while D3D uses pixel X (`SV_Position`); fixed to `gl_FragCoord.x` (correctness), but **did not** remove voids (`NO_FOG` confirms).

## 4. Missing/incorrect operations

| Change | Status |
|---|---|
| GLSL fog-range X = `gl_FragCoord.x` | Shipped |
| Vulkan EFB-copy RAM fingerprint (D3D `ramHash` parity) | Shipped |
| Sky coverage / planet draw | **Open** |
| Residual mesh holes | Partially sky-exposed clear; geom late ≈3% blackish |

## 5. Residual mesh

Late `geom_bot_two_thirds` blackish ≈ **3.2%** vs sky top-third ≈ **57%**. Many “holes” are clear showing through (same magenta proof class) rather than independent mesh corruption. XF ring left intact; no global cull disable.

## 6. EFB-copy ordering

Still fence-sync + region snap (A4h). In-frame inline full-EFB queue **not** reintroduced.

RAM fingerprint on copy + Lookup with `s_frame`: if guest overwrites copy dest, Lookup misses and Flush decodes CPU texture (D3D GetTexture parity). Safe: no DestroyImg on the draw path when hash mismatches.

## 7. A4h DEVICE_LOST experiment

Unchanged finding: thousands of full-EFB blits in one CB → TDR / `VK_ERROR_DEVICE_LOST`. Sync+region path remains. Not re-tried.

## 8. In-frame copy architecture

No new unsafe queue path. Sync drain + region blit retained.

## 9. Copy output verification

`scr_efb_copy_tex_test` OK. Live TR1 still depends on sync copies (`efbCopySynced` dominates).

## 10. Fog audit

- Generators: HLSL/GLSL fog body aligned except range X (fixed).
- Outdoor: fog present in GX state (`fogType=2` on many draws); **visual fog sign-off deferred** until sky coverage exists (void edge dominates).
- `SCR_A4I_NO_FOG` did not restore planet → fog not the void root cause.

## 11. Captures

| Path | Role |
|---|---|
| `out/a4h_*` | Preserved A4h baseline |
| `out/a4i_vk_tr1/` | A4i Vulkan TR1 |
| `out/a4i_d3d_tr1/` | D3D reference (A4h set reused where noted in meta) |
| `out/a4i_compare/` | Converted PNGs + magenta probe |
| `tools/a4i_tr1_capture.sh` | Capture + blackish + sky/geom split |

## 12. Blackish metric (unchanged definition)

RGB sum &lt; 24; `late_half` = mean of second half of shot list.

| Set | late_half |
|---|---:|
| A4h Vulkan | ~19.3% |
| A4i Vulkan | ~21.3% (scene timing noise; **not** an improvement) |
| D3D11 | ~0.3% |

## 13. Regional

| Region (A4i VK late) | blackish |
|---|---:|
| Sky top third | ~57.7% |
| Geom bottom two thirds | ~3.2% |

## 14. Validation

Title Vulkan + Khronos (`tools/run_nvidia_vk_validation.sh`): no `Validation Error` / `DEVICE_LOST` lines in `out/a4i_vk_tr1/title_val.log`.

## 15. Performance (RTX 3080)

TR1 Vulkan unique XFB ≈ **4.5–5.2**/s (unchanged class vs A4h). D3D11 ≈ 30–60. Bottleneck remains sync EFB copies.

## 16. Remaining defects

- Sky/planet/starfield coverage on Vulkan
- Residual clear holes in world
- True in-frame EFB copies without device loss
- Outdoor fog visual sign-off
- Full TR1 parity

## 17–18. Deferred

- AMD/RADV: **deferred / unverified**
- Native Windows Vulkan GPU: **unverified**

## Diagnostics (keep for A4j)

| Env | Purpose |
|---|---|
| `SCR_A4I_MAGENTA_CLEAR` | Prove clear vs drawn black |
| `SCR_A4I_DRAW_LOG` | Candidate bg draws |
| `SCR_A4I_NO_FOG` / `NO_ALPHA_TEST` / `NO_EFB_TEX` / `FAR_DEPTH_ALWAYS` | Probes |

## A4i success criteria (honest)

Sky root cause **class** identified (uncovered clear). Sky **not** corrected; blackish **not** improved toward D3D. Correctness fixes (fog-range X, EFB RAM hash) + capture tooling shipped; A4d–A4h architecture preserved.
