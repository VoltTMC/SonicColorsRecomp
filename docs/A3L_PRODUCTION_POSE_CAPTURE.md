# A3l — Production-Bounded Pose Capture and Snapshot Contract

**Date:** 2026-11-23  
**Starting commit:** `4b2c344` (A3k)  
**GPU evidence:** NVIDIA GeForce RTX 3080 via `tools/run_nvidia_clean.sh` + `tools/a2g_tr1_enter.txt`  
**Log:** `out/logs/a3l_tr1_capture.log`

Opt-in only: `SCR_POSE_CAPTURE=1`. Off by default. **No interpolation, no extra XFBs, no presentation changes.**

---

## A. Capture architecture

### Capture point (coherent)

| Stage | Hook | Role |
|-------|------|------|
| After `ScnMdlSimpleCalcWorld` | `PoseCapture_OnCalcWorld` | Associate live player `ScnMdl*` (call-time ownership) once World arrays are written |
| XFB copy | `PoseCapture_OnXfb` via `Perf_OnXfbCopy` | Copy World + active ViewPos slot + root after view calc; publish ring slot |

World is produced in CalcWorld; ViewPos is filled later (G3dProc task5 path). Publishing at XFB ensures both buffers belong to the completed frame. Coherence is proven by the A3k relation residual (≈0.002), not by assuming task order alone.

### Ownership selection

Call-time only (A3k PASS): CalcWorld `this` under live A1b shell/body, player-scale `nView≥64`.  
Does **not** use body-stored ScnMdl (A3k REJECTED for TR1 player path).  
`SCR_POSE_CAPTURE` auto-enables A1b identity sources (same pattern as A3i).

### Snapshot layout

- Header: format, seq, sim/XFB ids, shell/body/instanceKey, ScnMdl/ResMdl, modelGen, node counts, view slot, flags, reject, root, \(V̂\), residuals.
- Nodes: ResDic index, parentIdx (A3h `toParent`), mtxID, World, ViewPos, per-transform validity.
- Capacity: `kPoseMaxNodes=256`, `kPoseRingSlots=3`. Fail closed on overflow (no silent truncate).

### Hierarchy metadata

Resolved once per `modelGen` from `ResMdl` ResDic (`toParent` @ `+0x5C`, `mtxID` @ `+0x10`). Cached while ResMdl identity holds.

### Buffer copy

Active ViewPos = `ViewPosBase + byte101 * align32(nView×0x30)` when `byte101∈{0,1}`.  
Matrices copied into the snapshot (no live pointers retained).

### Publication

Guest/GPU drain publishes under a mutex after all fields are filled. Consumers see only `PoseFlag_Complete` slots via `GetLatest` / `GetPrevious`. Incomplete attempts increment counters and are not published.

### Ring

Three static slots: previous | latest | write. Wrap increments `ringOverwrite`. No per-frame heap, no file I/O, no diagnostic dump transport.

---

## B. Data contract

| Field | Source | Space / meaning |
|-------|--------|-----------------|
| `root` | `ScnMdl+0x3C` | Visual root (≠ `body+0x0C`) |
| `nodes[i].world` | `World[mtxID]` | Parent-accumulated world MTX 3×4 |
| `nodes[i].viewPos` | active ViewPos slot `[mtxID]` | View-space position MTX 3×4 |
| `vHat` | `ViewPos[0]·inv(World[0])` | Snapshot-level derived view transform |
| `parentIdx` | ResNode `toParent` → ResDic index | Hierarchy; −1 = root |
| `mtxID` | ResNode `+0x10` | Palette / World index |
| `scnMdlEa` / `resMdlEa` / `modelGen` | live + change detect | Model/skeleton lifetime |
| `shellEa` / `bodyEa` / `instanceKey` | A1b live tuple | Player ownership |
| `viewSlot` / `worldGen` / `xfbIndex` | `+0x101`, CalcWorld gen, XFB | Buffer / frame identity |

ViewEnv / ViewNrm are **not** in the core pose record (A3j identities unchanged; separate extension if needed).

**Node count:** TR1 Sonic ResDic = **78** nodes; **58** in-palette (`mtxID < nView=193`) carry World/ViewPos. Completeness requires all in-palette nodes; non-palette ResNodes keep hierarchy identity only.

---

## C. Transform validation

For each complete snapshot:

1. Hierarchy parent resolution for all ResNodes.
2. World/ViewPos same XFB + associated CalcWorld ScnMdl.
3. Finite matrices; \(V̂\) from palette index 0 (invertible World[0]).
4. `ViewPos[i] ≈ V̂·World[i]` for every in-palette node; tol `kPoseRelTol=0.05` (A3k residual ≈0.002).

### TR1 residuals (`a3l_tr1_capture.log`)

| Metric | Result | Status |
|--------|--------|--------|
| Typical `relMax` | 0.00098–0.00195 | **PASS** |
| Peak observed `relMax` | 0.00391 | **PASS** (≪ 0.05) |
| Pass rate | 58/58 in-palette | **PASS** |
| Relation vs A3k ≈0.002 | Preserved | **PASS** |

Representative: `xfb=3184 scn=809882e0 … relMax=0.00195 pass=58/78 flags=… complete=1` (denominator in early log = ResDic count; in-palette pass is 58/58).

---

## D. Completeness

| Item | Value | Status |
|------|-------|--------|
| ResDic nodes | 78 | PASS |
| In-palette nodes | 58 | PASS |
| Max supported | 256 | PASS |
| ScnMdl | `809882e0` (runtime, not hardcoded) | PASS |
| `nView` | 193 | PASS |
| Complete snapshots | 5641 (timeout run) | PASS |
| Incomplete | warm-up / no-owner before stage; none after stable capture in sampled logs | PASS |
| Overflow / truncate | 0 | PASS |
| Ring overwrites | expected after slot 3 (wrap) | PASS |
| Model gen bumps | gen 1→3 observed (ResMdl/lifetime); consumer must not pair across gen | PASS |

---

## E. Temporal correspondence

Within a fixed `modelGen` + `scnMdlEa` + ResDic layout:

- Stable `nodeIdx` / `parentIdx` / `mtxID`.
- Monotonic published `seq`; XFB gaps visible as non-consecutive `xfbIndex`.
- First complete snap carries `PoseFlag_Warmup` (no previous) — normal, not failure.
- `modelGen` change → do not pair with previous generation.

---

## F. Memory and performance

| Item | Value | Status |
|------|-------|--------|
| `sizeof(PoseSnap)` | ≈26816 B | PASS |
| Ring total | 3 × ≈26.8 KiB ≈ 80 KiB static | PASS |
| Copied MTX/snap | root + 58×2×48 ≈ 5.6 KiB | PASS |
| Flags-off baseline | ~60 Hz (prior A3k / this host) | PASS |
| `SCR_POSE_CAPTURE=1` TR1 | avg `simulation_hz` ≈59.9 (min ≈58.7, max 60.0) in-stage | **PASS** |
| Capture overhead | Within noise of 60 Hz guest pacing | PASS |
| Heap in hot path | None | PASS |
| Sync file writes | None | PASS |

RTX 3080 confirmed in log (`Found device: NVIDIA GeForce RTX 3080`).

---

## G. Remaining limitations

| Item | Status |
|------|--------|
| Other stages / characters / hierarchy sizes >256 | **UNVERIFIED** |
| Non-palette ResNodes (20/78) transforms | N/A (no World slot); hierarchy only |
| Native Windows GPU | **UNVERIFIED** |
| Interpolation / presentation | Explicit non-goal |

---

## Switches

| Env | Default | Role |
|-----|---------|------|
| `SCR_POSE_CAPTURE` | off | Bounded coherent pose ring |

Host test: `scr_pose_capture_test` (math + ring; in `regression_smoke`).

---

## Final decision gate

| Question | Answer |
|----------|--------|
| Production-quality temporally coherent pose-capture foundation? | **PASS** for TR1 Sonic under call-time ownership |
| Ready to begin interpolation in this milestone? | **No** — A3l delivers snapshots only |

A separate interpolation milestone may proceed only after this contract; A3l itself must not interpolate or emit extra frames.
