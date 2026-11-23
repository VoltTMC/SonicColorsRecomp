# A3s — Cached GX Replay and Asynchronous Scratch Rendering

**Date:** 2026-11-23  
**Starting commit:** `8991926` (A3r)  
**GPU evidence:** NVIDIA GeForce RTX 3080  
**Logs:** `out/logs/a3s_tr1_profile.log` (pre-bindcache), `out/logs/a3s_tr1_bindcache.log` (ignore-budget), `out/logs/a3s_tr1_budgeted.log` (soft budget)  
**Scope:** Profile `GX_ReplayCommands`, choose the smallest safe optimisation supported by evidence, preserve A3q pose + A3p archives.

---

## A. Baseline diagnosis

### Meaning of the 17–21 ms `GX_ReplayCommands` wall

Mid path calls `GX_ReplayCommands` on the **caller thread** under `GX_SyncGPU` (GPU worker idle). The host backend is **D3D11 via DXVK**, not a Vulkan command-buffer API owned by SCR. Wall time inside the replay function is therefore:

| Component | Typical dense TR1 (pose-ready) | Notes |
|-----------|-------------------------------:|-------|
| Vertex decode (`VertexLoader_Decode`) | **~1.8–3.4 ms** | Scales with FIFO prim count (~24–32k) |
| XF upload (`UpdateSubresource`) | **~0.0–0.1 ms** | After A3s partial dirty-range upload |
| `Flush()` aggregate | **~13.5–19 ms** | ~650–2500 `DrawIndexed` batches |
| Present / unique mid | **~0.1 ms** (often); up to several ms under load | Hash off by default |
| **End-to-end mid** | **~17–30 ms** | Dominated by Flush |

`SCR_MID_PROFILE` stage string (example, ignore-budget, post bind-cache):

```
poseOk=1 gx=220 midMs=20.22 | … replay=19.7ms present=0.1ms
  flush=15.3ms/1284 xfUp=0.1ms decode=2.2ms/24310 dix=1284
```

### CPU vs GPU vs sync

- **Primary bottleneck:** CPU-side D3D11 immediate-context **Flush** (Map VB/IB, state setup, `DrawIndexed` through DXVK). ~75–85% of mid wall.
- **Decode** is secondary (~10–15%). A full predecoded command plan would save ≈2 ms, not reach &lt;8 ms.
- **GPU blit** (existing timestamp queries around present) remains **~0.03–0.04 ms** — that measures the XFB blit, not the full scratch scene.
- Mid does **not** fence-wait for GPU completion before returning from replay; occasional multi-ms `present=` correlates with swap/queue pressure, not a deliberate mid fence.
- **Do not interpret** Flush wall time as pure GPU execution. Map/`DrawIndexed` cost is CPU + driver + possible ring-stall; a second full scene still needs GPU work for every mid.

### Main measured bottleneck

**`Flush()` / host draw submission of the archived full scene** (~14–19 ms), not FIFO opcode parse, XF upload, or pose override.

---

## B. Replay architecture

### Existing path (unchanged semantics)

```
PoseCapture_GetBracket → Pose_EvaluatePair → PoseInterp_BuildForHostTime
  → GX_ReplayCommands → LoadIndexedXF → PoseInterp_OverrideIndexed
  → Render_PresentScratchUniqueMid
```

### Optimisations evaluated

| Option | Verdict | Reason |
|--------|---------|--------|
| **A. Predecoded GX replay plan** | **Not shipped** | Decode ≈2 ms; does not move Flush |
| **B. Cached resource / state prep** | **Shipped (narrow)** | Skip redundant VS/PS/topo/cull/viewport/scissor/SRV/sampler binds across consecutive `Flush()`; partial XF `UpdateSubresource` dirty range |
| **C. Reusable D3D11 deferred command lists** | **Rejected** | Dynamic per-stream XF + A3q pose injection; recording stale matrices into a CL is incorrect; DXVK deferred-CL reuse not a proven win for this path |
| **D. Incremental segment reuse** | **Rejected** | Dense TR1 streams change; no generation evidence that large static segments dominate Flush |

### Dynamic pose / view injection

Unchanged from A3q: target-time World (58 nodes) + snapshot `V̂` → ViewPos; overrides applied at `LoadIndexedXF`. Under `Render_ReplayIsolation`, `PoseInterp_OverrideIndexed` skips the mutex (hundreds of indexed XF loads per mid). **Pose results are never cached across presentation times.**

### Cache identity / invalidation (bind cache)

Host bind cache is **not** keyed by guest EA. It is invalidated on `Render_ResetHostDrawState` (replay isolation entry/exit) and on any VS/PS/topo/cull/viewport/scissor/texture bind change. Stale binds are fail-closed by pointer/value mismatch.

---

## C. Async processing

**Not implemented.**

Rationale: the critical path is serial Flush on the D3D11 immediate context used for both guest and scratch rendering. Overlapping archive decode on a worker would at best hide ≈2 ms of decode; it cannot make two full-scene Flushes fit into a 16.6 ms guest interval on one GPU/context without starving sim. No second unsynchronised queue owner was introduced.

---

## D. Correctness

| Check | Result |
|-------|--------|
| Command ordering / completeness | Preserved (same `GX_ReplayCommands`) |
| A3p archive / 32768 FIFO select | Unchanged |
| A3q `poseOk=1`, 58 nodes, gx 110–220 | Observed on accepted dense mids |
| Target-time α | Non-endpoint α in midProfile lines |
| Env / terrain / buildings | Archived sample-held (A3q policy) |
| A2e / A2f exact bands | Untouched |
| Features default off | Yes (`SCR_DECOUPLED_PRESENT`, `SCR_MID_PROFILE`, etc.) |

---

## E. Performance (trace-off / profile-bounded)

### Soft budget (production default) — `a3s_tr1_budgeted.log`

| Metric | Typical |
|--------|--------:|
| Guest simulation | **~60 Hz** |
| Unique mid / mid_poseOk / mid_gx | **~4–6 /s** |
| Mid wall (`mid_ms`) | **~19–20 ms** |
| Flush / decode / xfUp | ~14–15 / ~2.2 / ~0.1 ms |
| Present | ~0.1 ms |
| GPU blit | ~0.03 ms |

### Ignore-budget — `a3s_tr1_bindcache.log`

| Metric | Typical |
|--------|--------:|
| Guest simulation | **~19–27 Hz** |
| Unique mid ≈ sim | **~19–27 /s** |
| Mid wall | **~20–30 ms** (Flush still dominates) |

### Targets

| Target | Result |
|--------|--------|
| Mid critical path &lt; ~8 ms | **FAIL** (~17–30 ms remains) |
| Sustained ~60 additional unique mids/s @ 60 sim | **FAIL** |
| Combined GPU utilisation for 60+60 | **Insufficient headroom** — one dense mid ≈ one guest frame of wall |

Bind-cache + partial XF did **not** move Flush under 8 ms. Predecoded plans and async submission would not either, given the measured stage split.

---

## F. Genuine-render evidence

Representative mid (`a3s_tr1_bindcache.log`):

```
ok=1 poseOk=1 gx=220 midMs=20.22 α=0.835
draws=24310 flush=15.3ms/1284 decode=2.2ms dix=1284
present=0.1ms  (SCR_MID_HASH off → hash=00000000)
```

Budgeted accepted mid (`a3s_tr1_budgeted.log`):

```
ok=1 poseOk=1 gx=220 midMs=19.02 α=0.477
draws=24207 flush=14.4ms/649 decode=2.2ms
simulation_hz ≈60 with unique_mid ≈5/s
```

---

## G. Limitations

1. **Below-8-ms mid target: not achieved.** Flush/D3D11 batch path remains ~14–19 ms on dense TR1.
2. **60+60 unique images/s: not achieved.** Soft budget protects ~60 Hz sim at ~5 mid/s; ignore-budget ≈1:1 at ~20–30 Hz.
3. **Limiting factor:** host Flush / draw submission of a full archived scene (CPU+driver), with a second full GPU scene still required per mid. Not FIFO decode; not XF upload; not pose mutex.
4. Environment / terrain / buildings remain **sample-held**.
5. Native Windows GPU **UNVERIFIED** (MinGW link OK).
6. Backend is D3D11/DXVK — Vulkan secondary-CB reuse from the A3s brief does not map 1:1.

### Switches (default-safe)

| Env | Default | Role |
|-----|---------|------|
| `SCR_MID_PROFILE` | off | Bounded flush/decode/xf/dix mid timers |
| `SCR_MID_IGNORE_BUDGET` | off | Force mid (may cut sim Hz) |
| `SCR_MID_HASH` | off | CPU mid image hash |
| `SCR_A3E_MAX_FIFO_DRAWS` | 32768 | Unchanged from A3r |

---

## Gate

**A3s: PARTIAL** — profiling complete; small Flush/XF/pose-path opts shipped; cached command-list / async worker **correctly declined**. Mid critical path remains **above ~8 ms**. Genuine 2× rendering is **not** approached until Flush cost (or equivalent GPU scene cost) is reduced without dropping commands or visual quality.
