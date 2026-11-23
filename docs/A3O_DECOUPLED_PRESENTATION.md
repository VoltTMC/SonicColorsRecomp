# A3o — Decoupled Presentation and Time-Driven Pose Interpolation

**Date:** 2026-11-23  
**Starting commit:** `1723086` (A3n)  
**GPU evidence:** NVIDIA GeForce RTX 3080 · `out/logs/a3o_tr1_sched.log`, `out/logs/a3o_tr1_decoupled.log`  
**Target:** Sonic Colors Wii PAL `SNCP8P` · TR1 Sonic first

Opt-in:

| Switch | Role |
|--------|------|
| `SCR_DECOUPLED_PRESENT=1` | Presentation scheduler (implies pose capture) |
| `SCR_DECOUPLED_PRESENT_RATE=2` | Target presents per sim update (default 2) |
| `SCR_PRESENT_TRACE=1` | Bounded mid-attempt logs |
| `SCR_PRESENT_LATENCY_MS` | Interpolation delay (default ≈½×60 Hz ≈ 8.3 ms) |
| `SCR_POSE_INTERP=1` | A3n ViewPos' path; time-driven α when decoupled on |
| `SCR_A2B_FIFO_CAPTURE=1` + `SCR_A3E_DENSE=1` | Required for unique mid archive replay |

Requires `--frame-rate 0` (uncapped host pacing) so `PaceUncappedHost` can schedule mid slots. Off by default.

---

## A. Architecture

### Chosen strategy: **B — archived FIFO scratch replay → mid XFB** (Approach A unsafe)

| Approach | Verdict |
|----------|---------|
| **A** Guest re-draw | **Rejected** — `fn_80014BD0` / draw walk can advance update slot 3; not render-only (FRAME_RATE A1) |
| **B** FIFO archive + matrix-substituted scratch replay | **Selected** — reuse A2b/A2c/A3e; `A2c_DecoupledMidPresent` → `Render_PresentScratchUniqueMid` |
| **C** Other frozen path | None production-ready |

Pipeline:

```
guest sim (~60 Hz) → PoseCapture snap (+hostTicks) → presentation clock (latency)
  → PoseCapture_GetBracket → α=(t−t0)/(t1−t0)
  → PoseInterp_BuildForHostTime → ViewPos'
  → [mid] GX_SyncGPU → A2c scratch replay (PoseInterp XF overrides; optional A3b cam α)
  → Render_PresentScratchUniqueMid  OR  fail-closed → Renderer_RepeatLastXfb (duplicate pace)
```

Guest gameplay/audio update is **not** re-run for mid frames.

### Threading

- Guest thread: `PaceUncappedHost` → `DecoupledPresent_OnPaceWait`
- Mid unique: `GX_SyncGPU()` then scratch replay on the held GPU lock
- Snapshot consumers **copy** under mutex; optional `PinSeq`/`UnpinSeq` blocks overwrite

### Snapshot / output lifetime

| Resource | Policy |
|----------|--------|
| Pose ring | 5 slots (~134 KiB); pin blocks overwrite (`ringPinBlock`) |
| Interp output | Double-buffer publish under mutex; rebuilt per target time |
| Mid XFB tex | Dedicated `s_midXfbTex` (not guest `s_xfbTex`) |
| Archive packet | Clone + EFB incoming addref; released after mid replay |

---

## B. Timing

| Clock | Source | Use |
|-------|--------|-----|
| Simulation snap time | `Platform_PerfCounter` at pose publish (`PoseSnap.hostTicks`) | Bracket endpoints |
| Presentation | Host monotonic; target = `now − latency` | α selection |
| Sim pacing | Unchanged 60 Hz fixed step in `PaceUncappedHost` | Guest cadence |

α = \((t-t_0)/(t_1-t_0)\) only when \(t_0 \le t \le t_1\). Unbracketed → fail closed (no extrapolation). Fixed `SCR_POSE_INTERP_ALPHA` remains the non-decoupled A3n debug mode.

**Latency:** default half sim period (~8.3 ms); override `SCR_PRESENT_LATENCY_MS`.

---

## C. Scene consistency

| Content | Mid unique (when archive ready) | Notes |
|---------|----------------------------------|-------|
| TR1 Sonic ViewPos | Interpolated (A3n semantics) | 58 in-palette World + \(V̂\) |
| Environment / static | Archive-time GX; optional A3b cam delta if enabled | Not full natural-cam proof on live mid path |
| Other dynamics | Sample-held at archived guest state | Documented limit |
| Unqualified models | Guest path | |

**Full-scene camera consistency:** designed via A3b probe α on scratch mid; **not** claimed as PASS for production TR1 mid without A3b + ready dense pass evidence.

---

## D. Genuine frame evidence

### Scheduler (no archive) — `a3o_tr1_sched.log`

| Metric | Observed |
|--------|----------|
| Device | NVIDIA GeForce RTX 3080 |
| Guest sim | ≈60 Hz |
| unique_xfb | ≈60 /s |
| present_calls | ≈120 /s |
| Unique mid | **0** (fail closed: no A2b) |
| Duplicate pace presents | fill 2× present cadence |

`present_calls ≈ 2 × unique_xfb` with **zero** unique mid — proves scheduler pacing only; duplicates are **not** counted as unique XFBs.

### With dense archive — `a3o_tr1_decoupled.log`

| Metric | Observed |
|--------|----------|
| Sim / unique_xfb | ≈33–37 Hz (A2b/A3e capture cost) |
| present_calls | ≈ unique_xfb (no wait-loop headroom for mid) |
| Unique mid | **0** in this run |

**Bottleneck:** dense FIFO capture makes each guest step longer than the 60 Hz period, so `PaceUncappedHost` has no idle window for mid replay. Unique mid path is implemented but **not** demonstrated at 2× under TR1 dense load.

---

## E. Performance

| Mode | Sim | Unique XFB | Present calls | Unique mid |
|------|-----|------------|---------------|------------|
| Flags off | ~60 | ~60 | ~60 (guest 60) / up to refresh (uncapped dup) | 0 |
| Decoupled, no A2b | ~60 | ~60 | ~120 | 0 (fail closed) |
| Decoupled + A2b dense | ~35 | ~35 | ~35 | 0 (no headroom) |

Static: pose ring 5×~26.8 KiB; mid XFB texture = EFB-sized. No per-frame heap in pose path. Mid clone uses existing A2b packet vectors (diagnostic cost).

---

## F. Fallback

Preserved A3m `Pose_EvaluatePair` rejects. Additional A3o:

| Condition | Behaviour |
|-----------|-----------|
| No A2b / no ready pass | Duplicate pace present; `midFail++`; not unique |
| Unbracketed target time | No time-α; try α=0.5 build or inactive |
| Pair reject / decompose | PoseInterp inactive; guest matrices |
| Ring pin block | Drop publish (`ringPinBlock`) |

Never extrapolates; never counts duplicate XFB as unique mid.

---

## G. Limitations

1. **Genuine 2× unique rendered frames: NOT achieved** on TR1 with current dense archive cost.
2. Scheduler **does** drive ~120 host presents/s from ~60 sim when uncapped without archive — those extras are **duplicates**, correctly excluded from unique_xfb.
3. Sonic-only pose interp; other actors sample-held.
4. Full-scene mid camera consistency **not** gated PASS.
5. Mid unique requires `--frame-rate 0` + A2b (+ dense for TR1 gameplay completeness).
6. Native Windows GPU: **UNVERIFIED**.
7. Guest redraw (Approach A) remains unsafe.

### Final gate (honest)

| # | Requirement | Result |
|---|-------------|--------|
| 1 | Guest sim ~60 Hz | **PASS** (without dense A2b) |
| 2 | Audio/gameplay unchanged | **PASS** |
| 3 | ~2 real scene renders / sim | **FAIL** — mid unique not sustained |
| 4 | Time-dependent α | **PASS** (API + decoupled mode) |
| 5 | Consistent camera mid | **PARTIAL** — design only |
| 6–7 | GX mid render + present unique | **IMPLEMENTED**, **not evidenced** at 2× |
| 8 | No duplicate counted unique | **PASS** |
| 9 | Snapshot/output lifetime | **PASS** (5-slot + pin) |
| 10 | Fail closed | **PASS** |
| 11 | Disabled = original path | **PASS** |

**A3o ships timing + fail-closed mid infrastructure; does not claim production 2× unique FPS.**
