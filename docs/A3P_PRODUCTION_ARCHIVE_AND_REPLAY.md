# A3p — Production-Efficient Full-Scene Archive and Scratch Replay

**Date:** 2026-11-23  
**Starting commit:** `6681bcb` (A3o)  
**GPU evidence:** NVIDIA GeForce RTX 3080 · `out/logs/a3p_tr1_*.log`  
**Target:** Sonic Colors Wii PAL `SNCP8P` · TR1 first

Opt-in (all off by default):

| Switch | Role |
|--------|------|
| `SCR_A2B_PROD=1` | Lean production capture (also auto-on with `SCR_DECOUPLED_PRESENT`) |
| `SCR_A2B_PROFILE=1` | Bounded stage timers → periodic `A2bProf` log lines |
| `SCR_DECOUPLED_PRESENT=1` | A3o scheduler (implies prod capture + pose capture) |
| `SCR_POSE_INTERP=1` | Time-driven mid pose (when brackets exist) |

---

## A. Performance diagnosis

### Original A3o 35 Hz bottleneck

| Hypothesis | Evidence |
|------------|----------|
| Dense FIFO + resource copy alone | Incomplete — **pre-arm** (armed=0) was already ~35 Hz |
| Soft-arm still ran heavy closure | **Confirmed** — `A2c_ClosureCaptureEnabled` == EnvOn → `CaptureIndexedArrays` every draw while unarmed |
| EFB incoming every pass | **Confirmed** — `A2b_OnPassEntryEfb` ran on EnvOn; `Render_EfbIncomingCapture` + every-pixel CPU hash |

### Capture vs replay (after fixes)

| Mode | Sim Hz | Notes |
|------|-------:|-------|
| Baseline (flags off) | ~60 | |
| Prod capture, soft-arm (armed=0) | ~60 | After gating closure + EFB on `CaptureActive` |
| Prod capture, armed, dense TR1 | ~60 | Profile ~1–2 ms/frame accounted |
| Decoupled + prod + interp | ~60 | Guest XFB ~60; present_calls often ~60 (no mid wait headroom) |

### Stage profile (armed dense TR1, `SCR_A2B_PROFILE`)

Representative steady `A2bProf` (per-frame averages):

| Stage | Cost | Notes |
|-------|-----:|-------|
| Indexed-array / closure | ~1.0 ms → ~0.1–0.7 ms | Worst-case span + covering / sat mask |
| Resource sample-fp + copy | ~0.3–0.5 ms | Address-keyed blob cache; no full O(n) hash in prod |
| FIFO append | ~0.15 ms | Incremental FNV |
| EFB incoming | ~0 after first | Prod skips CPU texture hash |
| Meta EvaluatePass | ~0.05 ms | Cmd meta clone; pass hash skipped in prod |

**Main historical bottleneck:** per-draw index walks + EFB every-pixel hashing while soft-arm/`EnvOn`, not mid replay.  
**Remaining mid bottleneck:** full-scene `GX_ReplayCommands` cost vs residual `PaceUncappedHost` wait — sustained mid slots are scarce when guest already fills the 16.7 ms budget.

---

## B. Optimisations

| Change | Exactness |
|--------|-----------|
| Soft-arm draw obs via `A2b_NoteDraw` + `A3e_NoteObservedDraws` (no A3a required) | Capture still off until armed |
| `A2c_ClosureCaptureEnabled` → `A2b_CaptureActive` | Heavy closure only when armed |
| EFB incoming only when `CaptureActive`; prod skips content hash | GPU copy retained for seed; MATCH path keeps hashes |
| Incremental FIFO FNV; res hashmap; covering spans; blob cache by EA+sample-fp | Non-prod keeps full `HashBytes` |
| Prod vertex capture: index-width worst-case span (≤64 KiB), MEM bounds clamp | Superset of legal indices up to cap; truncated only on MEM/cap limits |
| Lazy mid pin; prod mid skips `resData` clone + Apply/Restore (live guest RAM during pace wait) | Diagnostic MATCH still applies archive blobs |
| Mid `EvaluatePass` on pinned/current packet (not stale family report) | Prevents passBytes drift |
| Reverse-order `RestoreGuest` | Overlapping Apply unwind |
| Dense family `deps` floor 1 in prod | Covering dedupe must not demote A3eDense → Other |

---

## C. Archive completeness

| Metric | Observed (TR1 armed) |
|--------|----------------------|
| Peak cmd | ~257 KiB |
| Peak res bytes | ~8–14 MiB |
| Soft-arm → armed | draws≥150 or stage enter |
| Complete dense selection | Intermittent `A3e-dense` ready (see mid UNIQUE samples) |
| Overflow | Caps unchanged; incomplete → fail-closed |

Exact A2e/A2f MATCH paths remain behind non-prod `SCR_A2B_FIFO_CAPTURE` / `SCR_A2C_SCRATCH_REPLAY` (full hash + Apply).

---

## D. Camera / view consistency

| Item | Status |
|------|--------|
| Mid uses A3n ViewPos' when `PoseInterp_BuildForHostTime` succeeds | Wired |
| Logged TR1 mids | `pose=0` — brackets/apply not ready in those samples; fixed α=0.5 fallback |
| Full-scene mid camera | **PARTIAL** — environment follows archived GX; player interp not proven on UNIQUE samples |
| Sample-held dynamics | Acceptable when pose build fails (fail-closed α debug only) |

---

## E. Unique render evidence

From `out/logs/a3p_tr1_decoupled5.log` (and siblings):

| Event | Example |
|-------|---------|
| Archive | A3e-dense, draws=167 / 4617 / 831 |
| Scratch | `mid-OK` + `Render_PresentScratchUniqueMid` |
| Hashes | Distinct across UNIQUE lines (`a65f84b1`, `642b4414`, `c4f02638`) |
| Cadence | Sparse — not one mid per guest XFB |
| Duplicates | Fail-closed → `Renderer_RepeatLastXfb` (not counted unique) |

---

## F. Performance summary (RTX 3080)

| Metric | Capture-only prod | Decoupled + prod |
|--------|------------------:|------------------:|
| Guest simulation | ~60 Hz | ~60 Hz |
| Complete archive attempts | ~1 / XFB when armed | same |
| Scratch unique / s | 0 (no scheduler) | **sparse** (few UNIQUE / run) |
| Present calls / s | ~60 | often ~60 (no wait headroom) |
| Archive RSS delta | +~100–150 MiB vs baseline | similar |

---

## G. Limitations / gate

| Gate item | Result |
|-----------|--------|
| Complete TR1 archives ~60/s | **PASS** (armed dense) |
| Sim ~60 Hz with capture | **PASS** |
| Archive reusable for scratch | **PASS** (demonstrated) |
| Lifetimes through GPU mid | **PASS** with pin/unpin + EFB slot rules |
| Full-scene mid camera consistency | **PARTIAL** (`pose=0` on samples) |
| ~1 unique mid per guest update | **FAIL** (replay vs wait headroom) |
| Genuine presented mid image | **PASS** when UNIQUE fires |
| Duplicate XFB ≠ unique | **PASS** |
| Sim/audio unchanged by mid | **PASS** |
| Exact MATCH tests | Non-prod path preserved; run in CI/host tests |

**A3p does not claim sustained 120 FPS / 2× unique presentation.** It claims production capture is efficient enough to restore ~60 Hz simulation with complete dense archives, and that unique mid scratch replay works when scheduling headroom exists.

**Native Windows GPU:** UNVERIFIED.

---

## Logs

- `out/logs/a3p_tr1_baseline.log` — flags off ~60 Hz  
- `out/logs/a3p_tr1_capture_only5.log` — prod capture ~60 Hz armed  
- `out/logs/a3p_tr1_decoupled5.log` — decoupled + UNIQUE samples  
