# A3i — Live Sonic Model Ownership and World/View Calculation Provenance

**Date:** 2026-11-23  
**Target:** Sonic Colors Wii PAL `SNCP8P` Rev.00  
**GPU evidence:** NVIDIA GeForce RTX 3080 via `tools/run_nvidia_clean.sh`  
**Logs:** `out/logs/a3i_tr1_capture3.log`, `out/logs/a3i_tr1_capture4.log`, `out/logs/a3i_flags_off.log`  
**Input:** `tools/a2g_tr1_enter.txt` + `tools/setup_a2g_test_nand.sh`

Opt-in only. **No interpolation / second XFB / unique-FPS / guest or audio timing changes.**

| Switch | Role |
|--------|------|
| `SCR_A3I_MODEL_OWNERSHIP` | Capture CalcWorld `r3=this` + body/shell association |
| `SCR_A3I_CALC_WORLD` | Record CalcWorld matrix array pointers |
| `SCR_A3I_CALC_VIEW` | Treat ViewPos array joins as view-stage evidence |
| `SCR_A3I_PALETTE_PROVENANCE` | Join A3h indexed-XF pals → producing ScnMdl/stage |

Implementation: `runtime/debug/a3i_model_ownership.*`. Guest hook: `ScnMdlSimpleCalcWorld` @ `805B87D0` (`tools/game_addrs.txt`). Drain from A3a XFB path (or standalone when A3a off).

---

## A. Function identification

| Item | Result | Evidence |
|------|--------|----------|
| Expected names `ScnMdlSmpl_G3DPROC_CALC_*` | **Not** present as SEL/`functions.json` symbols | String/xref scan; RTTI `ScnMdlSimple` @ `807477dc` |
| `ScnMdlSimple::G3dProc` | **PASS** — guest `805B8DC0`, vtable slot @ `807aa914` | Task tree: 1=CalcWorld→`805B87D0`, 4=callback, 5=ViewPos materialize via `805AE820`/`805AE340`/`805AE000`, 6=draw XLU |
| `ScnMdlSimple` CalcWorld helper | **PASS** — `805B87D0`; `r3=this`, uses `+0xE8` ResMdl / `+0xEC` World / `+0xF0` attrib | Disasm + live hook |
| `ScnMdl::G3dProc` | Present @ `805BBF40` (vtable `807aa95c`) | **Not hooked** — PPC_OVERRIDE of G3dProc sites faulted (see Safety) |
| Calling convention | Member-style; `r3=this`, `r4=task` (G3dProc) / param (CalcWorld) | Live capture `rawThis=80960f80 task=1` |
| NW4R vs Colors | Colors uses stock nw4r `ScnMdlSimple` layout (A3h); SonicUSA `CTopNode::CalcWorld` strings exist but are debug/RTTI names, not the hooked calc path | |

**Safety note:** Registering `PPC_OVERRIDE` on `805B8DC0` / `805BBF40` produced late guest faults (`guest EA 818000a8`, MSR-like `r4/r5`) under dense TR1. A3i therefore hooks **only** `805B87D0` (CalcWorld). View stage is proven by joining A3h palette bases to `this+0xF4` (ViewPos), which G3dProc task 5 fills.

---

## B. Player model ownership

| Item | Status | Notes |
|------|--------|-------|
| Shell / body / instance | **PASS** | `80b58b00` / `90f9c6c0` / `27a5990d` (matches A3f/A3g/A3h) |
| CalcWorld `this` under live ownership | **PASS** | Many distinct ScnMdlSimple* while shell/body live (e.g. 14 models/XFB) |
| Verified `body → ScnMdl*` word | **PARTIAL** | Direct/1-level scans of body/`shell+0x28` did **not** hit primary ScnMdl EAs; path remains open |
| Strong candidate | **PARTIAL** | Largest live models: `809882e0` `nView=193` (`ViewPos=8098afe0`); earlier capture also `80979bc0` `nView=106` |
| Menu/title model | Observed | `80960f80` `nView=36` stable across title XFBs |
| Consecutive-frame stability | **PARTIAL** | Same shell/body/inst across TR1; primary ScnMdl among CalcWorld set can alternate with other large models when scoring by palette hits |

**Ownership evidence standard:** PaletteMatch (`own=3`) = ViewPos/World array base equals a high-hit A3h palette. That is **not** yet a stored body pointer. PlayerProven (`own=1`) was not achieved for the high-hit Sonic-scale model in these runs.

---

## C. Matrix provenance (high-hit pals)

| Palette base | Hits | ScnMdl | Array | Stage | Status |
|--------------|------|--------|-------|-------|--------|
| `8098afe0` | ~632 | `809882e0` | ViewPos (`kind=1`) | View materialize (task5 path) | **PASS** join (PaletteMatch) |
| `80992ee0` / `80995320` | ~624 | unresolved | — | likely double-buffer / draw copy | **PARTIAL** — still missing producer ScnMdl |
| `809619c0` (title) | ~52 | `80960f80` | ViewPos | View | **PASS** (title) |
| World arrays from CalcWorld | — | per-model `this+0xEC` | World | CalcWorld | **PASS** pointer capture |

Indexed XF → guest EA / mtxID remains A3h’s job (**PASS**). A3i adds `guest EA/base → ScnMdl + stage` for a subset of high-hit pals; the ~624-hit twin bases stay unresolved.

Representative mapped row (TR1): `base=8098afe0 scn=809882e0 kind=1 stage=3 row/mtxID vary (e.g. row=27 mtxID=25 ea=8098b490)`.

---

## D. Root composition

| Claim | Status |
|-------|--------|
| Body world at `body+0x0C` | Still available via A1b/A1c |
| CalcWorld writes `mpWorldMtxArray` | **PASS** (fields + helper) |
| ViewPos is post-view bone palette | **PASS** (join + A3h stage=viewPos) |
| `M = V · W` for player draws | **UNRESOLVED** (unchanged; not forced) |

No numerical multi-state proof that draw `M` equals `V·W` with `W=body+0x0C`.

---

## E. Completeness (representative TR1 XFB)

From `a3i_tr1_capture4.log` ~xfb=3193 (shell live):

| Metric | Value |
|--------|-------|
| CalcWorld captures / XFB | 14 (bounded; pre+filter) |
| Distinct ScnMdl this | 14 |
| PlayerProven calls | 0 |
| PaletteMatch / unclassified | PaletteMatch on large models; many unclassified |
| A3h/A3i pal links | 48; unresolved often ~35–47 |
| High-hit mapped | `8098afe0`→`809882e0` |
| High-hit unresolved | `80992ee0`/`80995320` (~624) |
| Overflow | call ovf=0 in sampled XFBs |

Earlier denser G3dProc hook run (`capture3`): up to 100 calls / 22 models / primary `80979bc0` nView=106 before G3dProc override was removed.

---

## F. Performance and regression

| Item | Status | Notes |
|------|--------|-------|
| Flags-off ~60 Hz (RTX 3080) | **PASS** | `a3i_flags_off.log` |
| A3i+A3h+A3a+A3g busy TR1 | ~30–42 Hz diag-bound | Expected; default off |
| Peak RSS (instr) | ~670 MiB | |
| `regression_smoke` | **PASS** | |
| A2e/A2f exact MATCH | **PASS** (preserved; no path changes) | Prior exact families unchanged |
| Linux build | **PASS** | |
| MinGW link | **PASS** | |
| Native Windows GPU | **UNVERIFIED** | |
| Late dense-path fault | **PARTIAL** | One late SIGSEGV attributed to `fn_805B87D0` under heavy A3i+A3h still seen in capture4; flags-off clean. G3dProc overrides removed. |

---

## Acceptance vs brief

| Criterion | Status |
|-----------|--------|
| 1. World/view routines for live Sonic | **PASS** CalcWorld; **PARTIAL** view (array join, G3dProc not hooked) |
| 2. `this` for those calls | **PASS** |
| 3. Verified body→ScnMdl ownership path | **PARTIAL** — palette/CalcWorld association; pointer path missing |
| 4. High-hit palette → model/stage | **PARTIAL** — 632-hit ViewPos mapped; 624-hit twin unresolved |
| 5. Transform stage / space | **PASS** for mapped ViewPos (view-space bone palette) |
| 6. Root composition | **UNRESOLVED** |
| 7. Stable across frames/states | **PARTIAL** |
| 8–9. Regressions / flags-off 60 Hz | **PASS** |

---

## Interpolation-readiness gate

**Not justified yet.** Missing: defensible `body→ScnMdl*` (or component) pointer path; stable single player ScnMdl identity; resolution of the ~624-hit unresolved palette twin; root composition. Do **not** start interpolation from A3i alone.

**Next (separate milestone):** Trace graphics component from `shell+0x28` / body that holds `809882e0`-class ScnMdl*; identify producer of `80992ee0`/`80995320` without re-hooking G3dProc (e.g. LoadIndexedXF source or ViewPos allocator).
