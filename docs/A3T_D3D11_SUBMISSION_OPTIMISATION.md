# A3t — D3D11 Immediate-Context Submission Optimisation

**Date:** 2026-11-23  
**Starting commit:** `055a6dc` (A3s)  
**GPU evidence:** NVIDIA GeForce RTX 3080 via DXVK (Linux)  
**Logs:** `out/logs/a3t_tr1_submit2.log`, `a3t_tr1_texcache.log`, `a3t_tr1_fillskip.log`, `a3t_tr1_budgeted.log`  
**Scope:** Profile and reduce SCR `Flush()` / D3D11 immediate-context mid-replay cost; measure full-scene GPU independently.

---

## A. Baseline breakdown

### What `Flush()` is

SCR `Flush()` is **not** `ID3D11DeviceContext::Flush`. It is the host draw-batch submit inside `runtime/gx/renderer.cpp`:

1. Build VS/PS keys → `GetVS` / `GetPS`
2. Partial XF `UpdateSubresource` (A3s)
3. Viewport / scissor
4. `FillVSConstants` / texture resolve / `FillPSConstants` / CB upload
5. Map VB+IB (ring, DISCARD/NO_OVERWRITE)
6. Bind IA/VS/PS/OM/RS state
7. `DrawIndexed`

Call chain for mid:

`A2c_DecoupledMidPresent → GX_ReplayCommands → RunCommands → Render_Draw* → Flush → D3D11 IA/VS/PS/OM + DrawIndexed`

Replay runs on the caller under `GX_SyncGPU` (GPU worker idle). Active path: **D3D11 → DXVK → Vulkan** on RTX 3080.

### Immediate-context profile (dense TR1, pose-ready, ignore-budget)

Representative (`a3t_tr1_submit2.log`):

| Category | Typical |
|----------|--------:|
| Flush wall | **13–17 ms** / ~650–2300 `DrawIndexed` |
| **prep** (FillVS/PS + GetTexture) | **12–15 ms** |
| Map VB/IB (+ CB Maps) | **0.7–1.2 ms** |
| `DrawIndexed` CPU API | **~0.0–0.1 ms** |
| XF `UpdateSubresource` | **~0.1 ms** |
| Decode (outside Flush) | **~1.8–3.1 ms** |
| Bind skipped / total | high (OM/VS/PS/CB/tex cache) |
| CB content skips | often ≫ uploads (when Fill runs) |

### Full-scene GPU (async timestamps)

Brackets around `GX_ReplayCommands` only (not the present blit). Double-buffered `TIMESTAMP` + `TIMESTAMP_DISJOINT`, `GetData(DONOTFLUSH)`.

| Metric | Typical dense mid |
|--------|------------------:|
| **Scratch GPU execution** | **~14–20 ms** |
| Present blit GPU (existing) | ~0.03 ms |

**The ~0.03 ms blit is not the scene.** Full mid GPU work is the same order of magnitude as the CPU Flush wall.

---

## B. Bottleneck conclusion

**Primary limiting factor for 60+60:** full-scene **GPU execution ~14–20 ms** per scratch image on dense TR1. Two such scenes cannot fit in a 16.7 ms sim interval even with zero CPU overhead.

**Dominant CPU cost inside Flush:** `FillVSConstants` / `FillPSConstants` / `GetTexture` (**prep ≈ 12–15 ms**), not Map and not `DrawIndexed` API time.

**Secondary:** vertex decode ~2 ms; VB/IB Map ~1 ms.

**Not the bottleneck:** redundant OM/VS/PS binds (already largely cached); CB Map when content unchanged; XF upload.

Command-list recording would not reduce GPU triangle/TEV work. Draw batching of already-merged indexed batches (~650–2300 dix for ~25k FIFO prims) would not cut GPU below ~8 ms for this scene.

---

## C. Optimisations

| Change | Role |
|--------|------|
| Extend bind cache | Blend, depth, RTV/DSV, input layout, VS/PS CB slots, XF SRV |
| Invalidate on `DrawQuad` | Prevent A3s topo/shader cache desync after EFB util draws |
| CB content skip | `memcmp` last VS/PS constants → skip Map |
| Tex-reg fingerprint reuse | Skip `GetTexture` when map regs + usedMaps unchanged |
| VS/PS Fill fingerprints | Skip Fill when XF/BP/tex-dim fingerprint matches (low hit rate on dense TR1 — state usually changes at Flush boundaries) |
| `SCR_D3D11_SUBMIT_PROFILE` | Map/CB/bind/dix counters (also on with `SCR_MID_PROFILE`) |
| Mid GPU timestamps | Always bracketed; async resolve; `gpu=` in mid detail |

### Rejected / not shipped

| Approach | Why |
|----------|-----|
| Deferred command-list reuse | Dynamic XF + A3q pose; GPU cost unchanged |
| Async worker | Moves CPU; does not shrink ~15 ms GPU |
| Aggressive draw merge | Batches already coalesced; GPU remains |

---

## D. Correctness

- Archive completeness / 32768 FIFO select unchanged.
- A3q path: `poseOk=1`, gx 110–220 on accepted dense mids.
- Soft budget: sim ≈60 Hz with pose-consuming mids.
- Bind cache invalidated on `ResetHostDrawState` and util `DrawQuad`.
- Pose never cached across target times.
- A2e/A2f bands untouched; `regression_smoke` OK; MinGW link OK.

---

## E. Performance (trace-off / profile-bounded)

### Soft budget — `a3t_tr1_budgeted.log`

| Metric | Typical |
|--------|--------:|
| Simulation | **~60 Hz** |
| Unique mid / mid_poseOk / mid_gx | **~9–10 /s** |
| Mid wall | **~17–19 ms** |
| Scratch GPU | **~15–19 ms** |

### Ignore-budget — `a3t_tr1_fillskip.log`

| Metric | Typical |
|--------|--------:|
| Simulation ≈ unique mid | **~33–37 Hz** |
| Mid wall / GPU | **~17–20 / ~14–20 ms** |

### Targets

| Target | Result |
|--------|--------|
| CPU Flush / submission &lt; ~8 ms | **FAIL** (prep still ~12 ms) |
| GPU headroom for 60+60 | **FAIL** (GPU alone ~14–20 ms/scene) |
| Sustained 60+60 unique | **FAIL** |
| Sim ~60 with soft budget | **PASS** |

---

## F. Limitations

1. Dense TR1 full-scene scratch is **GPU-bound at ~14–20 ms** on RTX 3080/DXVK; CPU prep is a second ~12 ms tax.
2. Meeting a CPU-only &lt;8 ms target would still leave GPU over the 8.33 ms 120 Hz slot.
3. Environment / terrain / buildings remain **sample-held** (A3q).
4. Native Windows D3D11 **UNVERIFIED** (MinGW links; Linux numbers are DXVK).
5. Fill/tex fingerprints help only when adjacent Flushes share state — dense TR1 often changes state at each Flush boundary.

### Switches (default-safe)

| Env | Default | Role |
|-----|---------|------|
| `SCR_MID_PROFILE` | off | Mid stage + implies submit counters in detail |
| `SCR_D3D11_SUBMIT_PROFILE` | off | Explicit submit counters |
| Mid GPU timestamps | always on mid path | Async; no hot-path block |

---

## Gate

**A3t: PARTIAL** — submission profile and full-scene GPU cost established; bind/CB/tex micro-opts shipped; **&lt;8 ms CPU and 60+60 FAIL** because dense TR1 scratch GPU ≈15–20 ms. Further 2× work must reduce **GPU scene cost** or change scheduling assumptions — not more immediate-context bind caching alone.
