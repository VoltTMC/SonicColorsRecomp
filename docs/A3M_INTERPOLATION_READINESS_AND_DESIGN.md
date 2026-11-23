# A3m — Interpolation Readiness and Design

**Date:** 2026-11-23  
**Starting commit:** `ac8c8a5` (A3l)  
**GPU evidence:** NVIDIA GeForce RTX 3080 · `out/logs/a3m_tr1_qualify.log`  
**Scope:** Analysis, validation, and design only. **No interpolation implemented.**

Opt-in qualify: `SCR_POSE_QUALIFY=1` (also enables A3l capture). Off by default.

---

## A. Readiness assessment

| Pillar | Status | Evidence |
|--------|--------|----------|
| Ownership | **PASS** | Call-time ScnMdl `809882e0`; shell/body via A1b; no body-stored pointer |
| Transform semantics | **PASS** | World camera-independent; ViewPos = \(V̂·World\); root = `ScnMdl+0x3C` |
| Temporal correspondence | **PASS** | 5564/5581 pairs OK (~99.7%); rejects classified |
| Capture completeness | **PASS** | A3l contract; 58/58 in-palette; RelOk ≈0.001 |
| Matrix suitability | **PASS** | All sampled World: **Rigid**; root Rigid; badCls=0 |
| Camera separation | **PASS** | camDom=772 vs poseDom=230; CamCut=15 fail-closed |
| Timing | **PASS** | XFB-tied capture; guest paced ~60 Hz; seq consecutive |
| Fallbacks | **PASS** | Pair reject enum + recommended behaviours (design) |
| Performance | **PASS** | Qualify-on TR1 `simulation_hz` ≈59.7–60 |

**Decision: READY to begin a separate interpolation implementation milestone**, with TR1 Sonic as the first target and the fallbacks below.

White Wisp / respawn / other stages: **UNVERIFIED** (not blocking TR1-first impl).

---

## 1. A3l audit (code vs docs)

Verified in `pose_capture.*` (no format change required):

| Claim | Enforced in code |
|-------|------------------|
| Capture after CalcWorld original | `a3i` PPC_OVERRIDE → `PoseCapture_OnCalcWorld` after `PPC_ORIGINAL` |
| Publish at XFB | `Perf_OnXfbCopy` → `PoseCapture_OnXfb` |
| Call-time ownership + A1b | Live shell/body required; `nView≥64` |
| Visual root `+0x3C` | Copied; ≠ body |
| World + active ViewPos slot | `byte101` ∈ {0,1}; copy not pointer |
| ResDic 78 / in-palette 58 | Hierarchy cache; Completeness on in-palette only |
| \(V̂\) + RelOk | Palette index 0; reject RelFail |
| 3-slot ring, Complete-only publish | Mutex; `GetLatest`/`GetPrevious` |
| Off by default | Env gate |

**Cadence:** One publish attempt per unique `Perf_XfbFrameIndex` (not raw sim ticks). Under `--60fps`, XFB ≈ guest sim ≈ 60 Hz. Do not treat XFB and sim as interchangeable on uncapped hosts without checking both IDs.

**Distinctions preserved:** 78 ≠ 58 ≠ `nView=193`.

---

## B. Proposed data flow

```
guest sim (60 Hz)
  → CalcWorld (World) → view calc (ViewPos)
  → A3l PoseCapture_OnXfb  [EXISTS]
  → SCR_POSE_QUALIFY pair/class counters  [EXISTS, opt-in]
  → future: temporal pair select + α(displayTime)
  → future: World TRS process (in-palette)
  → future: apply V̂(t) (or A3b camera if proven)
  → future: GX / draw path
```

Guest sim + audio timing unchanged. Presentation may sample between snaps later; A3m does not.

---

## C. Proposed transform strategy

### Canonical pose: World matrices (in-palette)

**Why:** Qualify shows World translation can move while ViewPos stays nearly fixed when camera follows the player (`viewΔT≪worldΔT` with large `vHatΔT`). Interpolating ViewPos alone cancels player motion against camera motion.

World matrices are parent-accumulated outputs of CalcWorld (nw4r). They do **not** include the camera.

### Visual root

`ScnMdl+0x3C` feeds CalcWorld; A3k found World[0] ≈ root (exact or near).  
**Future rule:** When consuming `nodes[].world`, do **not** also left-multiply `root` — that would double-apply. Keep `root` for validation / diagnostics / alternate attachment.

### Hierarchy / node set

| Set | Role in first impl |
|-----|--------------------|
| 58 in-palette ResNodes | **Required** — interpolate World (TRS); write GX palette |
| 20 non-palette ResNodes | **Identity/validation** — parents already baked into child World; no matrix interp required for GX |
| `nView=193` slots | Palette capacity; unused slots not in ResDic pose set |

Smallest safe consumer set: **58 in-palette World matrices** + header identity + `vHat` + parent table for validation. Retain full 78-node metadata in the snap (already present).

### \(V̂\) / ViewPos

| Field | Classification |
|-------|----------------|
| World | Required input (canonical) |
| ViewPos | Validation / diagnostic; recompute as \(V̂·W'\) after World interp |
| `vHat` | Required derived camera/view for the snap; interpolate separately |
| Root | Validation; do not double-apply with World |
| Parent/mtxID/modelGen/seq/xfb | Identity / pairing metadata |

### Camera separation

- **Pose:** World  
- **View:** \(V̂\) from snap (ViewPos[0]·inv(World[0])), **not** A1b XF pos0 (A3k rejected that as \(V\))

**A3b audit:** A3b interpolates A1b `viewMtx` (first XF PNMTX0) for scratch camera deltas. That path is **not** interchangeable with A3l \(V̂\) without a new equivalence proof. First skeletal impl should drive view from **snap `vHat`**, optionally later reconcile with A3b.

### Matrix class (TR1 Sonic)

From `a3m_tr1_qualify.log` (final sample):

| Class | World samples | Root |
|-------|---------------|------|
| Rigid | 323698 | 5581/5581 |
| Uniform / NonUniform / Shear / Singular / Invalid / Reflection | **0** | — |

**First impl:** decompose World → T + quat (+ uniform scale if ever seen); slerp/lerp; reject Shear/Reflection/Singular (none observed on TR1 Sonic).

### Final GX order (proposed)

1. Interpolate World → \(W'(α)\) per in-palette node.  
2. Interpolate \(V̂'(α)\) (rigid view).  
3. \(ViewPos'_i = V̂' · W'_i\).  
4. Upload palette; do not apply root again.

---

## D. Timing strategy

| Source | Semantics |
|--------|-----------|
| `seq` | Monotonic published complete snaps |
| `xfbIndex` | GPU/XFB ordinal at publish |
| `simFrameId` | Guest frame end counter |

**Bracket:** For display time \(t\) between snaps \(P,C\) with consecutive `seq` and pair OK:

\[
α = \mathrm{clamp}\big((t - t_P)/(t_C - t_P), 0, 1\big)
\]

Under VI-locked 60 Hz, \(t_{n+1}-t_n ≈ 1/60\) when `xfbIndex` differs by 1. Prefer wall-clock between XFB callbacks if available later; until then, use ordinal Δ with fixed guest dt.

| Condition | Behaviour |
|-----------|-----------|
| No previous / Warmup | Show current snap only |
| Target before \(t_P\) or after \(t_C\) | Clamp to nearest snap (no extrapolate) |
| `xfbGap` with consecutive `seq` | Still OK; widen dt |

---

## E. Rejection and fallback policy

From `Pose_EvaluatePair` + TR1 counts (`ok=5564/5581`):

| Reject | TR1 count | Detection | Future behaviour |
|--------|-----------|-----------|------------------|
| MissingPrev | 1 | No previous complete | Show current; wait |
| Warmup | 0 (curr) | First snap flag | Show current |
| ModelGen / Scn / Own | 1 gen | Identity mismatch | Reset history; current only |
| NodeCount / ParentTable | 0 | Hierarchy mismatch | Reset; current only |
| SeqGap | 0 | `seq≠prev+1` or large sim jump | Reset; current only |
| Teleport | 1 | World ΔT > 25 | Current only; reset blend |
| CamCut | 15 | \(V̂\) ΔT > 40 and World ΔT < 1 | Current only (no blend) |
| BadClass | 0 | Shear/singular/… | Current; skip node or snap |
| Incomplete | 0 | Flags | Never pair |
| Ring overwrite | n/a | Consumer lag | Prefer latest; drop stale prev |

**Never** pair across `modelGen`, fabricate matrices, or extrapolate past the bracket.

Unchanged pose (`worldΔT≈0`) is a **valid** pair (idle) — distinct from missing snap.

---

## F. Performance budget

| Item | Measured |
|------|----------|
| A3l snap | ≈26.8 KiB × 3 ≈ 80 KiB static |
| Capture-on | ≈59.9 Hz (A3l) |
| Capture+qualify | ≈59.7–60 Hz TR1 (`a3m_tr1_qualify.log`) |
| Qualify cost | Class + pair on published snaps; no dumps |

**Proposed future budget (design only):** keep total pose+view process ≪ 1 ms/frame on RTX 3080 so guest stays at 60 Hz; measure in the implementation milestone.

---

## G. Implementation milestones (future only)

1. Consumer reads validated pairs (`Pose_EvaluatePair`) + α from timing.  
2. World TRS decompose/compose for Rigid (reuse A3b math patterns, new call sites).  
3. \(V̂\) rigid interp; recompute ViewPos.  
4. Limited scratch/draw integration (one palette path).  
5. Fallbacks for CamCut/Teleport/ModelGen.  
6. Expand states (boost/wisp) + perf/visual QA.

---

## H. Explicit limitations

- Validated production target: **TR1 Sonic** (`stg110`).  
- White Wisp / respawn / other characters/levels: **UNVERIFIED**.  
- Native Windows GPU: **UNVERIFIED**.  
- A3b XF view ≠ A3l \(V̂\) without new proof.  
- Local (non-accumulated) ResNode transforms not captured — not required for World-space first impl.  
- No wall-clock timestamp in snap (ordinal timing sufficient at locked 60 Hz).

---

## Temporal qualification summary (TR1)

| Metric | Value |
|--------|-------|
| Pairs examined | 5581 |
| Pairs OK | 5564 (~99.7%) |
| CamCut rejects | 15 |
| Teleport | 1 |
| ModelGen | 1 |
| MissingPrev | 1 |
| BadClass / shear / non-uniform | 0 |
| World class | 100% Rigid (sampled) |

Camera-dominated vs pose-dominated deltas confirm **World ⊥ camera** for separation.

---

## Final decision gate

**Is the project ready to begin a separate interpolation implementation milestone?**

**YES — PASS** for a TR1-Sonic-first implementation that:

1. Uses A3l World as canonical pose.  
2. Applies interpolated \(V̂\) separately.  
3. Pairs only on `PosePair_Ok`.  
4. Falls back to current snap on CamCut/Teleport/identity breaks.  
5. Does not change guest sim or audio timing.

A3m itself introduces **no** interpolation, extra XFBs, or presentation changes.
