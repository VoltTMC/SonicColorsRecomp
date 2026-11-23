# A3j — Player Model Ownership and Dual-Palette Provenance

**Commit base:** `7aab85d` (A3i). **Target:** SNCP8P Rev.00.  
**GPU evidence:** NVIDIA GeForce RTX 3080 via `tools/run_nvidia_clean.sh` + `tools/a2g_tr1_enter.txt`.  
**Implementation:** `runtime/debug/a3j_model_palette.*` (opt-in). Extends A3i CalcWorld observation; improves A3h/A3i palette span + ViewNrm/ViewEnv joins.

| Switch | Role |
|--------|------|
| `SCR_A3J_PLAYER_MODEL_OWNER` | Deep body/component / CModelImpl(+0x1C) search + call-time association |
| `SCR_A3J_PALETTE_A_TRACE` | ViewPos multi-buffer join (group A) |
| `SCR_A3J_PALETTE_B_TRACE` | ViewEnv (+0xFC) / ViewNrm (+0xF8) join (group B) |
| `SCR_A3J_ROOT_COMPOSITION` | Body/world/view translation samples |

Disabled by default. No G3dProc / ViewA–C / Construct `PPC_OVERRIDE` (late guest fault `818000a8` under TR1 — same class as A3i).

---

## A. Ownership chain

| Hop | Evidence | Status |
|-----|----------|--------|
| Live shell / body / inst | `80b58b00` / `90f9c6c0` / `27a5990d` (A1b/A3f/A3i) | **PASS** |
| CalcWorld `r3=this` while live | `809882e0` every TR1 XFB (`nView=193`) | **PASS** |
| Stored `body → ScnMdl*` / `shell+0x28 → …` word | Bounded BFS + CModelImpl `+0x1C` scan: **no hit** for `809882e0` | **UNRESOLVED** |
| Call-time relationship | G3dProc task1 → `CalcWorld(805B87D0)` with `this=809882e0` concurrent with live player identity; both high-hit pals are fields of that object | **PASS** (call-time) |

**Classification:** primary `own=3` (PaletteMatch) / A3j `own=4` (Associated). `PlayerProven` / `ModelImplProven` not achieved.

Illustrative (verified call-time, not a stored field):

`Player shell/body live → CalcWorld(this=809882e0) → ScnMdlSimple fields → GX pals`

---

## B. ScnMdlSimple identity

| Field | Value (TR1) | Notes |
|-------|-------------|-------|
| `r3=this` | `809882e0` | CalcWorld hook |
| ResMdl | `929d3ee0` | |
| World `+0xEC` | `80988480` | |
| ViewPos `+0xF4` | `8098afe0` | |
| ViewNrm `+0xF8` | `8098f860` | `= ViewPos + 2×slot` |
| ViewEnv `+0xFC` | `80992ee0` | **group B base** |
| `nView` / slot | 193 / `align32(193×0x30)=0x2440` | |
| `+0x100` / `+0x101` | `b100=2` (num view buffers), `b101` toggles 0/1 | Double-buffer index |
| Stability | Primary across consecutive TR1 XFBs / walk-jump-boost script | **PASS** |

---

## C. Palette group A

| Item | Result |
|------|--------|
| Bases | `8098afe0` (slot0) / `8098d420` (slot1 = base+`0x2440`) |
| Hits | ~316–632 / XFB (slot alternation; prior A3i ~632 on active slot) |
| Object | `mpViewPosMtxArray` of `809882e0` |
| Producer | G3dProc task5 → `805AE820`/`805AE340`/`805AE000` with `dest = ViewPos + b101×slot` (disasm; not hooked) |
| GX path | A3h indexed XF → guest EA in ViewPos range → draws | **PASS** |

---

## D. Palette group B

| Item | Result |
|------|--------|
| Bases | `80992ee0` (slot0) / `80995320` (slot1 = base+`0x2440`) |
| Hits | ~312–624 / XFB |
| Object | `this+0xFC` (**ViewEnv** matrix array) of the **same** `809882e0` — not a second ScnMdl |
| Relation to A | Parallel double-buffered view-family array; same `nView`/slot geometry |
| ViewNrm `+0xF8` | `8098f860` present; lower hit prominence than ViewEnv in this capture |
| Ownership | Same call-time / PaletteMatch as group A | **PASS** (identity); stored player ptr still open |

Title-menu sanity: `80960f80` shows `Vp`/`Vn`/`Ve` with the same `b100=2` double-buffer pattern (`grpA`+`grpB` both mapped).

---

## E. GX provenance (representative)

| Group | Palette base | ScnMdl | Array | Slot | Example XF | Status |
|-------|--------------|--------|-------|------|------------|--------|
| A | `8098afe0` / `8098d420` | `809882e0` | ViewPos | 0/1 | e.g. row=18 mtxID=97 ea=`8098c210` | **PASS** |
| B | `80992ee0` / `80995320` | `809882e0` | ViewEnv | 0/1 | A3h loads join to `scn=809882e0` | **PASS** |

Concepts kept distinct: ScnMdl ≠ palette base ≠ XF row ≠ bone.

---

## F. Root transform composition

| Sample | Observation |
|--------|-------------|
| Body world (A1b) | Player pose available; T compared via A3j root samples |
| World[0] (`+0xEC`) | Large world-space T (e.g. `(-13085, 1299, 12894)`) |
| ViewPos[0] (active slot) | Camera-relative scale (e.g. `(0.01, -5.5, -31)`) |
| `bodyT == worldT` | **No** (`rel=3`) |
| `M = V · W` | **UNRESOLVED** — intermediate camera/view multiply not closed numerically |

Status: **PARTIAL** (stages identified; equation not established).

---

## G. Completeness (TR1 busy frame)

| Metric | Approx. |
|--------|---------|
| Registry / models (A3j) | up to 256 / ~45 live |
| Player-associated large | PaletteMatch / Associated on `809882e0` |
| Group A mapped | ViewPos slots → `809882e0` |
| Group B mapped | ViewEnv slots → `809882e0` |
| Unresolved pals | Smaller bases remain (props/FX) |
| Stored ownership proofs | 0 |
| Overflow | Registry capped (raised 128→256); view-write unused (no View hooks) |

---

## H. Performance and regression

| Item | Status | Notes |
|------|--------|-------|
| Flags-off ~60 Hz | **PASS** | `a3j_flags_off.log` |
| Dense A3j+A3i+A3h+A3a | ~30 Hz | Diag-bound; default off |
| Peak RSS (instr) | ~670 MiB | |
| Late SIGSEGV under dense diag | **PARTIAL** | Shutdown/`fn_805B87D0` context wipe also seen in A3i-only; flags-off clean |
| `regression_smoke` / A2e / A2f | run at ship | |
| Linux / MinGW | run at ship | |
| Native Windows GPU | **UNVERIFIED** | |

---

## Acceptance vs brief

| Criterion | Status |
|-----------|--------|
| 1. Verified player↔ScnMdl relationship | **PASS** call-time; **PARTIAL** stored pointer |
| 2. Group A provenance | **PASS** |
| 3. Group B producer/meaning | **PASS** (ViewEnv of same ScnMdl) |
| 4. Both → calc/GX stages | **PASS** |
| 5. Root / `M=V·W` | **PARTIAL** / **UNRESOLVED** equation |
| 6. Stability across frames/states | **PASS** for `809882e0` + both groups |
| 7–9. Regressions / ~60 Hz | see H |

---

## Interpolation-readiness gate

**Not yet.** Palette dual-group identity and stable ScnMdl are closed. Remaining blockers for interpolation:

1. Stored `body/component → ScnMdl*` (or durable ModelImpl) path — call-time only today.  
2. Numerical root composition (`M=V·W` or the game’s real multiply order).  
3. Optional: safer non-`PPC_OVERRIDE` View-dest capture if task5 args must be live-traced.

Do **not** begin bone/matrix interpolation in A3j.
