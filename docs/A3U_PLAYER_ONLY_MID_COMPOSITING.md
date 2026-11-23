# A3u — Player-Only Mid Compositing (Retained Environment)

**Date:** 2026-11-23  
**Starting commit:** `c57de28` (A3t)  
**GPU evidence:** NVIDIA GeForce RTX 3080 via DXVK (`tools/run_nvidia_clean.sh`)  
**Logs:** `out/logs/a3u_tr1_composite2.log`, `a3u_tr1_budgeted.log`, `a3u_tr1_ignorebudget.log`  
**Scope:** Prove scene decomposition into retained pre-player EFB colour+depth + Sonic-only mid replay; measure cost vs A3t full-scratch GPU (~14–20 ms).

Opt-in: `SCR_A3U_PLAYER_COMPOSITE=1` (implies mid uses bg+player segment).  
Audit: `SCR_A3U_AUDIT=1`. **Off by default.** Does not advance sim/audio. Environment/camera remain guest-sampled (sample-held for mid).

---

## A. Feasibility findings

### Ownership / attribution

| Item | Evidence |
|------|----------|
| Live player model | PoseCapture `scn=809882e0` (run-local); ViewPos `8098afe0` |
| Player XF rows | ViewPos-range `LoadIndexedXF` → matrix index `xfAddr/12` (0..7 typical → mask `0xff`) |
| Non-player overwrite | Non-ViewPos XF into same matrix slot **clears** the bit |
| Player draw count | **~1400–1506** DrawIndexed-class draws / XFB (TR1 dense) |
| Draws before player | ~1–634 |
| Draws after player | **~22k–25k** (`contig=0`) |
| Contiguous to end? | **No** — large post-player scene/effects/UI tail |

Initial bug (`xfAddr/4`) marked almost every post-ViewPos draw as player; fixed to matrix index + clear-on-overwrite.

### Draw ordering

Sonic is **not** the last geometry. Strategy **A** (snapshot EFB immediately before first ViewPos→XF load) is used:

1. Flush pending env batches.
2. `CopyResource` live EFB colour + depth → bg ring slot.
3. Record `fifoOff = A2b_CurrentCmdBytes()` and `GXState` at that point.
4. Guest continues with normal Sonic + post-player draws into the live EFB (unchanged normal image).

### Chosen decomposition

| Choice | Rationale |
|--------|-----------|
| **A — pre-player EFB snapshot** | Avoids a second full-scene render; bg has no Sonic pixels/depth |
| Not B (split normal pass) | Would change the production render path; not required for a mid PoC |
| Not overlay on XFB with Sonic | Forbidden (ghost) |

**Safe for mid correctness gate (player vs bg):** bg excludes old Sonic; depth restored; player pass depth-tests against retained env.  
**Not a full-frame reconstruct:** post-player draws (~22k+) remain sample-held on the normal guest image only — omitted from the extra mid composite (explicit limitation).

---

## B. Background strategy

| Item | Detail |
|------|--------|
| Source | Live EFB colour + depth at first ViewPos XF |
| Format | Same as host EFB (`CopyResource` of `s_efbColor` / `s_efbDepth`) |
| Ring | 4 slots (`Render_A3uCapturePrePlayer` / `BeginScratchFromBg`) |
| Camera | Sample-held: mid restores `gx` at capture; **no** A3b XF view substitution |
| Lifetime | Slot keyed by `xfbIndex`; mid looks up matching/newest valid slot |
| Extra full-scene? | **No** — capture is a GPU copy during the normal frame |
| Clear during mid | `ClearEFB` no-op while `A3u_PlayerOnlyMidActive` (protects restored bg) |

---

## C. Player-only replay

| Item | Detail |
|------|--------|
| Segment | Archive bytes `[fifoOff … end)`; `g_gx` restored from capture |
| Filter | Skip draws whose PNMTX rows are not in the current player row mask |
| Pose | A3q path: `PoseInterp_OverrideIndexed` → `A3u_NotePlayerXfRow` |
| Fail closed | `dix==0` or `gxConsumed==0` → no Present (avoids bg-only / missing Sonic) |
| Cache identity | Bg slot: `xfbIndex` + fifoOff + scn/view; mask rebuilt each mid |

Typical accepted mid (budgeted TR1): **dix ≈ 40–72**, **gx ≈ 110–181**, flush prep ≪ full scene.

---

## D. Composition correctness

| Check | Result |
|-------|--------|
| Normal guest image when disabled | Unchanged |
| Normal guest image when enabled | Still full live EFB→XFB (Sonic + post-player) |
| Old-pose ghost in mid | Absent from bg; mid does not seed from XFB-with-Sonic |
| Depth | Restored pre-player depth; depth test left enabled |
| Post-player / transparent tail | **Omitted from mid** (sample-held on normal only) |
| Shadows / effects | Only ViewPos-row-owned draws; others sample-held |
| Camera | Sample-held bg camera + ViewPos' from A3q |
| Empty player pass | Rejected |

Pixel-exact normal vs “bg + guest-pose Sonic” reconstruct was **not** automated this milestone; normal path is untouched, so normal equivalence is by construction when the experiment is off, and when on the live XFB remains the full guest render.

---

## E. Performance (RTX 3080 / DXVK)

### vs A3t full-scratch GPU (~14–20 ms)

| Metric | A3t full scratch | A3u player-only (accepted) |
|--------|-----------------:|---------------------------:|
| Mid GPU (timestamps) | **~14–20 ms** | **~1.3–1.9 ms** typical |
| Mid CPU wall | ~17–22 ms | **~4.5–5.5 ms** typical |
| Dix / mid | ~650–2300 | **~40–140** |
| Full-scene scratch for mid | Yes | **No** |

### Soft budget (`a3u_tr1_budgeted.log`)

| Metric | Typical |
|--------|--------:|
| `simulation_hz` | **~60** |
| `unique_mid` (poseOk+gx) | **~6–8 /s** |
| `mid_ms` (accepted) | ~4.5–5.5 |

### Ignore budget (`a3u_tr1_ignorebudget.log`)

| Metric | Typical |
|--------|--------:|
| `simulation_hz` | ~36 (mid steals pace) |
| `unique_mid` | **~15–20 /s** |
| Mid GPU | ~1.6 ms |

**Not claimed:** sustained 60+60, full-scene 2×, or 120 FPS from present counts.

---

## F. Limitations

- Post-player scene (~22k draws) **not** in the mid composite.
- Camera motion **sample-held** between guest updates.
- Player set = ViewPos XF matrix ownership only (attachments/shadows/effects may be excluded).
- Some mid attempts fail (`no bg/seg`, empty pass); fallback = normal guest XFB.
- Sustained 60 additional composites/s **not** achieved under soft budget (~6–8/s).
- Full-scene 2× rendering **still unachieved** (by design this milestone avoids it).
- Native Windows GPU **UNVERIFIED** (MinGW link OK).

---

## Controls

| Env | Default | Role |
|-----|---------|------|
| `SCR_A3U_PLAYER_COMPOSITE` | off | Enable bg capture + player-only mid |
| `SCR_A3U_AUDIT` | off | Ordering / cost logs |

Requires existing A3o/A3p/A3q stack (`SCR_DECOUPLED_PRESENT`, `SCR_POSE_INTERP`, prod A2b).

## Tests

- `scr_a3u_classify_test` — matrix-index mask accept/reject  
- `scr_pose_capture_test`  
- Linux + MinGW builds  
- TR1 via `tools/setup_a2g_test_nand.sh` + `tools/a2g_tr1_enter.txt` + `tools/run_nvidia_clean.sh`
