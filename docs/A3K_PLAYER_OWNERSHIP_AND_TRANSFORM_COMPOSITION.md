# A3k — Persistent Player Ownership and Root Transform Composition

**Commit base:** `7551316` (A3j). **Target:** SNCP8P Rev.00.  
**GPU evidence:** NVIDIA GeForce RTX 3080 via `tools/run_nvidia_clean.sh` + `tools/a2g_tr1_enter.txt`.  
**Implementation:** `runtime/debug/a3k_owner_transform.*` (opt-in). Extends A3i CalcWorld observation; hooks `CModelImplCreateWrap` (`803C6920`) for create registry only.

| Switch | Role |
|--------|------|
| `SCR_A3K_OWNER_LIFETIME` | ModelImpl create registry + reverse body→impl search |
| `SCR_A3K_BODY_TRANSFORM` | `body+0x0C` / `+0x168` via `BodyGetWorldMtx` |
| `SCR_A3K_MODEL_ROOT` | `ScnMdl+0x3C` root mtx (CalcWorld → WorldHier arg) |
| `SCR_A3K_WORLD_VIEW_COMPOSE` | World / ViewPos / V̂·W residuals |

Disabled by default. No G3dProc / ViewA `PPC_OVERRIDE`. No interpolation. A3j palette identities preserved.

---

## A. Ownership lifecycle

| Hop | Evidence | Status |
|-----|----------|--------|
| Live shell / body / inst | `80b58b00` / `90f9c6c0` / `27a5990d` | **PASS** |
| CalcWorld `this` while live | `809882e0`, `nView=193`, every TR1 XFB | **PASS** |
| `CModelImplCreateWrap` (`803C6920`) | Stores `ScnMdl*` at `ModelImpl+0x1C` (create @ `803C6D4C`) | **PASS** (for that factory) |
| Create registry → `809882e0` | 88 creates observed; **zero** hold `809882e0` at `+0x1C` | **REJECTED** for player |
| Body/shell BFS → ModelImpl/ScnMdl | No stored path (same as A3j) | **UNRESOLVED** as direct ptr |
| Call-time association | Live shell/body concurrent with CalcWorld `this=809882e0` | **PASS** |

**`CModelImpl+0x1C`:** Proven as the ScnMdl slot for `SonicUSA::Graphics::Model::CModelImplNWBase<ScnMdl>` objects constructed through `803C6920→803C6D4C`. **Disproven** as the ownership path for the live TR1 player ScnMdl `809882e0` (that instance never appears in the create registry; `c1c=Rejected` with creates>0).

Illustrative (call-time / scene scheduling, not a body field):

`Player shell/body live → ScnRoot G3dProc task1 → CalcWorld(this=809882e0) → World/ViewPos → GX`

---

## B. Ownership conclusion

**Call-time ownership proven and sufficient** for identifying the live player model across TR1 XFBs and animation states tested by `a2g_tr1_enter.txt`.

| Class | Result |
|-------|--------|
| Persistent direct body→ScnMdl* | **Not found** |
| Persistent ModelImpl under body graph | **Not found** for `809882e0` |
| Scheduler / create registration | CModelImpl factory **exists** but **not used** for this player ScnMdl |
| Call-time CalcWorld `this` | **PASS** — stable `809882e0`, `nView=193`, pals unchanged from A3j |

Future pose capture can key on CalcWorld `this` (or A3j primary) while the live shell/body identity is present. A durable body-stored pointer is not required by the guest architecture for this model.

---

## C. Transform stages

| Stage | Guest / field | Coordinate space | Status |
|-------|---------------|------------------|--------|
| Body world | `body+0x0C` (`BodyGetWorldMtx` `80053718`) | Actor mtx; T often ~0 | **PARTIAL** (not visual root) |
| Body position | `body+0x168` | World position (A1b); TR1 script often stale ~(0,1,0) | **PARTIAL** |
| Model / ScnObj root | `ScnMdl+0x3C` (CalcWorld passes to WorldHier `805AEF00`) | World | **PASS** |
| ResNode local / parent | A3h `toParentNode` / stride `0xD0` | Local → parent accumulate | **PASS** (unchanged) |
| World calculation | `CalcWorld` `805B87D0` → World array `+0xEC` | World | **PASS** |
| View calculation | G3dProc task5 → `805AE820` ViewA | Camera-relative | **PASS** (disasm + math) |
| ViewPos | `+0xF4` slots `8098afe0`/`8098d420` | View | **PASS** (A3j) |
| ViewNrm | `+0xF8` `8098f860` | View normals (not position) | **PASS** (role) |
| ViewEnv | `+0xFC` `80992ee0`/`80995320` | Env/specular family | **PASS** (A3j) |
| GX indexed XF | A3h loads from ViewPos/ViewEnv | Same as palette | **PASS** |

**Body vs root:** `body+0x0C` translation does **not** equal `ScnMdl+0x3C` / World[0]. The visual world placement is the ScnObj root at `+0x3C`, fed into CalcWorld hierarchy — not the physics body mtx.

**Root vs World[0]:** Same space; residual ≈5 (Y) typical → `World[0] ≈ Root · Local₀` (`relRW=WorldEqRootTimesLocal`), not byte-identical.

---

## D. Matrix composition

Guest multiply convention: GC / nw4r `PSMTXConcat` — `C = A · B` (3×4 row-major).

### Verified equation

\[
\mathrm{ViewPos}[i] = \hat{V} \cdot \mathrm{World}[i]
\]

where \(\hat{V} = \mathrm{ViewPos}[0] \cdot \mathrm{World}[0]^{-1}\) (same \(\hat{V}\) for all sampled nodes).

| Metric (TR1, `809882e0`) | Value |
|--------------------------|-------|
| Nodes tested | 8/8 |
| `vwEst` max residual | ≈0.001–0.004 |
| Tolerance | 0.05 |
| A1b/XF pos0 as \(V\) | **REJECTED** (`vwRes` ~1e4) — not CalcView camera |
| Status | **PASS** |

A1b “viewMtx” (first XF pos-matrix 0) is **not** the CalcView camera matrix; do not use it as \(V\).

### Chain (summary)

```
ScnMdl+0x3C (root)
  → CalcWorld / WorldHier → World[i]   (world space)
  → ViewA (task5) → ViewPos[i] = V_cam · World[i]
  → indexed XF / GX draw
```

`M = V·W` with A1b \(V\) remains **rejected**. With CalcView-consistent \(\hat{V}\), **PASS**.

---

## E. Temporal consistency

| Check | Result |
|-------|--------|
| Primary ScnMdl `809882e0` across TR1 XFBs | **PASS** (A3j stable + A3k dumps) |
| ViewPos/ViewEnv/ViewNrm bases | Unchanged from A3j |
| `b101` double-buffer toggle | Observed 0/1 |
| `ViewPos = V̂·World` across frames | **PASS** (residuals ~1e-3) |
| Model replace mid-capture | Not observed for primary |

---

## F. Performance and regression

| Check | Result |
|-------|--------|
| Flags-off RTX 3080 | ≈60 Hz (`a3k_flags_off.log`) |
| A3k + A1b + A3j owner + A3a | ≈60 Hz in-stage (`a3k_tr1_capture2.log`) |
| Peak RSS (instr) | ≈673 MiB |
| `regression_smoke` | **PASS** |
| A2e-multitex exact | **MATCH** (`a3k_a2c_retry.log`) |
| A2f-efbfb exact | **MATCH** |
| Linux build | Links |
| MinGW build | Links (`out-windows/SonicColorsRecomp.exe`) |
| Native Windows GPU | **UNVERIFIED** |

---

## Acceptance vs brief

| Criterion | Status |
|-----------|--------|
| 1. Ownership lifecycle of `809882e0` | **PASS** (call-time / scene) |
| 2. `CModelImpl+0x1C` proven or disproven for player | **PASS** (factory yes; player path no) |
| 3. Persistent path or call-time sufficient | **PASS** (call-time) |
| 4. Body / model-root relationship | **PASS** (root=`+0x3C`; body≠root) |
| 5. World ↔ ResNode hierarchy | **PASS** (CalcWorld + A3h) |
| 6. World ↔ ViewPos | **PASS** (`ViewPos=V̂·World`) |
| 7. ViewEnv / ViewNrm roles | **PASS** (distinct from ViewPos) |
| 8. GX provenance | **PASS** (A3j + composition) |
| 9. Multi-state numerical validation | **PASS** (TR1 script; residual ~1e-3) |
| 10–12. A2e/A2f / smoke / ~60 Hz | **PASS** |

---

## Interpolation-readiness gate

| Pillar | Status | Notes |
|--------|--------|-------|
| 1. Ownership readiness | **PASS** | Call-time CalcWorld `this` + live shell/body |
| 2. Transform readiness | **PASS** | World + ViewPos spaces closed; root at `+0x3C` |
| 3. Temporal readiness | **PASS** | Stable ScnMdl + node indices across XFBs |
| 4. Capture readiness | **PARTIAL** | Bounded pose ring exists in A3k/A3j; production capture path not productized |

**Recommendation:** A separate **interpolation-readiness** milestone is now justified for ownership + transform + temporal pillars. Capture packaging (complete pose buffer without diag drain) remains the smallest next gap. **Do not start bone/matrix interpolation in A3k.**
