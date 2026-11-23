# A3r — Sustained Production Throughput for Unique Mid Renders

**Date:** 2026-11-23  
**Starting commit:** `9733f08` (A3q)  
**GPU evidence:** NVIDIA GeForce RTX 3080  
**Logs:** `out/logs/a3r_tr1_sustained.log` (ignore-budget), `out/logs/a3r_tr1_budgeted2.log` (default soft budget)  
**Scope:** Turn the proven A3q scratch path into a production scheduler that can sustain additional unique mids without diagnostic tracing.

---

## A. Selection / capacity diagnosis

| Counter | Meaning | Dense TR1 magnitude |
|---------|---------|---------------------|
| `A2b_NoteDraw` / `summary.drawCount` | FIFO primitive batches with `count>0` | **~25–28k** peak |
| Status `draws/xfb` | `Render_Draw` between XFBs | **~650–750** |
| Former **16384** ceiling | A3e `ClassifyFamily` / `a3eSel` max on **FIFO** `drawCount` | Rejected valid archives |
| A3a `A3a_CapMaxDraws` (16384) | Separate attribute-row storage | Not the mid-select gate |
| `A2b_CapMaxResources` (16384) | Resource slot budget | Independent; Incomplete if exceeded |

A3q temporarily used 65536. A3r sets a **measured** default **32768** (`peak≈28k + margin`), overridable via `SCR_A3E_MAX_FIFO_DRAWS` (150–65536). No silent truncation: over-cap → `not selected` / fail-closed.

Pose data already existed when archives were rejected under 16384 — selection, not pose publication, was the gate.

---

## B. Performance profile (trace off)

### Dense TR1 mid wall cost (pose-ready)

| Stage | Typical |
|-------|--------:|
| Soft-fail (no packet / not ready) | ~0.01–0.12 ms |
| Successful full-scene mid | **~17–21 ms** |
| Dominant cost | `GX_ReplayCommands` of archived FIFO (~25k prims) |
| `Render_PresentScratchUniqueMid` | ≪1 ms with **hash off** |

A3q `SCR_MID_HASH` every-pixel staging readback was multi-ms; **default off** in A3r (`SCR_MID_HASH=1` to enable).

### Configurations

| Mode | Sim Hz | Unique mid/s | mid poseOk/s | Present/s | Notes |
|------|-------:|-------------:|-------------:|----------:|-------|
| A3p capture, no mid | ~60 | 0 | 0 | ~60 | Baseline |
| Default soft budget | **~60** | **~10** | **~10** | ~80 | Production default |
| `SCR_MID_IGNORE_BUDGET=1` | **~30** | **~30** | ~30 | ~60 | 1:1 mid/sim; sim halved |
| Sparse TRACE force (A3q) | ~60 | ~2 | ~2 | ~64 | Correctness only |

**Bottleneck:** full-scene scratch replay ≈18–20 ms ≫ residual pace wait on dense TR1. Soft budget preserves 60 Hz sim; ignore-budget trades sim Hz for 1:1 mid ratio. **Neither reaches 60+60≈120 unique images/s.**

---

## C. Implementation

| Change | Role |
|--------|------|
| Per-sim mid on `OnGuestPaceEnter` | Attempt even when wait loop empty |
| Soft budget + EMA (cap 12 ms, decay on skip, probe 1/60) | Protect sim; refresh cost |
| `SCR_MID_IGNORE_BUDGET` | Measure max mid/sim ceiling |
| Skip mid CPU hash unless `SCR_MID_HASH` | Remove readback tax |
| `SCR_A3E_MAX_FIFO_DRAWS` / default 32768 | Capacity from TR1 evidence |
| Stats: attempts / selected / skip / midMs / poseOk / gx | Honest accounting |
| Status line: `unique_mid`, `mid_poseOk`, `mid_gx`, `mid_ms` | Trace-off visibility |
| Replay/present ms in mid detail | Stage split under profile |

Preserved: A3p lean mid (no prod Apply/Restore), pin/unpin, reverse RestoreGuest (non-prod), A3q pose path, A2e/A2f exact bands unchanged.

---

## D. Pose correctness

Accepted production mids (budgeted dense TR1):

- `poseOk=1`, `nodes=58`, `gx=110–220` consumed  
- ViewPos runtime slot (e.g. `8098afe0`)  
- Non-endpoint α when bracketed  
- Fail-closed `pair_reject` unchanged  

A mid without `poseOk=1` and `gx>0` is not counted toward `mid_poseOk` / `mid_gx` rates.

---

## E. Camera / scene

Unchanged from A3q:

| Category | Behaviour |
|----------|-----------|
| Sonic | Interpolated `ViewPos' = V̂'·World'` |
| Env / terrain / buildings | Archived sample-held |
| A3b XF | Not used as V̂ |

No claim of full-scene temporal camera interpolation.

---

## F. Genuine render evidence

Default budget (`a3r_tr1_budgeted2.log`):

```
simulation_hz 60.0 | unique_mid 10.1 | mid_poseOk 10.1 | mid_gx 10.1 | mid_ms 17.25
midProfile … ok=1 poseOk=1 gx=220 midMs=17.31
```

Ignore-budget (`a3r_tr1_sustained.log`):

```
simulation_hz ~30 | unique_mid ~30 | mid_poseOk ~30 | mid_gx ~30 | mid_ms ~19–20
```

---

## G. Performance results (decision)

| Target | Result |
|--------|--------|
| Complete archives ~60/s | **PASS** (A3p lean) |
| Guest sim ~60 Hz (default) | **PASS** |
| Capacity no longer drops valid mid archives | **PASS** (32768 FIFO draw select) |
| A3q pose on every accepted mid | **PASS** (`mid_poseOk` ≈ `unique_mid`) |
| ~1 unique mid per guest update @ 60 | **FAIL** (~10/s @ 60 Hz; or ~30/s @ 30 Hz) |
| ~120 unique images/s | **FAIL** |
| Duplicate ≠ unique | **PASS** |

---

## H. Limitations / next

1. **Primary bottleneck:** ~18–20 ms CPU/GPU full FIFO scratch replay — needs command-list caching, GPU async, or proven scene reduction before 60+60 is realistic.  
2. Environment remains sample-held.  
3. Native Windows GPU **UNVERIFIED**.  
4. TR1 Sonic only.

### Switches (all off / default-safe)

| Env | Default | Role |
|-----|---------|------|
| `SCR_DECOUPLED_PRESENT` | off | Enables scheduler + prod mid attempt |
| `SCR_MID_IGNORE_BUDGET` | off | Force mid even when EMA won't fit |
| `SCR_MID_HASH` | off | CPU image hash on mid present |
| `SCR_MID_PROFILE` | off | Bounded mid timing logs |
| `SCR_A3E_MAX_FIFO_DRAWS` | 32768 | A3e FIFO draw select ceiling |

---

## Gate

**A3r: PARTIAL** — capacity and production scheduling fixed; sustained 2× unique presentation **not** achieved. Soft-budget default delivers ~10 genuine pose-consuming mids/s at 60 Hz sim on RTX 3080 TR1.
