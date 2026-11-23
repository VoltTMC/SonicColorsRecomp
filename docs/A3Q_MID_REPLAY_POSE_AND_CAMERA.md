# A3q — Scratch Replay Pose Injection and Full-Scene Camera Consistency

**Date:** 2026-11-23  
**Starting commit:** `5a6f03a` (A3p)  
**GPU evidence:** NVIDIA GeForce RTX 3080 · `out/logs/a3q_tr1_mid_pose4.log`  
**Scope:** Prove A2c scratch mid consumes A3n interpolated Sonic pose and a coherent camera/view policy. **Not** a sustained 2× throughput milestone.

Opt-in diagnostics: `SCR_MID_POSE_TRACE=1` / `SCR_MID_CAMERA_TRACE=1` (off by default). Force-mid probe is guest-thread only (never from the XFB/GPU thread).

---

## A. `pose=0` diagnosis

| Item | Finding |
|------|---------|
| Log field | `poseOk` boolean printed as `pose=` / `poseOk=` |
| Location | `runtime/debug/decoupled_present.cpp` · `TryUniqueMid` |
| Meaning | **1** iff `PoseInterp_BuildForHostTime` or fallback `PoseInterp_BuildAtAlpha` published an active output; **0** otherwise |
| Not | Override count, node count, or GX match count (those are `nodes=` / `gx=`) |

### Why A3p UNIQUE samples showed `pose=0`

1. **Timing:** Most UNIQUE mids fired during menu/load (archive ready, A3e-dense ~167 draws) **before** PoseCapture had two complete snaps (`complete≥2` starts ~XFB 3870+ in TR1).
2. **Scheduling:** During dense TR1, pace wait often has no residual headroom (`present_calls≈unique_xfb`), so mid rarely ran after pose was live.
3. **Archive selection (A3q fix):** Once pose was live, force-mid attempts failed `not selected` because FIFO prim `drawCount` peaked ~25k while A3e family/select capped at 16384. Raised select ceiling to **65536** (status `draws/xfb` ≈650 remains a different counter).

`pose=0` on early UNIQUE hashes was therefore **missing pose application**, not a misleading counter — but the counter itself is a bool, not an override tally.

---

## B. Pose data path (UNIQUE mid)

Correlated ID joins: build → pin → scratch → GX → present → clear.

```
PoseCapture_GetBracket / GetLatest+Previous
  → Pose_EvaluatePair (via BuildFromPair)
  → PoseInterp_BuildForHostTime | BuildAtAlpha
  → published ViewPos' (58 in-palette Worlds · V̂'·World')
  → A2c_DecoupledMidPresent + ReplayIsolation
  → GX_ReplayCommands → LoadIndexedXF → PoseInterp_OverrideIndexed
  → Render_PresentScratchUniqueMid
  → PoseInterp_ClearPublished (+ restore mid-α after diag force)
```

### Representative UNIQUE samples (`a3q_tr1_mid_pose4.log`)

| id | α | poseOk | nodes | view | seq | gx consumed | archXfb | hash |
|----|--:|-------:|------:|------|-----|------------:|--------:|------|
| 1748 | 0.579 | 1 | 58 | `8098afe0`/`8098afe0` | 1/2 | **110** | 3870 | `840e0bef` |
| 1749 | 0.523 | 1 | 58 | `8098afe0`/`8098afe0` | 31/32 | **220** | 3900 | `37c4d1bc` |
| 1750 | 0.420 | 1 | 58 | `8098afe0`/`8098afe0` | 61/62 | **220** | 3930 | `ea0c2172` |

- Canonical slots match A3j/A3n runtime ViewPos (`8098afe0` / twin `8098d420` on guest path).
- `gx` is the mid-window delta of `PoseInterp_GxOverrideCount` (overrides **consumed** by scratch indexed XF), not merely built.
- 149 UNIQUE lines with `poseOk=1` in the probe run; alphas non-endpoint (~0.42–0.58).

### A3n vs A2c boundary

Scratch uses the **same** `LoadIndexedXF` → `PoseInterp_OverrideIndexed` hook as live GX. Isolation gates **A2b capture only**; pose overrides remain active. Published buffer must be built **before** `GX_ReplayCommands`; `ClearPublished` runs after present so the next guest frame does not leak mid-α (diag force re-`BuildAtAlpha(0.5)` after mid).

---

## C. Camera / view consistency by draw category

| Category | Mid view source | Status |
|----------|-----------------|--------|
| Sonic (player ScnMdl) | A3n `ViewPos' = V̂'·World'` via indexed XF override | **PASS** (gx>0, nodes=58) |
| Static env / terrain / buildings | Archived GX command stream (sample-held at archive XFB) | **PASS** (policy) |
| Other dynamic actors | Sample-held World in archive (no extra interp) | Acceptable |
| Effects | Archived stream | Sample-held |
| A3b XF `viewMtx` | **Not** used as V̂ substitute (`a3bXF=0` in MidCamera lines) | Preserved |

`MidCamera` log: `sonic=ViewPos'(PoseInterp V̂'·World') env=archived_sample_held a3bXF=0`.

Full-scene **target-time** env camera (interpolating every ScnMdl view) remains future work; A3q proves Sonic uses target-time pose/view composition while the rest of the scene stays on one coherent archived sample (no mixed A3b XF).

Camera-cut / pair reject: fail-closed (`fb=pair_reject`, `poseOk=0`, mid may still UNIQUE from archive without pose override) — observed e.g. id=1770/1771 with `PoseInterp fallback reject=11`.

---

## D. Runtime evidence scenarios

| Scenario | Evidence |
|----------|----------|
| A — Sonic moving (TR1 script) | UNIQUE poseOk=1, α∈(0,1), gx=110–220, distinct hashes |
| B/C — camera + combined | Env sample-held; Sonic ViewPos'; V̂' from A3l pair; no A3b XF |
| D — reject/recovery | `pair_reject` → poseOk=0; later valid pairs resume apply |
| E — endpoint | Not separately forced; BuildAtAlpha(0/1) covered by host pose tests |

Probe: every 30th XFB sets a guest-pace force-mid flag (`SCR_MID_POSE_TRACE`); does **not** claim sustained 2×.

---

## E. State safety

| Concern | Result |
|---------|--------|
| Pinned packet | `A2b_PinMidPacket` / unpin in mid cleanup; EvaluatePass on that packet |
| Guest RAM (prod lean) | Mid on guest thread during/after pace enter; no Apply/Restore; live RAM for archived resources |
| `RestoreGuest` | Reverse restore retained on non-prod Apply path |
| Snapshot pin | `PoseCapture_PinSeq` around mid; unpin after |
| Output buffer | Published under mutex; cleared after mid; immutable during GX |
| GPU completion | `GX_SyncGPU` around mid present; ReplayIsolation prevents capture re-entry |
| Force-mid thread | Flag from `OnXfb` (GPU); consume in `OnGuestPaceEnter` (guest) — SyncGPU never on GPU thread |

---

## F. Performance (RTX 3080, sparse force-mid + pose trace)

| Metric | Result |
|--------|-------:|
| Guest simulation | ~60 Hz |
| Unique XFB | ~60 Hz |
| Present calls (dense TR1) | ~64/s (sparse force mids) |
| Complete armed archive | Preserved (A3p lean path) |
| UNIQUE poseOk=1 | 149 in ~145s probe |
| Sustained 2× unique | **Not claimed** |

---

## G. Remaining work (throughput milestone)

1. Sustained ~1 unique mid per guest update without force-mid steal.
2. Optional target-time env/view for non-player ScnMdls (semantics TBD).
3. Native Windows GPU verification.
4. Further mid replay cost reduction once correctness scheduling exists.

---

## Switches

| Env | Default | Role |
|-----|---------|------|
| `SCR_MID_POSE_TRACE` | off | Bounded MidPoseTrace + enriched UNIQUE logs; sparse guest force-mid |
| `SCR_MID_CAMERA_TRACE` | off | Same enable path as pose trace |
| `SCR_DECOUPLED_PRESENT` + `SCR_POSE_INTERP` + `SCR_A2B_PROD` | off | Production mid + pose path |

---

## Gate

| Criterion | Result |
|-----------|--------|
| `pose=0` meaning established | **PASS** |
| Pose path through scratch understood | **PASS** |
| UNIQUE mid builds + consumes pose (gx>0, nodes=58) | **PASS** |
| Canonical ViewPos matching | **PASS** |
| No double visual root | **PASS** (A3n semantics unchanged) |
| Coherent camera policy (Sonic interp + env sample-held) | **PASS** |
| CamCut / reject fail-closed | **PASS** |
| A3p ~60 Hz capture preserved | **PASS** |
| Sustained 2× | **FAIL** (explicit non-goal) |
| Win GPU | **UNVERIFIED** |
