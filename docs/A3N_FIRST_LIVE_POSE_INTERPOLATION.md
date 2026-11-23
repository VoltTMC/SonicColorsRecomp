# A3n — First Live Pose Interpolation (TR1 Sonic)

**Date:** 2026-11-23  
**Starting commit:** `db3db2c` (A3m)  
**GPU evidence:** NVIDIA GeForce RTX 3080 · `out/logs/a3n_tr1_interp*.log` / `a3n_tr1_gxfix.log`  
**Scope:** First live interpolated Sonic pose through existing GX. **No extra XFBs / no higher-FPS claim.**

Opt-in: `SCR_POSE_INTERP=1` (enables A3l capture). Optional: `SCR_POSE_INTERP_ALPHA`, `SCR_POSE_INTERP_TRACE`, `SCR_POSE_INTERP_VALIDATE`. Off by default.

---

## A. Architecture

| Stage | Implementation |
|-------|----------------|
| Snapshots | A3l ring (`PoseCapture_OnXfb`) |
| Pair select | `Pose_EvaluatePair` (A3m) — reject → inactive output |
| Alpha | Fixed `SCR_POSE_INTERP_ALPHA` (default **0.5**) = deliberate mid-bracket latency at existing present cadence |
| World' | Rigid TRS SLERP of 58 in-palette nodes |
| \(V̂'\) | Rigid interp of snap `vHat` (not A3b XF viewMtx) |
| ViewPos' | \(V̂'·World'\) into preallocated palette buffer |
| GX | `LoadIndexedXF` substitutes floats when CP array base CanonAddr-matches ViewPos slot0/slot1 |
| Fallback | Publish inactive → guest ViewPos unchanged |

Output lifetime: published under mutex for the following GPU draws of that XFB window; replaced or cleared next XFB.

---

## B. Transform implementation

- Matrix: GC 3×4 row-major (same as A1b/A3l); concat = PSMTXConcat order.
- Decompose: column lengths → orthonormalize → Shepperd quaternion; reject shear/reflection/singular/non-uniform.
- SLERP: normalize, shortest-path sign flip, nlerp near identity.
- Translation: linear lerp.
- Scale: uniform only (TR1 World = Rigid, scale≈1).
- Endpoints: α=0/1 reconstruct to source (host-tested).
- Validate mode: reconstruct residual ≈0 by construction; endpoint errors tracked.

**Root:** not re-applied. World already includes CalcWorld from `ScnMdl+0x3C`.

---

## C. Identity and safety

- Player ScnMdl via call-time ownership (A3k/A3l).
- 78 ResDic identities retained in snaps; **58** in-palette Worlds interpolated.
- Never pair across `ModelGen` / CamCut / Teleport / SeqGap (A3m policy).
- Guest snapshot buffers never mutated.
- Other models: CP bases ≠ ViewPos slots → no override (e.g. ViewEnv `80995320` correctly missed).

---

## D. Runtime results (TR1)

| Metric | Result | Status |
|--------|--------|--------|
| RTX 3080 | Confirmed | PASS |
| Apply with 58 nodes | Thousands (e.g. 5626 in long run) | PASS |
| ViewPos slots | `8098afe0` / `8098d420` | PASS |
| ViewPos' residual | 0 (by construction) | PASS |
| GX overrides | **>0** after CanonAddr fix (`gx≈176k` in 70s sample) | **PASS** |
| CamCut fallback | reject=11 observed | PASS |
| `simulation_hz` | ≈59.7–60 | PASS |
| Extra XFBs | None | PASS |

**GX consumption proof:** `LoadIndexedXF` → `PoseInterp_OverrideIndexed` with `CanonAddr(CP base)` ∈ {ViewPos slot0, slot1}; counters `gxOverrides` increment in TRACE logs (`a3n_tr1_gxfix.log`).

Miss bases include unrelated palettes / ViewEnv — expected.

---

## E. Performance

| Mode | Guest sim |
|------|-----------|
| Interp off | ~60 Hz |
| Interp on (+ validate/trace) | ~59.7–60 Hz |

Static: A3l ring ~80 KiB + interp output ~same order (palette ViewPos' × 256). No per-frame heap; no file I/O.

---

## F. Limitations

- TR1 Sonic only.
- Fixed alpha mid-bracket (not wall-clock display scheduling).
- No additional presentation frames / no FPS claim.
- White Wisp / respawn / other stages: **UNVERIFIED**.
- Native Windows GPU: **UNVERIFIED**.
- A3b XF view still not used as \(V̂\).

---

## Final gate

A3n **PASS**: interpolated World + \(V̂\) → ViewPos' reaches GX indexed XF loads for the live player ViewPos palette, with safe fallback and ~60 Hz guest sim.
