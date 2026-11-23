# A3v — Production Player-Pass Replay, Occlusion Qualification and Cadence

**Date:** 2026-11-23  
**Starting commit:** `5b00b14` (A3u)  
**GPU evidence:** NVIDIA GeForce RTX 3080 via DXVK (`tools/run_nvidia_clean.sh`)  
**Logs:** `out/logs/a3v_tr1_budgeted3.log`, `a3v_tr1_ignorebudget.log`, `a3v_tr1_budgeted2.log`  
**Scope:** Reusable truncated player plan, pre-decode skip, soft-budget diagnosis/fix, post-player occlusion classification. Opt-in `SCR_A3U_PLAYER_COMPOSITE` (unchanged default off).

---

## A. Sparse plan

### Classification (unchanged ownership rule)

ViewPos guest EA → XF matrix index `xfAddr/12`; non-ViewPos overwrite clears the bit. TR1: **~1506 player draws**, **~27 before**, **~23.6k non-player after first player**.

Counter semantics:

| Counter | Meaning |
|---------|---------|
| `drawsPlayer` | Live draws whose PNMTX rows hit the ViewPos mask |
| `drawsAfter` | Non-player draws after the first player (interleaved + trailing) |
| `drawsAfterLast` | Non-player draws after the **final** player draw (trailing only) |
| Status `draws/xfb` | Host DrawIndexed batches per XFB (≠ FIFO prim count) |

Typical TR1: `afterLast ≈ 127` while `after ≈ 23600` → nearly all “after” draws are **interleaved** during the player span, not a clean post-player tail.

### Plan contents (A3v)

Per background/`xfbIndex` slot:

1. Record `fifoOff` at pre-player EFB capture (first ViewPos XF).
2. Record `playerCmdEnd = A2b_CurrentCmdBytes()` on each player draw.
3. Mid consumes archive bytes **`[fifoOff, playerCmdEnd)`** (truncation window).
4. No offline opcode strip — display lists make dry-run strip unsafe (A3v attempt failed: `kept=0`).
5. Interleaved non-player draws remain in the window; replay skips them via `A3u_ShouldSkipDraw` **before** `VertexLoader_Decode`.

Cache identity: `(xfbIndex, fifoOff, playerCmdEnd)`. Rebuild is O(1) (store end offset; pointer into pinned archive). Hits observed under ignore-budget (multiple mids / same window).

| Cost | Result |
|------|--------|
| Plan build | ~0.000 ms (offset only) |
| Mid replay CPU (accepted) | **~1.3–1.8 ms** (was ~4.5–5.5 ms in A3u) |
| Mid GPU | **~0–1.7 ms** typical |

---

## B. Post-player / interleaved content

### Distribution (live BP heuristic at non-player draws in player span)

From `SCR_A3U_OCCLUSION_TRACE` / audit (`occ=opaque/blend/other`):

| Category | Approx. count / XFB | Notes |
|----------|--------------------:|-------|
| Opaque-ish (z on, blend off) | ~23 100 | Depth occluder **candidates** |
| Blend on | ~530–550 | Transparency / FX candidates |
| Other | ~5 | |
| Trailing only (`afterLast`) | ~127 | Truncated from mid plan |

### Handling (choice **C** + truncation)

- Trailing ~127 cmds: dropped by plan window.
- Interleaved ~23k: still in the byte window but **not drawn** on mid (skip).
- Normal guest XFB remains complete.
- Mid is **not** visually equivalent to the full guest frame for occlusion/transparency that interleaved with Sonic.

No full-scene replay and no occlusion-only depth pass shipped this milestone (would need proven stencil/depth semantics). Explicit sample-held limitation.

---

## C. Background retention

Unchanged from A3u Strategy A: EFB colour+depth `CopyResource` at first ViewPos XF; 4-slot ring; restore into scratch before player pass; clears suppressed during player-only mid; sample-held camera (no A3b XF view).

---

## D. Scheduler diagnosis (6–8/s → ~8–10/s)

### Root causes

1. **Budget EMA included `GX_SyncGPU` wait** (guest GPU drain), often >> player work → soft budget rejected most attempts (`emaCap` 12 ms + 2 ms reserve vs ~5–8 ms remain after guest).
2. **After guest update+render**, pace remain is often only a few ms; mid must fit in that residual.
3. Readiness gaps (no dense archive / no bg) during menus dilute attempts.
4. Mid CPU was inflated by **decoding** ~23k skipped interleaved draws.

### Fixes

| Change | Effect |
|--------|--------|
| Work EMA excludes SyncGPU | Predicts ~0.4–2 ms player work |
| A3u reserve 0.5 ms, ema cap 7 ms | Fits more residual windows |
| Ignore sub-0.5 ms failures in EMA | Avoids collapse to ~0 then odd skips |
| Pre-decode skip | Mid CPU **~1.6 ms** (was ~4.5 ms) |
| Truncation plan | Drops trailing cmds; O(1) reuse |
| Denser pace spacing when A3u | `freq/120` present gate |

### Measured soft budget (`a3v_tr1_budgeted3.log`)

| Metric | Typical / peak |
|--------|----------------|
| `simulation_hz` | **~60** |
| `unique_mid` (poseOk+gx) | **~4–10/s** (peak **~9.7/s**) |
| Accepted mid wall | **~1.6–1.8 ms** |
| Sync | **~0.05 ms** when GPU already idle |

### Ignore budget

~29 Hz sim + **~15–17** unique mid/s (similar ceiling to A3u; not production).

**60+60 not achieved.** Limiter: residual pace window after guest work + archive readiness, not player GPU (~1 ms).

---

## E. Correctness

| Check | Result |
|-------|--------|
| Old Sonic absent from bg | PASS (pre-player capture) |
| Depth restore + test | PASS |
| A3q gx overrides on accepted mids | PASS (`gx≈110`, `dix≈40–50`) |
| Empty pass fail-closed | PASS |
| Normal path when disabled | Unchanged |
| Mid vs full guest occlusion | **Not equivalent** (interleaved skip) |
| Camera | Sample-held bg camera |

---

## F. Performance summary (RTX 3080)

| Mode | Sim Hz | Unique mid/s | Mid CPU | Mid GPU |
|------|-------:|-------------:|--------:|--------:|
| A3u soft (baseline) | ~60 | ~6–8 | ~4.5–5.5 ms | ~1.3–1.9 ms |
| **A3v soft** | ~60 | **~4–10** (peak ~9.7) | **~1.6–1.8 ms** | ~0–1.7 ms |
| A3v ignore-budget | ~29 | ~15–17 | ~5–6 ms wall | — |
| A3t full scratch | — | — | ~17–22 ms | ~14–20 ms |

---

## G. Limitations

- Interleaved env/FX during Sonic’s draw span remain sample-held on mid (skipped).
- Trailing post-player truncated; not replayed.
- Environment/camera sample-held.
- Soft unique_mid ≪ 60/s; full-scene 2× unachieved.
- Offline strip of non-player opcodes not safe (DL nested draws).
- Native Windows GPU **UNVERIFIED**.

## Controls

| Env | Default | Role |
|-----|---------|------|
| `SCR_A3U_PLAYER_COMPOSITE` | off | Enable path |
| `SCR_A3U_AUDIT` | off | Ordering / mid logs |
| `SCR_A3U_PLAN_PROFILE` | off | Truncation window logs |
| `SCR_A3U_BUDGET_TRACE` | off | Soft-budget skip/probe |
| `SCR_A3U_OCCLUSION_TRACE` | off | Trailing/occ category logs |

## Tests

`scr_a3u_classify_test`, `scr_pose_capture_test`, Linux + MinGW builds, TR1 via `tools/run_nvidia_clean.sh`.
