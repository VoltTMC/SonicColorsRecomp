# A3h — Guest-Side Skeleton Hierarchy and GX Palette Provenance

**Status date:** 2026-11-23  
**Target:** Sonic Colors Wii PAL `SNCP8P` Rev.00  
**GPU evidence:** NVIDIA GeForce RTX 3080 via `tools/run_nvidia_clean.sh`  
**Primary log:** `out/phase33_a3h_tr1_nv21.log` (also nv16–nv20 crash/fix bisect)  
**Starting commit:** `1359a8d` (A3g)

## Switches (default off)

| Env | Role |
|---|---|
| `SCR_A3H_SKELETON_TRACE` | Resolve `nw4r::g3d::ScnMdlSimple` + `ResNode.toParentNode` |
| `SCR_A3H_PALETTE_MAP` | Indexed XF → guest EA / mtxID / row map |
| `SCR_A3H_HIERARCHY_DUMP` | Denser bounded logs |

Implementation: `runtime/debug/a3h_skeleton_palette.*`. FIFO hook in `gx_fifo.cpp` (`A3h_OnIndexedXf`). Frame drain from `A3a_OnXfbCopy`. A3g caps raised to draws=768 / obs=2048 (honest overflow).

---

## A. Guest code findings

| Item | Result | Status |
|---|---|---|
| Graphics stack | **nw4r::g3d** `ScnMdlSimple` / `ResMdl` / `ResNode` (not Mirage) | PASS |
| Layout source | ogws NW4R G3D: `mResMdl@0xE8`, `mpWorldMtxArray@0xEC`, `mpViewPosMtxArray@0xF4`, `mNumViewMtx@0x102` (u16) | PASS |
| Parent field | `ResNodeData.toParentNode` relative ptr @ `+0x5C`; `mtxID` @ `+0x10`; node size `0xD0` | PASS |
| Player ownership (prior A3g) | shell `80b58b00` → body `90f9c6c0` → inst `27a5990d` | PASS |
| Palette upload | GX `LoadIndexedXF` → CP array base + `index*stride` → XF mem; PNMTXIDX row → XF addr `row*4` | PASS |
| CalcWorld/CalcView hook | No SNCP8P `GA` symbol yet for `ScnMdlSmpl_G3DPROC_CALC_*`; structural discovery only | PARTIAL |
| Player→ScnMdl pointer chain | Shell walk reaches small nearby ScnMdls; **does not** yet prove the high-hit player draw palette’s owning ScnMdl | PARTIAL |

**Important:** `mpViewPosMtxArray` is a **pointer field**, not inline storage. `paletteBase - 0xF4` is not a valid ScnMdl address.

---

## B. Hierarchy results

| Claim | Evidence | Status |
|---|---|---|
| Parent relationships from guest data | `toParentNode==0` → root; else parent EA = node+ofs matched in ResDic | PASS (for resolved ScnMdls) |
| Example tree (nv21) | `927d3e70` mtxID=5 root → child `927d3f40` (+0xD0) → … 7–9 nodes, parents resolved | PASS |
| Player Sonic full skeleton | High-hit GX pals (~300 loads) often **ScnMdl backref miss**; primary often a **6-mtx / 7-node** prop/HUD-adjacent model (`809aab60` / `92f6cd80`) | PARTIAL |
| Root / body world vs skeleton | A3g `M ≠ V·W` unchanged; body `+0x0C` still not proven as skeleton root input for captured XF | UNRESOLVED |
| Hierarchy across anim states | Verified parent links stable for resolved ResMdl while that ScnMdl remains selected; player-specific ScnMdl identity across walk/jump/boost **not** locked | PARTIAL |
| False-positive rejection | Naive “largest ScnMdl near shell” produced `res=80000010` garbage; tightened `LooksLikeScnMdlAny` (align, MEM bounds, ResNode spot-check) | PASS |

---

## C. Palette mapping

| Claim | Evidence | Status |
|---|---|---|
| Indexed XF → guest EA | `A3h_OnIndexedXf` records canon EA, index, stride, XF row; hash deferred to XFB | PASS |
| mtxID | Load `index` into palette array (stride typically `0x30`) | PASS |
| Row → observation | A3g obs content-hash / XF-row match → `mapped=1869` unres=0 on player frames | PASS |
| Row → ResNode | Only when captured hierarchy’s `mtxID` matches; many player rows still `node=-1` | PARTIAL |
| High-hit pals (e.g. `80966de0` ~316 hits) | Often unresolved to ScnMdl (draw-time copy / different owner buffer) | PARTIAL |

Example (nv16/nv21): `xfRow=27 mtxID=115 guest=8098e9b0` with `base = guest - 115*0x30 = 8098d420`.

---

## D. Completeness (vs A3g 128 / ~518 overflow)

| Metric | A3g (`1359a8d`) | A3h (nv21) | Status |
|---|---|---|---|
| Selected draws | 128 +ovf≈518 | **646/768 +ovf0** | PASS |
| Observations | ~obs capped with overflow | **1869/2048 +ovf0** | PASS |
| Unique obs / dup / conflict | retained | uniq=1662 dupRow/Hash=1869 conflict=154 | PASS |
| Idx XF loads | n/a | up to **2594/4096 +ovf0** | PASS |
| Silent truncate | yes (128 keep) | no (honest caps + overflow counters) | PASS |

---

## E. Temporal correspondence

| Claim | Status |
|---|---|
| Player instance `27a5990d` stable in TR1 | PASS (A3g + A3h) |
| A3g `samePrev=1` after settle | PASS |
| A3h `sameSkel=1` for primary ScnMdl | PARTIAL (primary often churns among small resolved models; not locked to Sonic ScnMdl) |
| Node index meaning across frames | PASS for a fixed ResMdl; UNVERIFIED for player body ScnMdl until ownership closed |

---

## F. Transform semantics

Preserved from A3g:

- Captured draw matrices are **model×view** (view space).
- `inv(V)·M` modelApprox validated.
- Body world at `body+0x0C` exists; **`M = V·W` does not hold** for observed player draws — still UNRESOLVED as to which stage W should enter.

A3h adds:

- Guest palette entries from indexed XF are the **pre-XF** 3×4 sources (view-pos or world array, or a draw-time copy).
- Composition order for hierarchy: local → parent-accumulate (`CalcWorld`) → view (`CalcView`) → GX indexed load — **routine addresses not hooked yet**.

---

## G. Performance

| Mode | sim_hz (RTX 3080) | Status |
|---|---|---|
| Flags-off TR1 | **~60 Hz** (`phase33_a3h_flags_off.log`) | PASS |
| Dense A3a+A3g+A3h TR1 | **~6–8 Hz** busy (diag-bound; expected) | PASS (honest) |
| Crash class | Host `Read32(non-canonical EA)` misattributed to `CameraManagerTick` — fixed via canonical-only `GuestOk` + deferred hashing | PASS |

Windows native GPU: **UNVERIFIED**. MinGW link: OK.

---

## Acceptance checklist

1. Guest skeleton/animation structures identified — **PARTIAL** (nw4r layouts + ResNode parents; player ScnMdl ownership open)  
2. Parents from verified guest data — **PASS** (for resolved ScnMdls)  
3. Actor/model root — **UNRESOLVED** (`M ≠ V·W` boundary documented)  
4. PNMTXIDX → guest source/stage — **PASS** (EA/mtxID/stage); node link **PARTIAL**  
5. Hierarchy↔matrix relationship — **PARTIAL**  
6. Aggregation beyond 128 — **PASS** (646/768+ovf0)  
7. Dup / shared / conflict distinguishable — **PASS**  
8. Stable skeleton-node correspondence — **PARTIAL**  
9. Flags-off ~60 Hz — **PASS**  
10. Regressions — **PASS** (`regression_smoke`; A2f-efbfb + A2e-multitex MATCH; MinGW links)

---

## Interpolation readiness

**Do not proceed to interpolation yet.**

Missing minimum evidence:

1. Hook or otherwise prove the **player body’s** `ScnMdlSimple*` and its `ResMdl` / bone count.  
2. Map high-hit indexed-XF palette bases to that ScnMdl’s `mpViewPosMtxArray` / `mpWorldMtxArray` (or prove they are intentional copies and hash-match).  
3. Close the **root / body world** composition (`M` vs `V·W`) from guest CalcWorld/CalcView.  
4. Demonstrate `sameSkel=1` on that player ScnMdl across idle/walk/jump/boost.

Smallest next investigation: register `ScnMdlSmpl_G3DPROC_CALC_WORLD` / `CALC_VIEW` (or Colors equivalents) in `game_addrs` and capture `r3=this` when shell/body ownership is live — avoid broad MEM heuristics.
