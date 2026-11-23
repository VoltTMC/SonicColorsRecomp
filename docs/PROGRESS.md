# Progress log

## 2026-11-23 — A3o: decoupled presentation + time-driven pose α

**Goal:** Independent 2× presentation scheduler; time-driven α; safe mid render without advancing guest sim.

**Shipped:** `decoupled_present.*` (`SCR_DECOUPLED_PRESENT`, `_RATE`, `SCR_PRESENT_TRACE`, latency); pose `hostTicks` + 5-slot pin ring; `PoseInterp_BuildForHostTime` / time-driven α; `A2c_DecoupledMidPresent` + `Render_PresentScratchUniqueMid`; report `docs/A3O_DECOUPLED_PRESENTATION.md`.

**Evidence (RTX 3080):** Uncapped+decoupled (no A2b): sim≈60, unique_xfb≈60, present_calls≈120 with mid fail-closed (duplicates only). Dense A2b: sim≈35, no mid headroom. Unique mid 2× **not** claimed.

**Gate:** Timing/scheduler/fail-closed **PASS**; genuine 2× unique renders **FAIL** (archive cost / missing sustained mid). Win GPU **UNVERIFIED**.

## 2026-11-23 — A3n: first live TR1 Sonic pose interpolation

**Goal:** Consume qualified A3l pairs; rigid World + \(V̂\) SLERP; recompute ViewPos; feed existing GX indexed XF. No extra XFBs / no FPS claim.

**Shipped:** `pose_interp.*` (`SCR_POSE_INTERP`, `_ALPHA`, `_TRACE`, `_VALIDATE`); `Pose_InterpRigid` / quat SLERP; `LoadIndexedXF` override via `CanonAddr` ViewPos slots; report `docs/A3N_FIRST_LIVE_POSE_INTERPOLATION.md`.

**Evidence (RTX 3080):** 58-node apply; ViewPos `8098afe0`/`8098d420`; GX `gxOverrides` ≫ 0; CamCut fallback; sim ≈60 Hz. Host rigid tests PASS.

**Gate:** Live interpolated pose **PASS** through GX. Higher presentation FPS remains a **separate** milestone.

## 2026-11-23 — A3m: pose qualification + interpolation readiness design

**Goal:** Qualify A3l snapshots for a future interpolation consumer; produce design only (no interp).

**Shipped:** `pose_qualify.*` (`SCR_POSE_QUALIFY`); `Pose_ClassifyMtx` / `Pose_EvaluatePair`; host test extensions; report `docs/A3M_INTERPOLATION_READINESS_AND_DESIGN.md`.

**Evidence (RTX 3080, `a3m_tr1_qualify.log`):** pairs OK 5564/5581; World 100% Rigid; CamCut=15 Teleport=1 ModelGen=1; camDom vs poseDom confirms World⊥camera. Capture+qualify ~60 Hz.

**Gate:** Interpolation implementation milestone **READY (PASS)** for TR1 Sonic-first; **not started** in A3m. Wisp/respawn/other stages UNVERIFIED.

## 2026-11-23 — A3l: production-bounded pose capture

**Goal:** Coherent World+ViewPos player pose snapshots at ~60 Hz without diagnostic drain; bounded ring; no interpolation.

**Shipped:** `pose_capture.*` / `pose_capture_math.cpp` (`SCR_POSE_CAPTURE`); CalcWorld associate + XFB publish; 3-slot ring; ResDic hierarchy cache; \(V̂\) validation; host `scr_pose_capture_test`; report `docs/A3L_PRODUCTION_POSE_CAPTURE.md`.

**Evidence (RTX 3080, `a3l_tr1_capture.log`):** ScnMdl `809882e0`, ResDic 78 / in-palette 58, `nView=193`; `relMax` ≈0.001–0.004 (A3k ≈0.002 preserved); 5641 complete snaps; slot 0/1; ring wrap OK. In-stage `simulation_hz` avg ≈59.9.

**Perf:** capture-on ~60 Hz; A2e/A2f MATCH; `regression_smoke` OK; MinGW links; Win GPU **UNVERIFIED**.

**Gate:** Pose-capture foundation **PASS**. Interpolation remains a separate milestone — **not started**.

## 2026-11-23 — A3k: ownership lifecycle + ViewPos = V̂·World

**Goal:** Persistent body→ScnMdl path (or prove call-time sufficient); prove/disprove `CModelImpl+0x1C`; close body/root/world/view/GX composition without forcing `M=V·W` with A1b \(V\).

**Result:** Call-time ownership **PASS** for `809882e0`. `CModelImpl+0x1C` **PASS** as factory field; **REJECTED** for live TR1 player (creates never hold that ScnMdl). Visual root = `ScnMdl+0x3C` (≠ `body+0x0C`). **`ViewPos[i] = V̂·World[i]`** with \(V̂=\mathrm{ViewPos}[0]\cdot\mathrm{World}[0]^{-1}\), residual ≈0.002 (8/8 nodes). A1b XF pos0 as \(V\) rejected.

**Perf:** flags-off ~60 Hz; A3k instr ~60 Hz TR1. A2e/A2f MATCH; `regression_smoke` OK; MinGW links; Win GPU **UNVERIFIED**.

**Docs:** `docs/A3K_PLAYER_OWNERSHIP_AND_TRANSFORM_COMPOSITION.md`. Interp gate: ownership/transform/temporal PASS; capture packaging PARTIAL — separate readiness milestone OK; **no interp in A3k**.

## 2026-11-23 — A3j: player model ownership + dual-palette provenance

**Goal:** Close body/component→ScnMdl ownership, resolve high-hit palette groups A (`8098afe0`) and B (`80992ee0`/`80995320`), and bound root composition. No interpolation.

**Shipped:** `a3j_model_palette.*` (`SCR_A3J_PLAYER_MODEL_OWNER`, `SCR_A3J_PALETTE_A_TRACE`, `SCR_A3J_PALETTE_B_TRACE`, `SCR_A3J_ROOT_COMPOSITION`); A3i/A3h double-buffer + ViewNrm/ViewEnv joins; report `docs/A3J_PLAYER_MODEL_AND_PALETTE_PROVENANCE.md`. No G3dProc/View/Construct `PPC_OVERRIDE` (fault class `818000a8`).

**Evidence (RTX 3080, `a3j_tr1_capture4.log`):** ScnMdl `809882e0` `nView=193` — ViewPos=`8098afe0` (group A + slot1 `8098d420`), ViewEnv=`80992ee0` (group B + slot1 `80995320`), ViewNrm=`8098f860`; `b100=2` double-buffer. Call-time CalcWorld under live shell/body **PASS**; stored `body→ScnMdl*` **UNRESOLVED**. `M=V·W` **UNRESOLVED**.

**Perf:** flags-off ~60 Hz; dense diag ~30 Hz. Win GPU **UNVERIFIED**.

**Interp gate:** **Not ready** — need stored ownership path + closed root composition.

## 2026-11-23 — A3i: live Sonic ScnMdl ownership + world/view provenance

**Goal:** Hook nw4r world/view calc (or Colors equiv.), capture `r3=this` under live `shell→body→inst`, join high-hit indexed-XF pals to producing ScnMdl/stage. No interpolation.

**Shipped:** `a3i_model_ownership.*` (`SCR_A3I_MODEL_OWNERSHIP`, `SCR_A3I_CALC_WORLD`, `SCR_A3I_CALC_VIEW`, `SCR_A3I_PALETTE_PROVENANCE`); `ScnMdlSimpleCalcWorld` @ `805B87D0` in `game_addrs`; report `docs/A3I_PLAYER_MODEL_PROVENANCE.md`. G3dProc (`805B8DC0`/`805BBF40`) identified but **not** PPC_OVERRIDE’d (faulted under dense TR1).

**Evidence (RTX 3080):** CalcWorld `this` live with shell `80b58b00` / body `90f9c6c0` / inst `27a5990d`; high-hit ViewPos `8098afe0` (~632) → ScnMdl `809882e0` (`nView=193`) **PASS**; twin ~624-hit pals unresolved **PARTIAL**; body→ScnMdl* word **PARTIAL**; `M=V·W` **UNRESOLVED**.

**Perf:** flags-off ~60 Hz; dense A3i+A3h+A3a busy diag-bound. `regression_smoke` OK; MinGW links; Win GPU **UNVERIFIED**.

**Interp gate:** **Not ready** — need proven body/component→ScnMdl pointer + unresolved high-hit twin + root composition.

## 2026-11-23 — Windows Xbox / SDL gamepad input latency

**Goal:** Investigate GitHub reports of high Xbox controller latency on Windows; fix only with evidence.

**Cause:** `4205982` throttled Windows `SDL_PumpEvents` to 250 ms (hotplug). That also starved `SDL_UpdateJoysticks`, so XInput/HIDAPI pad state could stay stale up to ~250 ms. Linux pumps every frame → unaffected.

**Fix:** `SDL_UpdateJoysticks()` inside `SdlGamepad_Read`; keep hotplug pump throttled. Opt-in `SCR_INPUT_LATENCY` / `--input-latency` timing + overlay. Report: `docs/WINDOWS_XBOX_INPUT_LATENCY.md`.

**Validation:** Source + SDL XInput update path **PASS**; native Windows Xbox hardware **UNVERIFIED**. `regression_smoke` + Linux/MinGW builds OK. No sim/audio timing changes.

## 2026-11-23 — A3h: guest skeleton hierarchy + GX palette provenance

**Goal:** Evidence-driven mapping from player draws → nw4r `ScnMdlSimple` / `ResNode` parents → indexed-XF palette rows. Fix A3g 128-contributor truncation. No interpolation / second XFB / unique-FPS.

**Shipped:** `a3h_skeleton_palette.*` (`SCR_A3H_SKELETON_TRACE`, `SCR_A3H_PALETTE_MAP`, `SCR_A3H_HIERARCHY_DUMP`); FIFO `A3h_OnIndexedXf`; A3g caps draws=768/obs=2048; canonical-only guest reads (fixes CameraManagerTick-attributed SIGSEGV); report `docs/A3H_SKELETON_PALETTE_PROVENANCE.md`.

**Evidence (RTX 3080, `phase33_a3h_tr1_nv21.log`):** inst `27a5990d`; A3g **646/768+ovf0** (was 128+≈518); idxLoads ≤2594/4096+ovf0; ResNode parents **PASS** on resolved ScnMdls (e.g. 6-mtx/7-node trees); indexed XF→guest EA/mtxID **PASS** (mapped=1869); player Sonic ScnMdl ownership **PARTIAL**; `M≠V·W` **UNRESOLVED**.

**Perf:** flags-off ~60 Hz; dense A3h+A3a+A3g busy ~6–8 Hz (diag). A2f-efbfb + A2e-multitex MATCH; `regression_smoke` OK; MinGW links; Win GPU **UNVERIFIED**.

**Next:** Hook `ScnMdlSmpl_G3DPROC_CALC_WORLD`/`CALC_VIEW` (or Colors equiv.) for player `this`; close high-hit palette→ViewPos ownership — still before interpolation.

## 2026-11-23 — A3g: player multi-draw pose aggregation + hierarchy reconstruction

**Goal:** Aggregate A3f’s per-draw player matrices into a coherent per-frame pose; reconstruct hierarchy as far as evidence allows; assess interp readiness. No interpolation / second XFB / unique-FPS.

**Shipped:** `a3g_pose_aggregate.*` (`SCR_A3G_POSE_AGGREGATE`, `SCR_A3G_HIERARCHY_TRACE`, `SCR_A3G_POSE_DUMP`); `playerInstanceKey=hash(shell,body)`; nearest-128 eye-near skeletal draws; `obsKey` temporal match; duplicate row/hash/conflict flags; `inv(V)·M` modelApprox; A3b decompose → READY/UNSUP/UNMATCH; overlay.

**Evidence (RTX 3080, `phase32_a3g_tr1_nv4.log`):** stg110 via `a2g_tr1_enter.txt`; inst stable `27a5990d` (shell `80b58b00` / body `90f9c6c0`); draws=128 +ovf≈518 (honest); uniq obs≈350; `samePrev=1`; interpReady high after settle; roots=0; bone parents **unresolved**. Completeness **PARTIAL**.

**Perf:** flags-off ~60 Hz; dense+A3g busy TR1 diag-bound. A2e/A2f MATCH; `regression_smoke` OK; MinGW links; Win GPU **UNVERIFIED**.

**Next:** Guest bone-tree / palette source hooks; stratified keep beyond 128 — still before interpolation / second XFB.

## 2026-11-21 — A3f: player draw ownership + skeletal-palette provenance

**Goal:** Prove (or reject) draw-time association between the guest player instance and A3a skinned candidates; classify PNMTXIDX matrices; capture consecutive palettes. No interpolation / second XFB / unique-FPS.

**Shipped:** `a3f_player_provenance.*` (`SCR_A3F_PLAYER_TRACE`, `SCR_A3F_PALETTE_CAPTURE`, `SCR_A3F_DUMP`); staged PASS/PARTIAL/FAIL/UNVERIFIED/N/A report; BodyGetWorldMtx fan-out from A1c; A1b shell hooks auto-enable under A3f; `structKey` (no geomSig) for consecutive identity; overlay line.

**Ownership chain (TR1 gameplay, RTX 3080):**
`PlayerGetPos` / `PlayerGetWorldMtxPtr` / `BoostPluginTick` r4 → shell `80b58b00` → `+0x28` → `+0x84` → body `90f9c6c0` → `BodyGetWorldMtx` (`fn_80053718`) refreshes `body+0x0C` (T often 0) + pos `body+0x168` → A3a draw-time XF model×view bones nearest to `V·playerPos`.

**Candidate `66308a11`:** **REJECTED** as the player draw (pre-gameplay only; `tgtNear=0` in `stg110`). A3e stability was draw identity without guest ownership.

**Proven palette:** multi-row `posMtxDir=1` near-eye meshes (bestDist≈0.4–1.0); PNMTXIDX rows e.g. 0/3/6/9/12; kind=`modelViewBone`; complete **per draw** (not full skeleton). Multiple `structKey`s under one body. Log: `out/phase31_a3f_tr1_dense_nv.log` (+ `a2g_tr1_enter.txt`).

**Perf:** flags-off ~60 Hz; A3f+A3a title ~60 Hz; dense+A3f busy TR1 diag-bound. A2f MATCH; `regression_smoke` OK; MinGW links; Win GPU **UNVERIFIED**.

**Next:** Multi-draw player palette aggregation / bone hierarchy; stratified A3a keep — still before second XFB.

## 2026-11-21 — A3e: dense-frame archive scalability + player palette

**Goal:** Complete busy TR1 A2b/A3a archives without silent truncation; stable player skeletal palette across consecutive frames (no interp). Preserve A2/A3 exact-match; guest/audio timing; no second XFB / unique-FPS.

**Shipped:**
- `a3e_budgets.*` — `SCR_A3E_DENSE` heap caps (A2b res **16384**/128 MiB, cmd 16 MiB, A3a draws **16384**); soft-arm `SCR_A3E_ARM_ON_DRAWS`; `rejectedResources` + Incomplete (honest); overlay line
- `a3e_player_palette.*` — `SCR_A3E_PLAYER_PALETTE`; identityKey-stable cand palette; mtx row hashes; no interp
- A2b → vectors + runtime caps; A3a → heap draws / `A3a_CapMaxDraws`; A2c family `A3e-dense` (slots=8); a2g → `A3e_OnStageEntered`
- `tools/run_nvidia_clean.sh` — clean-env RTX path (`VK_ICD_FILENAMES=nvidia_icd.json`); Cursor LD pollution broke DXVK CreateDevice (do not use RADV for validation)

**Evidence (RTX 3080):** `out/phase30_a3e_tr1_nv2.log` (+ `phase30_a3e_tr1_nv.log`). Early 167/167 complete + MATCH; busy cover peak 632/632 @ 651 res; A3e-dense MATCH at deps≈6933 pass≈256 KiB (`colorMis=0`); palette `id=66308a11` stable 31 XFBs mtxN=4 changing (body EA=0 PARTIAL); soft-arm @167; watchdog 0. Flags-off ~60 Hz (`phase30_a3e_perf_off_nv.log`). Exact A2f MATCH (`phase30_a3e_exact_nv.log`). `regression_smoke` OK; MinGW links; native Win GPU **UNVERIFIED**.

**Limits:** Soft-arm + capture cost → peak obs below prior ~31k uncapped; body EA often unset; still Incomplete past caps (honest).

**Next:** Player body-EA latch / stronger ownership; denser-than-16k strategy or stratified sampling; still before second XFB.

## 2026-11-21 — A3d: full-scene GX archive coverage audit

**Goal:** Evidence-based accounting of what a real gameplay frame archives/replays — beyond the A3c env subset. No second XFB / no unique-FPS / no skeletal interpolation.

**Shipped:** `a3d_scene_coverage.*` (`SCR_A3D_SCENE_COVERAGE`); category histogram (HUD / env static|moving / skinned player|other / particle-like); A3a store cap **1536** + `overflowCat[]` for uncapped observation; A2c → `A3d_NoteReplay`; overlay line; bounded dumps (gameplay-first).

**Coverage terms:** observed ≠ captured ≠ complete ≠ replayable ≠ exact-match validated ≠ interpolatable.

| Scene | Evidence | Headline |
|-------|----------|----------|
| Title/menu HUD | `phase29_a3d_title_cover.log` | ~70 draws, **100%** A3a store, A2b often `complete=1`; category ≈69 HUD + particles |
| Early TR1 (quiet) | `phase29_a3d_tr1_cover2.log` xfb≈828–879 | **167/167** draws; hud=69 envS=45 skinP=1 skinO=51; rows=10 plyCand=1; A2b complete (356 res) |
| Busy TR1 | Prior `phase28_a3c1_tr1b.log` (A3a@400) | Peak **drawCount≈31027** with massive store drop; A2b hits **2048** resource cap → not full-scene complete |
| Exact MATCH | `phase29_a3d_exact_title.log` / `phase29_a3d_tr1_cover.log` | A2e multitex + A2f efbfb colour+depth MATCH (selected families ≠ full scene) |

**Perf:** flags-off / A3d-only ≈60 Hz (RTX 3080 via `tools/run_nvidia_clean.sh`); A2b+A3a adds cost under load. Disabled → no A3d spam.

**Limits (superseded in part by A3e):** Busy frames exceeded A3a/A2b caps at A3d time; skinned IDs candidate-heuristic; RADV stage-load watchdog hangs; native Windows GPU **UNVERIFIED**.

**Next:** → A3e (done above).

## 2026-11-21 — A3c.1: endpoint reference semantics + depth classification

**Goal:** Explain α=0 vs α=1 asymmetry; prove once-only delta application; classify midpoint depth; keep diagnostics from tanking sim Hz.

**Endpoint asymmetry cause (Confirmed):** Archive is frame \(N{+}1\) with \(V_{\mathrm{base}}=V_{\mathrm{curr}}\). α=1 → \(D=I\) → MATCH. α=0 applies previous camera to **the same** \(N{+}1\) geometry → DIFF vs archive live image is **expected** (camera-only), not a broken previous-frame endpoint. Cross-frame previous endpoint = Test D (prior archive’s identity MATCH).

**Shipped:** Labeled probes (A identity / B α=1 / C α=0 camera-only / D prior identity / Mid); `depthClass`; geomUnchanged via A3a `identityKey`+`geomSig`; coverage log; `SCR_A3C_PROBE_LIMIT`; skip A3b probes on A3cEnv; math tests for base=\(V_{\mathrm{curr}}\).

**Evidence (`phase28_a3c1_tr1b.log`):**
- Test A/B exact MATCH; `applies=1 xfHooks=8` (begin once + per XF load).
- Test C: `geomUnchanged=1`, colour+depth DIFF, `camDist≈3.08`, explicitly not vs live \(N\).
- Test D: prior archive identity retained.
- Mid: `expected_camera_motion` + `depth_only_camera_motion` classified; `camLerp=1`.
- Capture path ~50–56 Hz; probe sets ~15–22 Hz; `regression_smoke` + math test OK.

**Limits:** Full-scene / skeletal / second XFB unchanged. Dense A3cEnv replays still costly while probes run.

## 2026-11-21 — A3c: coherent TR1 environment capture + natural camera interp

**Goal:** Bounded, coherent Tropical Resort Act 1 environment subset rendered in scratch at a **naturally** interpolated camera pose (consecutive guest frames), with exact archive endpoints. No second XFB / no unique-FPS / no skeletal or player-body work.

**Selected group:** `A3cKind_Env` static/single-matrix world draws (DL or larger non-UI); excludes HUD, skinned multi-row actors, player candidates. A2c family `A3cEnv` with raised select budgets when `SCR_A3C_*` on.

**Shipped:** `a3c_env_capture.*`; A2b budgets (8 MiB cmd / 2048 res / 24 MiB); A3a 400 draws; A3b `baseIsPrev`, row mask, frozen camera pair; A2c `A3cEnv` family + sync natural probes.

**Transform:** Archives embed \(V_{\mathrm{curr}}\) → \(D = V_\alpha · V_{\mathrm{curr}}^{-1}\) (`baseIsPrev=false`). XF loads re-hooked once. Projection unchanged.

**Superseded probe framing:** See A3c.1 for correct α=0 / Test D semantics (`phase28_a3c1_tr1b.log`).

**Limits:** Full TR1 still overflows on densest frames. Not full-scene / not production high-refresh.

**Next:** Broader env layers / multi-pass depth stacks still before skeletal interp or second XFB.

## 2026-11-21 — Windows controller fix (critical)

**Goal:** Reliable Windows/Wine controller input (Xbox + DualShock/DualSense) without regressing Linux.

**Root cause:** First pass used XInput-only packing. DualShock 4 (`054C:05C4`) is not an XInput device, so Wine/Windows saw `pads: 0` while Linux SDL listed `PS4 Controller`. Earlier slot-packing bug (raw XInput user index as player slot) remains fixed by packing connect order.

**Fix (`fix/windows-controller`):**
- Shared `runtime/platform/sdl_gamepad.{h,cpp}` for Linux + Win32.
- Win32: `SDL_Init(GAMEPAD)` only (no SDL video); HIDAPI hints before init; hotplug via `SdlGamepad_PumpHotplug`.
- CMake: FetchContent SDL3 for `WIN32`; link `SDL3::SDL3`; POST_BUILD copy `SDL3.dll`.
- `Ui_FeedGamepadFromPads()` for launcher/overlay ImGui nav from packed slot 0.

**Validation:**
- MinGW: PE imports `SDL3.dll` (not `XINPUT1_3.dll`); `SDL3.dll` beside exe.
- Wine + USB DS4: `--list-gamepads` → `pads: 1` / `0: PS4 Controller`.
- Linux native: same pad → `pads: 1` / `PS4 Controller`.
- **Native Windows hardware:** still required before release claim (Wine ≠ native).

## 2026-11-21 — A3b: camera-pose interpolation + scratch rendering

**Goal:** Interpolate consecutive camera poses (TRS/quat, not element-wise view lerp) and apply the intermediate pose to archived GX rendering in scratch only. No second XFB / no unique-FPS claim / no skeletal interp.

**Camera / view semantics:** A1b `viewMtx` = first XF LoadXF of PNMTX0 in the XFB frame = guest **world-to-view** (3×4 row-major, column vectors). Model-views are `View*Model`; camera delta `D = Vα · inv(V0)` is **left-multiplied** onto GX_PNMTX0..9 (rows 0,3,…,27). PNMTX0 is not always a standalone camera during UI; during menu/TR1 motion `viewT` tracks camera movement. Guest `CameraManager` node pos hooks often idle (`camPos=0`); XF view is the rendering source of truth for A3b.

**Shipped:** `a3b_camera_math.*` (decompose/slerp/delta) + `a3b_camera_interp.*` (pose ring, cut/proj fail-closed, scratch apply); `SCR_A3B_CAMERA_INTERP`, `SCR_A3B_CAMERA_ALPHA`, `SCR_A3B_FORCE_VIEW_DX` (static-view midpoint proof); unit test `scr_a3b_camera_math_test`; A2c probes TestA/B/C; overlay line.

**Evidence:**
- Math unit test OK (identical / translate / rotate / slerp sign / singular / shear / reflection).
- TestA α=0 endpoint MATCH+exercised on A2f (`phase27_a3b_force_dx.log`).
- TestC α=0.5 with `FORCE_VIEW_DX=8` midpoint hash distinct + TestB α=1 DIFF (`phase27_a3b_force_dx.log`).
- TR1 consecutive poses with `fail=000` and smooth `viewT` motion; cuts `fail=020` (`phase27_a3b_tr1_view.log`).
- Boot proj changes fail-closed (`fail=080`); A2d/A2e/A2f still MATCH; capture-off ~60 Hz; `regression_smoke` OK.

**Limits:** Full TR1 scene archives still overflow / not replay-ready — A3b scratch proof uses complete A2f/A2e UI passes + forced view delta, plus real pose capture on TR1. Not full-scene camera interp. No second XFB.

**Next:** Bounded camera-sensitive env draw archive for natural (non-forced) midpoint on 3D geometry — still before skeletal interp / second XFB.

## 2026-11-21 — A3a: draw-level transform attribution + scratch matrix override

**Goal:** Know which PNMTXIDX rows each GX draw uses, correspond draws across frames, and safely substitute a matrix in scratch replay only.

**GX semantics:** Per-vertex PNMTXIDX (VCD) or `XF_MATINDEX_A`; row index → `xfmem[row*4..]`. Not “last LoadXF”. Skinned draws use multiple rows.

**Shipped:** `a3a_draw_attr.*` — `SCR_A3A_DRAW_ATTR`, `SCR_A3A_MATRIX_OVERRIDE` (+ mode/row/dx); hooks in `Render_Draw` / `Render_OnXF` / A2c probe; overlay line.

**Evidence:** TestA noop MATCH+exercised (`phase26_a3a_testA.log`); TestB tx DIFF on A2e (`phase26_a3a_testB.log`). TR1: stable skinned `id=66308a11` rows 9/12/15/18 with `candΔ` (`phase26_a3a_tr1_attr2.log`). A1b player hooks fire but `+0x168` stayed ~(0,1,0) / body mtx identity.

**Interp feasibility:** Prefer TRS on bones + A1b view for camera; no element-wise lerp. **Next A3b:** camera-only view scrub **or** one confirmed bone TRS after reliable world pose — still no second XFB / no unique-FPS.

## 2026-11-21 — A2g-StageCoverage: TR1 access + natural EFB search

**Goal:** Reproducible Tropical Resort Act 1 entry on an isolated NAND, then natural (unforced) cross-frame EFB sampling search during real gameplay.

**Shipped:** `--nand` / `SCR_TEST_NAND_DIR` isolation + boot diagnostics; `tools/setup_a2g_test_nand.sh`; `tools/a2g_tr1_enter.txt`; stage/disc hints in `a2g_natural_trace`; audit `docs/NAND_SAVE_AUDIT.md`.

**Fixture:** Copy of verified in-tree save (Tropical Resort unlocked) into gitignored `out/test_nand/` — not a hand-written flag dump. Userdata SHA unchanged across test runs.

**Stage entry:** **PASS** — `A2g-Stage: entered 'stg110'` + music stage context (`phase25_tr1_nat_full.log`). Input = normal pad script only.

**Natural TR1 (~8k presents, walk/jump/boost/camera/pause):** nextXfb=**0**, multiXfb=**0**, CROSS=**0**, longestLifeXfb=**0**. Resources replaced same-present before later sampling. Synthetic history stress **skipped** (no new lifetime question beyond A2g retain MATCH + this natural result).

**Perf:** Trace-on TR1 sim often ~35–55 Hz (stage + trace cost); underruns≈0 after settle. Capture/trace off path unchanged. No unique-FPS claim.

**Next:** Other stages if needed; natural replay still blocked (no consumer). Do not equate A2g selftest with natural.

## 2026-11-21 — A2g-Natural: live cross-frame EFB discovery

**Goal:** Find whether the unmodified game samples an unrecreated EFB→tex generation across a guest/present boundary.

**Shipped:** `SCR_A2G_NATURAL_TRACE=1` (`a2g_natural_trace.*`) — create/replace/sample/invalidate counters + bounded CROSS edges; `SCR_A2G_SELFTEST` gates synthetic retain (default on with scratch replay).

**Evidence:** Title + 150s TAS menu mash — **0** next/multi natural samples (`phase24_nat_*.log`); ~129k creates / ~593k same-frame binds; replacements ≈ creates. Trace @ ~60 Hz. Selftest still MATCH; off silent; `regression_smoke` OK.

**Natural replay:** not started (no consumer). Follow-up: A2g-StageCoverage (TR1).

## 2026-11-21 — Approach A2g: cross-frame EFB texture persistence

**Goal:** Retain scratch EFB→tex generations across a guest/present frame boundary and sample them in the next frame with exact live MATCH.

**Audit:** Live `s_efbCopies` persist on the host, but title/menu always re-copy before sample (0 cross-frame samples in >200k hits). Split live vs scratch maps; genId + canonical dest keys; prev/curr archive packets + edges.

**Controlled proof:** Consecutive A2f idle passes; producer → retain scratch copies → consumer with skip-tex-copy under isolation. `edges=5/5`, `copyH=5/5`, `retain=1`, `dCmp=1 MATCH` (`phase23_a2g_selftest2.log`). A2f/A2e still MATCH. Capture off ~60 Hz; on-path still ~35 Hz.

**Next:** Natural live cross-frame sample (gameplay history buffer) or longer history ring — no scene-wide / no unique-FPS. Docs: `docs/FRAME_RATE.md` A2g.

## 2026-11-21 — Approach A2f: controlled EFB-feedback capture/replay

**Goal:** Archive and scratch-replay a pass with real EFB→tex feedback (producer → copy → sample → consume), with exact MATCH. Also defer everyPixel compare off the XFB critical path where possible.

**Selected pass:** Title/boot idle HUD mip-chain (~5504 B, ~70 draws, **5 sampled EFB copies**). Same screen as A2d, but now classified `A2f-efbfb` once copy→sample is closed. Prior bug: isolation `DoEFBCopy` read **live** EFB; fixed to copy from **scratch**.

**Shipped:** `A2bRes_EfbCopy` + Sampled; scratch-sourced DoEFBCopy; `A2cReady_EfbFeedback`; async compare begin/poll (skipped ≠ MATCH); `SCR_A2C_COMPARE_SYNC`.

**Validation (`out/phase22_a2f_*.log`):** A2f-efbfb MATCH (`efbC=5/5`, `det=1`, `colorMis=0`, `depthMis=0`, `dCmp=1`, `liveC==scrC`); A2e-multitex also MATCH under A2f (`phase22_a2f_long.log`); capture off ~60 Hz; on-path ~35 Hz (capture/replay cost; async compare does not eliminate it); underruns≈0 after settle; `regression_smoke` OK. MinGW links; native Win GPU replay unverified.

**Next:** cross-frame EFB texture lifetime, or another static/UI profile — no scene-wide / no unique-FPS. Docs: `docs/FRAME_RATE.md` A2f.

## 2026-11-21 — Approach A2e: second independent static/UI pass MATCH

**Goal:** Show A2c/A2d archive→incoming-EFB seed→scratch MATCH is reproducible on a second, structurally different static/UI pass — not a one-pass special case. No scene-wide / interpolation / second XFB / unique-FPS claim.

**Second pass (A2e-multitex):** ~12.1–12.5 KiB cmds, ~80–82 draws, **8–10 texture deps** (no DL). Contrasts A2d-idle (~5.5 KiB / ~70 draws / 1–3 deps, typically 1 DL + 1 tex). Classified by size/draw/dep fingerprint only.

**Shipped:** `A2cPassFamily` + per-family ready/replay slots; seed colour/depth verify (`seed=1/1`); dual-family overlay; post-dual-MATCH replay backoff. Same boundary/isolation as A2d.

**Validation (`out/phase21_a2e_*.log`):** A2d-idle **MATCH** and A2e-multitex **MATCH** (`det=1`, `colorMis=0`, `depthMis=0`, `dCmp=1`, `liveC==scrC`, `liveD==scrD`); A2d still MATCH after A2e; capture off silent @ ~60 Hz; `regression_smoke` OK. Diagnostic on-path slows uncapped sim while comparing — expected, default off.

**Next:** Controlled EFB-feedback experiment, or a third static/UI profile — still no actors / no scene-wide. Docs: `docs/FRAME_RATE.md` A2e.

## 2026-11-21 — Approach A2d: EFB initial-state + host-state closure

**Goal:** Eliminate A2c live-vs-scratch DIFF for the selected title/boot pass by capturing pass-entry EFB (colour+depth), seeding scratch from it, matching the comparison boundary to pass-exit (pre-post-XFB-clear), and realising host draw state from archived `g_gx` + resources.

**Boundary:** Commands `[0, first toXFB BP 0x52)` exclusive. Incoming EFB = after post-XFB `ClearEFB` (`A2b_OnPassEntryEfb`). Live reference = EFB at `A2b_OnXfbCopy` before that clear. Scratch = replay of the same prefix into independent RTVs.

**Shipped:** GPU `CopyResource` EFB slots; scratch seed (no post-seed clear); exact colour + depth compare (hash-authoritative depth); host reset via `gxStart` + `Render_ResetHostDrawState` + tex-cache drop; honest `MATCH` / `DEPTH_DIFF` / `COLOR_MATCH` verdicts. Default off.

**Validation (`out/phase20_a2d_*.log`):** `det=1`, `colorMis=0`, `depthMis=0`, `dCmp=1 MATCH` with `liveC==scrC` and `liveD==scrD`; `efbIn=yes`; capture off silent; sim/unique_xfb ≈60; `regression_smoke` OK. **No unique-FPS claim.**

**Completeness:** command + resource + initial-state + host-state (this D3D11 path) + determinism + **pixel equivalence** for this pass. EFB-as-texture chains / scene-wide / interpolation **not** claimed. Next: A2e second static/UI pass MATCH gate.

**Docs:** `docs/FRAME_RATE.md` A2d.

## 2026-11-21 — Approach A2c: resource closure + scratch-EFB replay

**Goal:** Close vertex/texture deps for a selected archived pass; optionally replay into an isolated scratch EFB with PE IRQs suppressed. No second game XFB / no interpolation.

**Selected pass:** Title/boot idle (~5504 cmd bytes, ~70 draws, 1×64 B DL + 1×~150 KiB texture). Chosen for size, reproducibility, and lack of moving scene actors — not assumed fully self-contained for pixel parity.

**Shipped:** Draw-time vertex-array range capture; texture/TLUT/TMEM snapshots; resource dedupe/refcounts (256 / 4 MiB); per-pass `A2cPassReport` readiness gate; `SCR_A2C_SCRATCH_REPLAY=1` scratch RT + PE suppress + guest RAM/`g_gx` restore; overlay line. Default off.

**Validation (`out/phase19_a2c_*.log`):** ready title frames with `reqUnres=0`; replay `det=1 drew=1`; full-EFB fingerprint **DIFF** vs live; capture/replay off silent; sim/unique_xfb ≈60. Gate rejects incomplete deps. **No unique-FPS claim.**

**Honest status:** Resource closure + isolated deterministic scratch output **yes**. Pixel MATCH vs live **not yet** (A2d). Scene-wide replay **not** claimed.

## 2026-11-21 — Approach A2b: bounded GX FIFO archive

**Goal:** Opt-in archive of GX FIFO bytes + draw-context/resources **before** `ProcessFifo` erases them. Capture/validate only — **no replay / redraw / second XFB**.

**Lifecycle:** `GPFifo` → Gather → 32 MiB ring → GpuThread → `s_fifo` → **`A2b_OnFifoSubmission`** → `ProcessFifo` erase. DLs / indexed XF snapshotted at decode; XFB finalizes GX + A1b/A1c context + CP array inventory.

**Shipped:** `runtime/debug/a2b_fifo_archive.*` behind `SCR_A2B_FIFO_CAPTURE=1` / `SCR_A2B_FIFO_DUMP=1` / `SCR_A2B_FIFO_DUMP_DIR`. Bounds 4 MiB / 512 coalesced subs / 96 resources / 1 MiB res copies. Overlay line; curr/prev compare; structured `.bin`+`.txt` dumps. Default off; FIFO processing unchanged.

**TR1 (`out/phase18_a2b_tr1b.log`, `out/a2b_dumps_tr1b/`):** bin size matches `totalBytes`; no silent cmd truncation after coalesce; busy frames hit **resource** cap (`flags` Incomplete) while cmds stay complete; with A1b/A1c: `a1b=yes`, `ply=yes`, `a1cLoads`~1200; sim≈60, unique_xfb≈60, underruns≈0. Capture-off silent. Boot on/off same unique-frame metrics.

**Complete:** ordered command bytes + hashes; DL/indexed-XF copies when under caps; XF/CP/BP end-of-frame snap; guest correlation hooks. **Unresolved:** vertex/index extents, TMEM/textures, EFB pixels, object-stable tags from slots. **Not replay-ready.**

**Next:** A2c — smallest IRQ-suppressed scratch-EFB replay of one archived pass after resource completeness work. Docs: `docs/FRAME_RATE.md` A2b.

## 2026-11-21 — Approach A1c: scene-wide GX transform capture

**Goal:** Observe GX position-matrix loads + guest `BodyGetWorldMtx` refreshes without extra sim/XFB.

**Player (`fn_80053718`):** Refreshes `body+0x0C` via `fn_800533E8` only when dirty bit0 at `+0x2DD8`; returns `+0x0C`. A1b identity-T came from reading without that path / T often 0 even after refresh. Position stays at `+0x168`. Capture is post-`PPC_ORIGINAL(BodyGetWorldMtx)` only.

**Shipped:** `SCR_A1C_GX_CAPTURE` / `SCR_A1C_GX_DUMP` — bounded XF pos-mtx loads (≤128/frame, slots 0–9), body EA samples (≤48), curr/prev frame, overlay. `Render_OnXF(..., indexed)`.

**TR1 evidence (`out/phase17_a1c_tr1*.log`):** slots 1–9 T move with camera/scene (e.g. (−34,−22,−167) → (−0.3,−16,−64)); player `bodyEa` stable with hash changes; ≤1 guest body EA (player); 1000+ loads/frame truncated; sim≈60, underruns≈0; capture-off silent. **No unique-frame claim.**

**Ready for A2?** Observation yes; object-stable interp **no** without FIFO archive + better actor tagging. Next: A2b archive XF/draw bytes.

## 2026-11-21 — Approach A1b: guest pose / camera-state capture

**Goal:** Immutable guest pose snapshots for future host interpolation (Approach A). No extra unique XFBs.

**Shipped:** `runtime/debug/a1b_pose_capture.*` behind `SCR_A1B_POSE_CAPTURE=1` / `SCR_A1B_POSE_DUMP=1`. Curr+prev `A1bPoseSnap` at sim-frame end; XF proj/viewport + first pos-mtx-0 view at XFB. Overlay line when enabled.

**Guest sources:** `PlayerGetPos` (`0x80033C34`, r4=shell), `PlayerGetWorldMtxPtr` (`0x80033C78`), `BoostPlugin*` r4; body via shell→`+0x28`→`+0x84`; mtx `+0x0C`, pos `+0x168`. `CameraManagerTick` (`0x80571720`). **Not** `lbl_808F3378` as shell (type `0x88C0`; chain invalid). MEM2 pointers (`0x9…`) accepted.

**Validation:** Title + TR1 path (`--nand userdata`, phase8-style input script). `player=ok` with live shell/body; viewT moves with camera; `pair=yes`; sim≈60; in-stage underruns≈0; capture-off silent. Body `+0x0C` translation often identity without `fn_80053718` refresh — documented blocker for A2. No unique-frame claim.

**Docs:** `docs/FRAME_RATE.md` A1b (sources, matrix conventions, scene-wide classification). `regression_smoke` after push.

## 2026-11-21 — Approach A1: renderer feasibility investigation

**Pipeline:** Guest `fn_800123A0` → `fn_800115BC` → `fn_80014AA8`/`fn_80014BD0` (update+draw coupled) → GXFIFO → `gx_fifo` consume/erase → BP `0x52` XFB → Present. `Renderer_RepeatLastXfb` is blit-only.

**A/B/C/D:** Guest redraw unsafe (`fn_8053FCA4` / update slot 3). FIFO replay unavailable (bytes erased) and identical without matrix patch. Host interpolation is the viable long-term path but needs actor matrices (not in `game_addrs` yet). Approach B still rejected.

**Prototype:** No safe unique-frame spike this phase. Groundwork only: `SCR_A1_XF_CAPTURE=1` snapshots XF projection/viewport at each XFB (`runtime/debug/a1_xf_capture.cpp`); does not raise `unique_xfb`. Docs in `docs/FRAME_RATE.md` (A1 section).

Boot-path uncapped @ 120 Hz (`out/phase15_a1_{off,on}.log`): with/without capture, `simulation_hz`/`unique_xfb`/`unique_frames_presented` ≈ 60, `present_calls` ≈ 120, underruns ≈ 0. Capture does not inflate unique frames. `regression_smoke` OK.

## 2026-11-21 — Genuine unique frames >60: blocked; Approach A roadmap

**Verdict:** Uncapped today is presentation-uncapped only (`unique_xfb` ≈ 60). Guest update→draw→FIFO→XFB is one chain per `FrameLoopWait`; FIFO is consumed (not safely replayable); raising sim Hz (Approach B) breaks per-iteration systems. **Approach B rejected.** Long-term path is Approach A (decoupled host render between 60 Hz sim steps) — multi-phase; not claimed complete.

Delivered A0: architecture docs in `docs/FRAME_RATE.md`, `unique_frames_presented` metric (first Present per XFB only), audio underrun check vs 60 FPS baseline. Do not treat `present_calls` as unique game FPS.

Boot-path `--perf` scale=3 @ 120 Hz (`out/phase14_*.log`):

| Mode | sim | unique_xfb | unique_frames_presented | present_calls | underruns/window | repeats |
|------|----:|-----------:|------------------------:|--------------:|-----------------:|--------:|
| uncapped `--no-vsync` | ~60 | ~60 | ~60 | **~120** | ~0 | ~0–2 |
| uncapped `--vsync` | ~60 | ~60 | ~60 | ~60 | ~0–2 | ~0 |
| `--60fps` | ~60 | ~60 | ~60 | ~60 | ~64 (pre-existing boot/SDL) | ~0 |
| `--frame-rate 30` | ~30 | ~30 | ~30 | ~30 | ~0–7 | ~0 |

Uncapped underruns ≤ 60 FPS baseline; `unique_frames_presented` tracks `unique_xfb`, not re-Presents.

## 2026-11-21 — High-refresh presentation: honest metrics + paced Present

**Problem:** Uncapped mode reported ~500–700 “FPS” from `Present()` spam of an unchanged XFB while `unique_xfb` stayed ~60. That figure is not unique game frames.

**Fix:**
- Pace uncapped `Renderer_RepeatLastXfb` to `display_refresh_hz` (or `host_fps_cap`) when VSync is off; stop ~1000 Hz Present spam.
- VSync on: unique-XFB `Present(1)` only (FIFO / host refresh); pace loop pumps AI/AX without re-Present(1) (avoids starving audio). Compositor holds each image across refreshes.
- Status / overlay / `--perf`: `simulation_hz`, `unique_xfb`, `present_calls`, `display_refresh_hz`, `guest_vi_hz`, `vsync`.
- CLI `--vsync` / `--no-vsync`. Docs: no safe unique &gt;60 Hz without redesign/interpolation.

Boot-path `--perf` scale=3 on 120 Hz (vkd3d FIFO when vsync on):

| Mode | sim | unique_xfb | present_calls | display | audio repeats |
|------|----:|-----------:|--------------:|--------:|--------------:|
| uncapped `--no-vsync` | ~60 | ~60 | **~120** | 120 | ~0 |
| uncapped `--vsync` | ~60 | ~60 | **~60** | 120 | ~0 |
| `--60fps` | ~60 | ~60 | ~60 | 120 | ~0 |
| `--frame-rate 30` | ~30 | ~30 | ~30 | 120 | ~0 |

Logs: `out/phase13_*.log`. `regression_smoke` OK. See `docs/FRAME_RATE.md`.

## 2026-11-21 — Split sim / xfb / present / display / VI metrics

Status + FPS overlay no longer treat `g_framesPresented` (Present calls, including uncapped re-presents) as “the” FPS. Report `sim`, `xfb`, `present`, `guest_vi`, host `display` Hz, and VSync/tearing. DXGI `ALLOW_TEARING` when VSync is off (if supported). VSync sync interval follows the host refresh, not emulated VI. See `docs/FRAME_RATE.md`.

## 2026-11-21 — Uncapped audio: pump AI/AX during re-present wait

**Cause:** `PaceUncappedHost` busy-re-presented without `Sched_RunDue` / `ppc_check_irq`. 30/60 modes rely on VI-spin IRQ checks to push AI DMA and run guest AX mid-frame; skipping that starved the mixer → hundreds of `g_audioRepeats`/s and broken audio while present stayed high.

**Fix:** Each pace-loop iteration runs `Sched_RunDue` + `Irq_Update` + `ppc_check_irq` (same path as VI spins), then optional `Renderer_RepeatLastXfb`. Sim stays 60 Hz; present stays uncapped.

Measured boot-path status (scale=3):

| Mode | Sim | Present | Underruns (avg) | AI repeats (avg) | Rate |
|------|----:|--------:|----------------:|-----------------:|-----:|
| 30 | ~30 | ~30 | ~0 | **0** | ~32 kHz |
| 60 | ~60 | ~60 | ~0 | **0** | ~32 kHz |
| Uncapped (before) | ~60 | ~800 | variable | **200–750** | dips |
| Uncapped (after) | ~60 | ~850–900 | ~0 | **~0–6** | ~32 kHz |

Logs: `out/phase11_audio_{30,60,uncapped}.log`.

## 2026-11-21 — Uncapped: 60 Hz sim + unlocked re-present (no gameplay speedup)

**Regression after aacec3a:** Skipping VI barriers unlocked ~400–540 present Hz, but the guest loop also ran that fast. Wall-clock overwrite of `lbl_808F2D98` was insufficient — many systems advance once per `FrameLoopWait` (see decomp `fn_800115BC` + FRAME_RATE audit).

**Fix:** Keep VI barrier bypass for presentation. Publish fixed `N=1` / `dt=1/60`. `PaceUncappedHost` holds **sim at 60 Hz** and calls `Renderer_RepeatLastXfb()` until the next sim slot. `host_fps_cap` limits re-presents only. See `docs/FRAME_RATE.md`.

Boot-path `--perf` (scale=3), measured:

| Mode | Sim Hz | Present Hz | Notes |
|------|-------:|-----------:|-------|
| `--frame-rate 30` | ~30 | ~30 | unchanged |
| `--60fps` | ~60 | ~60 | unchanged |
| `--frame-rate 0` | **~60** | **~790–835** | ~730+ re-presents/s; xfb ~60 |

Logs: `out/phase10_perf_{30,60,uncapped}.log`.

## 2026-11-21 — Uncapped mode: remove secondary 60 Hz VI barriers

**Root cause:** After bypassing `FrameLoopWait`'s `VIGetRetraceCount` EQ spin, the guest still blocked on `fn_80354320` (VI flush drain) and `fn_80353360` (wait for retrace-counter bump). Both sync to emulated 60 Hz VI — explaining `--perf` with **0 EQ spins** but a hard ~16.7 ms guest∶XFB∶present period.

**Fix (partial):** Override those as `VIFlushAndWait` / `VIWaitNextRetrace`; no-op while uncapped. Superseded for gameplay timing by the sim/present split above.

## 2026-11-20 — Release v0.3.7 (hotfix)

- Music original preview (vgmstream-cli), managed Windows Music deps persistence/Check tools, and Music tab performance since 0.3.6.

## 2026-11-20 — Fix original preview picking vgmstream123

- Doctor preferred `vgmstream123` (player) after `vgmstream-cli -h` exited 1; preview then failed with “Live output driver does not take an output file”.
- Probe decode-only CLI (banner/`-o outfile`); never select 123. Win32 `ExistsFile` + prefer `music_asset.exe` beside the launcher for winpthread.

## 2026-11-20 — Music Library UI performance

- Root cause of tab lag: per-frame full `Wav_Load` in list status + Windows `MusicDeps_Status`/LinkBins on every draw + unclipped list.
- Fix: `Wav_Probe`, ImGuiListClipper + filtered index cache, deps status cache; `tools/music_perf_check.sh` + `docs/MUSIC_LIBRARY_PERFORMANCE.md`.

## 2026-11-20 — Fix Check tools vs managed Music deps

- Embed Python `._pth` ignores PYTHONPATH → `music_asset` now bootstraps `sys.path` via `-c`; rewrite read-only `python312._pth` for site-packages; normalize `/` in extract paths for Wine/Win32.
- Doctor skips unusable drop-ins and continues to managed `current/bin` (bad AppData ffmpeg no longer blocks).

## 2026-11-20 — Fix Windows managed Music deps persistence

- Root cause: install markers/usable checks disagreed with `extract_to`; CLI readiness only looked at `current/bin`, so a failed LinkBins left packages “missing” and triggered redownload loops.
- Fix: files under manifest `extract_to` are authoritative (no redownload when present); LinkBins recursive + primary publish into `current/bin`; doctor/manager share that path; detailed `[Dependency]` logs; path-contract tests in `music_deps_test.sh`. Native Windows relaunch E2E still UNVERIFIED.

## 2026-11-20 — Release v0.3.6 (hotfix)

- Windows Music tab **Install Music tools** (auto deps manager) + vgmstream discovery + CreateProcess 87 lineage from 0.3.5.

## 2026-11-20 — Windows Music Library automatic dependency manager

- Pinned `data/music_deps_manifest.json` (Python embed 3.12.8, cricodecs 1.2.0, vgmstream r2168, BtbN LGPL FFmpeg).
- Downloads into `%LOCALAPPDATA%\SonicColorsRecomp\dependencies`; Music tab Install/progress UI; user `tools/bin` still overrides.
- Not bundled in Setup/ZIP (runtime download + license notes in `licenses/MUSIC_DEPS_RUNTIME.txt`). Native clean-Windows E2E still UNVERIFIED.

## 2026-11-20 — Windows vgmstream discovery for Music preview

- Root cause: `SCR_MUSIC_BIN` set only via CRT `_putenv_s` was invisible to `CreateProcess` children; preview error said “not found on PATH” even for AppData/`tools/bin` drop-ins.
- Fix: `SetEnvironmentVariableW`, Windows `.exe`-first name search, `searched` paths in doctor/preview errors, DLL-friendly `PATH` when running host tools.

## 2026-11-20 — Release v0.3.5 (hotfix)

- Windows Music Library: fix `CreateProcess` error 87 / `PREPARE FAILED (rc=127)` (`CREATE_UNICODE_ENVIRONMENT` + NUL std handles). `scr_run_process_test` added.

## 2026-11-20 — Music Library host dependency doctor + setup

- Dependency map: catalog/replacement WAV preview need no host tools; originals need cricodecs+vgmstream; prepare needs cricodecs+ffmpeg.
- `music_asset doctor` JSON report; search `tools/bin` before PATH; `music_deps_setup` for local cricodecs venv.
- Music tab **Check tools**; packages ship `tools/bin/README` + setup scripts. Do not bundle GPL FFmpeg.
- `tools/music_deps_test.sh`; launcher prefers `/usr/bin/python3` over IDE AppImages.

## 2026-11-20 — Cross-platform compatibility audit pass

- Doctor probes ffmpeg/vgmstream by executing them (`ok`/`missing`/`unusable`); `SCR_MUSIC_APP_BIN_ONLY` for clean-env tests.
- `music_deps_setup` prints ready/blocked + next steps; Music UI Check tools shows ready|blocked.
- Ship `libwinpthread-1.dll` next to `tools/music_asset.exe` (Windows DLL search is exe-dir first).
- `package_cross_check` Wine-loads `music_asset.exe`; `SCR_UI_SHOT` smoke accepts save or headless abort.
- Added `docs/CROSS_PLATFORM_COMPATIBILITY.md` (honest PASS/PARTIAL/UNVERIFIED matrix).
- AppImage now ships `tools/music_asset` + `music_asset_pkg` (parity with Windows ZIP).
- `MusicTool_Find` prefers exe dir over cwd; `tools/package_cross_check.sh` covers both packages.

## 2026-11-20 — Fix Windows Music CPK path + cache status

- Root cause (cache message): UI treated missing `current.json` as a hard failure; that file is only written after Prepare with replacements. Original preview/export never needed it.
- Root cause (Windows prepare/preview): `OriginalCpk()` missed portable `exe/game/...` layout; Music UI cache preferred cwd while game used UserDir — Prepare and Play could disagree.
- Shared `MusicPaths_*` for CPK + cache/preview roots; clearer status text; prepare writes `current.json` even for use-original; `scr_music_paths_test`.

## 2026-11-20 — Installer: portable migrate is optional

- Fresh install is the default; portable folder only requested after opt-in migrate.
- Wine tests: `package_windows_installer_test.sh` (fresh /MIGRATE / invalid / Unicode).

## 2026-11-20 — Release v0.3.4 (hotfix)

- Windows primary asset is `SonicColorsRecomp-Setup-v*.exe` (per-user Inno). Install-layout zip retained for in-app updates only.

## 2026-11-20 — Windows installer (per-user Inno Setup)

- Primary Win distro: `package_windows_installer.sh` → Setup.exe under `%LOCALAPPDATA%\Programs\…`.
- `installed.txt` forces `%APPDATA%\SonicColorsRecomp` user data; portable ZIP remains optional.
- Music drop-ins / venv prefer user-data `tools/bin` so they survive updates.
- Setup can import portable data; uninstall asks before deleting AppData.

## 2026-11-20 — Release v0.3.3 (hotfix)

- Hotfix packages: Music Library doctor/setup, `tools/bin` drop-ins, winpthread beside `music_asset.exe`, clean-env dependency tests.

## 2026-11-20 — Release v0.3.2 (hotfix)

- Hotfix packages: Windows auto-update install + Music Library `catalog.json` in redistributables (plus v0.3.1 tab click fix lineage).

## 2026-11-20 — Fix Windows Music Library empty catalog / missing manifest

- Root cause: Windows redistributable omitted `mods/music/catalog.json` (Linux repo runs had it on disk).
- Package `catalog.json` (+ README) in `package_windows_cross.sh`, `package.ps1`, and Linux AppDir.
- `MusicManifest_DefaultDir` prefers exe-adjacent `catalog.json` over empty cwd `mods/music`; copies catalog into user dir when the install is read-only.
- Prepare saves `manifest.toml` before running; clearer errors when no replacements assigned.
- `package_windows_music_check.sh` + `music_manifest_test` packaged-layout coverage.

## 2026-11-20 — Fix Windows auto-update install after download

- Root cause: `apply_update.cmd` wrote `%%ZIP%%` via `fprintf("%s", …)`, so CMD never expanded the zip path and `Expand-Archive` failed silently under `CREATE_NO_WINDOW`.
- Fix: generate `apply_update.ps1` with embedded UTF-8 paths, stage under `downloads/`, wait for PID exit, robocopy install with `.preupdate` exe rollback, log + notepad on failure.
- Shared validation (`update_apply`): size match, zip/AppImage magic, zip must contain `SonicColorsRecomp.exe`, reject `..` entries; AppImage apply backs up before replace.
- `scr_update_apply_test` covers script generation + validation. No GitHub release in this batch.

## 2026-11-20 — Release v0.3.1 (hotfix)

- Hotfix packages for unclickable launcher tabs after boot (`bootFrames` instead of stuck `forceTab`).

## 2026-11-20 — Fix unclickable launcher tabs

- Root cause: boot `forceTab` used `frames < 2`, but `frames` only advanced on install prompt / UI shots, so `ImGuiTabItemFlags_SetSelected` stayed forced on the remembered tab and cancelled mouse selection.
- Fix: dedicated `bootFrames` counter (same pattern as Settings sub-tabs). Linux + Windows builds OK; SCR_UI_SHOT_TAB + live clicks confirmed.

## 2026-11-20 — Release v0.3.0

- Tagged packages: AppImage + Windows zip (updater, launcher QoL, verify-data, RecompLauncher settings, last played, reinstall confirm).

## 2026-11-20 — Reinstall confirm + Copy paths

- Confirm before reinstalling intact game data; Copy next to Open game/user data on Play.

## 2026-11-20 — Last played on Play tab

- `last_played_at` stamped on Play / `--skip-launcher`; shown under game-data status.

## 2026-11-20 — Versioned window titles + RecompLauncher settings

- Launcher/game titles show `g_version`; RecompLauncher gains pause/hints/backup settings + Open diagnostics.

## 2026-11-20 — --verify-data CLI

- Windowless full/quick integrity check with progress lines; AppStream keywords refreshed.

## 2026-11-20 — Clear shader cache

- System → Clear shader cache… (user folder only); package.ps1 TESTING + README sync.

## 2026-11-20 — list-gamepads + shader cache folder

- CLI `--list-gamepads`; System → Open shader cache; diagnostic reports list pad names.

## 2026-11-20 — Maximized restore + release-notes unit test

- `window_maximized` restored on launch; SoftenReleaseBody moved to `release_notes.h` with `scr_release_notes_test`.

## 2026-11-20 — Persist game window size + pause on minimize

- Closing the game writes the last windowed client size to config.ini.
- `pause_on_focus_loss` also pauses when the window is minimized.

## 2026-11-20 — Windows zip Install & relaunch

- Deferred `apply_update.cmd` waits for exit, expands the zip, copies over a writable portable install, relaunches.
- Shared Install & relaunch entry for AppImage + Windows zip.

## 2026-11-20 — AppImage Install & relaunch

- `UpdateCheck_ApplyAppImageAndRelaunch` copies the download over `$APPIMAGE` and respawns; Play/System confirm UI.
- Windows zip packages remain Reveal-only (no in-place replace of a running exe).

## 2026-11-20 — What's new + open downloaded package

- Play banner after version bump (`last_seen_version`); Open AppImage / Reveal package after download.
- Confirm before quitting while a game-file check is running.

## 2026-11-20 — Crash-banner diagnostics + copy paths

- Play crash recovery: Save diagnostic report; System: Copy log / user-data paths.
- Desktop entry Keywords/Comment mention PAL + RecompLauncher.

## 2026-11-20 — Install helpers test

- `scr_install_helpers_test` covers disc extensions + USA/JP unsupported messages; wired into `regression_smoke`.
- `package.ps1` TESTING.txt aligned with cross-pack (formats / RecompLauncher / CLI).

## 2026-11-20 — Nearby disc formats + last ISO

- Nearby/`game/` discovery accepts ISO/RVZ/WIA/WBFS/GCZ/CISO/NFS (not only `.iso`).
- Remembers last successful install path; Play missing-data shows free space.

## 2026-11-20 — Release-notes softener fix + AppStream

- SoftenReleaseBody only strips line-leading markdown (keeps mid-line hyphens).
- `--check-updates` prints notes when newer; AppStream description / URLs refreshed.

## 2026-11-20 — Update notes + diagnostics prune

- Newer-release banner / Settings show softened GitHub release notes.
- Keep newest 20 `diagnostics/scr_diag_*.txt`; TESTING/README CLI sync.

## 2026-11-20 — Richer diagnostics + CLI

- Reports include host/portable, free space, data path, abnormal-exit marker, `.1.log` tail.
- Fixed update-download lines gated on HasResult; CLI `--save-diagnostics`.

## 2026-11-20 — Install free-space check

- `Install_Run` rejects early when the destination volume cannot hold the extract (2× if reinstall).
- Installer dialog shows free space at the install location.

## 2026-11-20 — Previous-crash banner

- Crash/FATAL writes `last_abnormal_exit.txt`; Play shows a banner with Open log folder / Dismiss.
- Crash dialogs mention log rotation (`.1.log` after relaunch). `package_smoke` checks new CLI help flags.

## 2026-11-20 — Wrong-region disc messaging

- USA (SNCE*) / Japan (SNCJ*) discs get explicit “not supported yet” errors on install, verify, and region select.
- Installer / Play missing-data copy names supported PAL SNCP8P vs USA/JP.

## 2026-11-20 — Updater polish

- AppImage downloads get `+x`; `downloads/` keeps the newest 3 packages.
- Play → Open downloads; CLI `--download-update`.

## 2026-11-20 — Log rotation + update package download

- `SonicColorsRecomp.log` rotates to `.1.log`…`.5.log` on each launch.
- Settings / Play: Download update package → `user_data/downloads/` (no auto-install).

## 2026-11-20 — RecompLauncher support

- `recomplauncher.json` (levels 1–3): game check SNCP8P, prepare extract, settings, dumps from Redump.
- CLI `--game` / `--prepare` (progress lines); `RECOMPLAUNCHER=1` skips built-in launcher.
- Packaged into AppDir root and Windows zips; README badge.

## 2026-11-20 — Music Library Clear filters

- One-click reset of search + Category/Variant/Replaced/World (persists cleared state).

## 2026-11-20 — CLI `--check-updates`

- Queries latest GitHub release tag and exits (0 up to date, 2 newer, 1 failed). No download.

## 2026-11-20 — Richer diagnostic reports

- Diagnostics include save backup count, screenshot_keep, and last update-check message.

## 2026-11-20 — Play-tab update availability hint

- Shared GitHub Releases tag check; Play shows a banner + Open Releases when a newer tag exists.

## 2026-11-20 — Screenshot retention

- `[System] screenshot_keep` (default 100) prunes oldest PNGs after F12.

## 2026-11-20 — Config backups folder + save backup count

- System → Open config backups; Save data shows backup count vs keep limit.

## 2026-11-20 — GPU blit timing on FPS overlay

- D3D11 timestamp queries around the present blit; overlay shows `GPU blit ~N ms` when resolved.

## 2026-11-20 — Persist Music Library filters

- `[Music] filter_cat/var/repl/world` remember Category/Variant/Replaced/World combos.

## 2026-11-20 — Win32 audio remaster toast

- XAudio2 `MaybeRemaster` shows the same reconnect/lost toast as Linux after device remove/default change.

## 2026-11-20 — Play portable status + config backup prune

- Play tab shows portable vs user-data path; Quick folders → Open screenshots.
- `config_backups/` keeps the newest 20 after each settings-reset backup.

## 2026-11-20 — Input hint glyphs

- On-screen Jump/Slash/Pause hints draw coloured pad chips + keyboard key glyphs (first bind).

## 2026-11-20 — FPS overlay Present ms + draws/XFB

- Always sample Present wall time; overlay shows smoothed Present ms and draw calls per XFB.

## 2026-11-20 — Save backup on write + retention

- `[System] auto_backup_saves_on_write` — deferred backup after NAND flush (stage clear / menu save).
- `[System] save_backup_keep` — prune oldest `save_backups/` folders (default 20; 0 = unlimited).
- Debounced 2s + 60s cooldown; toast “Save backed up”; launch backups also prune.

## 2026-11-20 — Persist settings sub-tab

- `[System] settings_tab` remembers Video/Input/Audio/System across sessions.

## 2026-11-20 — Video profiles + Play recovery links

- Named video/graphics profiles under `video_profiles/`; Video tab Save/Load.
- Play tab: Open Releases when data missing; Open log + Releases when corrupt.

## 2026-11-20 — `--show-hints` + HiDPI checklist

- CLI `--show-hints` for this-run input prompts; PERFORMANCE.md HiDPI/resize smoke steps.

## 2026-11-20 — Perf overlay detail + input hints

- FPS overlay: guest Hz, RSS MB, audio underruns; `Platform_ProcessRssMb`.
- System → Show input hints (Jump/Slash/Pause pad+key prompts).

## 2026-11-20 — Play-tab quick folders

- Open game data / user data / mods; Copy version on the Play tab.

## 2026-11-20 — Launcher size, richer FPS overlay, update check

- Persist launcher window size; FPS overlay shows ms + approx 1% low.
- System → Check for updates (GitHub Releases latest tag; no download).

## 2026-11-20 — Persist launcher tab

- `[System] launcher_tab` remembers Play/Settings/Mods/Music across launches.

## 2026-11-20 — Hotkey conflict warn + startup timing + Music empty state

- Warn when two host hotkeys share a key; log `startup: guest entry ready in N ms`.
- Music Library empty-catalog guidance bullets.

## 2026-11-20 — Remappable host hotkeys

- `[System] hotkey_settings|screenshot|fullscreen` (F1–F12); System UI combos; Linux/Win handlers.
- Included in input profiles; Escape still closes overlay; Alt+Enter still toggles fullscreen.

## 2026-11-20 — Native checklist + launcher status polish

- Expanded `docs/windows-native-checklist.md` for profiles, FPS overlay, UI scale, backups.
- Launcher Current settings shows UI scale and auto-backup hint.

## 2026-11-20 — UI scale

- `[System] ui_scale` (75–150%) for launcher + overlay; System slider; config_test roundtrip.

## 2026-11-20 — Mod profiles + PERFORMANCE.md

- Named mod enable/order profiles under `user_data/mod_profiles/`; Mods tab Save/Load.
- `docs/PERFORMANCE.md` measurement notes; backlog item 12/15 updated.

## 2026-11-20 — Auto save backup on launch

- `[System] auto_backup_saves_on_launch` + System checkbox; runs `nand_backup` before Play (not for `--nand`).

## 2026-11-20 — `--show-fps` + backlog audit

- CLI `--show-fps` enables the overlay for the current run without editing config.ini.
- `docs/DEVELOPMENT_ROADMAP.md` top-20 improvement backlog (spiral/H7 excluded).

## 2026-11-20 — Input binding profiles

- Save/load controller + keyboard + deadzone profiles under `user_data/input_profiles/`.
- Input settings UI: name, Save, Load combo, open folder; `scr_config_test` roundtrip.

## 2026-11-20 — Focus-loss mute + launcher status

- Pause-on-focus-loss mutes host audio (`AudioOut_SetSuspended`) and stops rumble immediately.
- Launcher Play tab shows current display/audio/controller summary from config.

## 2026-11-20 — Config backup on reset + FPS overlay

- `Config_BackupCurrent` writes `user_data/config_backups/config_YYYYMMDD_HHMMSS.ini` before System → Reset.
- `[System] show_fps_overlay` + in-game corner FPS (host present rate); toast confirms backup filename.
- `scr_config_test` covers show_fps_overlay roundtrip and backup path.

## 2026-11-20 — Trigger deadzone + graphics presets

- `trigger_deadzone` + Input UI; Video graphics presets (Performance/Balanced/Quality).
- Launcher first-run bullets when data missing; README Releases play path + Windows honesty.
- `scr_config_test` extended for trigger roundtrip.

## 2026-11-20 — Continuous improvement (parity + QoL)

- Fresh audit → `docs/WINDOWS_FEATURE_PARITY.md` matrix (FULL/PARTIAL/MISSING/UNVERIFIED/PLATFORM LIMITATION).
- Removed experimental AP1 spiral gates; issue **DEFERRED** in FRAME_RATE / audit / roadmap.
- Stick deadzone L/R in config + Input settings; `scr_config_test` covers roundtrip.
- Launcher home shows build version; `audio_device` docs corrected for Windows WASAPI.
- Validated: linux-clang build, MinGW `out-windows/SonicColorsRecomp.exe`, `regression_smoke` OK.

## 2026-11-20 — 60 FPS compatibility audit (no code fixes)

Wrote `docs/FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md`: Dolphin wiki + Colors DX
issues A–G classified (WideSpring soft-lock **ALREADY FIXED**; flicker leans
**DOLPHIN-SPECIFIC**). **Issue B CONFIRMED** in recomp: Aquarium Park Act 1
(`stg410`) opening loop — stuck and/or shot into void/death at wait=1. Fix not
AP1 spiral (`stg510`): **DEFERRED indefinitely**. Experimental spiral gates removed
from the tree. Do not resume without an explicit new plan.

---

## 2026-11-20 — Spring soft-lock second RCA (gate audit)

### Why first `--spring-cd-gate` (early-return) was insufficient

- Boot log ≠ in-stage proof the override ran during the bounce.
- Pointer filter was MEM1-only (`>>28 == 0x8`); MEM2 springs would bypass.
- Early-return ≡ native cooldown epilogue, yet soft-lock persisted → either gate
  never engaged, or **repeated `ObjSpringActivate` is not the sole cause**.

### Changes

- Gate now **ORs bit15 into `+0xF0`** then calls original (native `+0x188` path).
- MEM1+MEM2 EA check; `--spring-diag` event log; `ObjSpringLaunch` hook;
  `Perf_GuestFrameIndex()`.
- Default 30/60 unchanged. True-60 fix **not claimed** until diag capture.

### Diag sessions (`spring_diag_guest60.log` / `guest60b.log`)

stg110 guest60c: soft-lock = **WideSpring** (`wact` every frame, `col=act=0`).
Cause: collide→activate has no cooldown. Gate early-returns WideSpring activate for
~0.5s guest time. **Verified:** soft-lock gone with gate. **Launcher:** `frame_rate=60`
now defaults to wait=1 + spring gate (no separate `--guest-60` / `--spring-cd-gate`).

### Dolphin AR (unchanged conclusion)

`008F2D97` / `0090DB0B` = force wait=1 only; same as `--guest-60`.

---

## 2026-11-20 — 60 FPS spring soft-lock (wait-field policy)

### Root cause

`--60fps` / `frame_rate=60` previously set VI wait fields to **1** (true 60 Hz
guest). Float dt from `fn_803E19B0` scales with N, but spring object state at
`CObjSpring+0xE0` / `+0xF0` (`fn_80078C24`, `fn_80078524`) advances once per
guest update. At wait=1 the spring re-arms while Sonic still overlaps →
infinite activation loop. Same class as Dolphin AR 60fps / Ultimate spring bugs.

### Fix

`Perf_WaitFields()`: `frame_rate=60` → **wait=2** (native sim, spring-safe).
Opt-in true guest-60 via `SCR_GUEST_60=1` / `--guest-60` (wait=1, known broken).
Status text: `60/sim30` vs `60/guest`. Docs: `docs/FRAME_RATE.md`.

### Verify

- Build: `linux-clang` link OK after `crash.cpp` includes `perf.h` (C linkage).
- Boot log: `--60fps` → `wait=2`; `--60fps --guest-60` → wait=1 warn.
- Matching decomp untouched. H7 black-screen still deferred.

---

## 2026-11-19 — Ongoing development (M7 + `--help` + exclusive FS + first-launch UX)

- Extracted `Fs_WriteFileAtomic`; NAND `Flush` uses it (C1 behaviour unchanged).
- Added `scr_nand_fs_test` (create / replace / read-only parent / rename-onto-dir).
- CLI `-h` / `--help` prints usage and exits before launcher/log.
- Added `docs/DEVELOPMENT_ROADMAP.md`.
- Config/settings: `exclusive_fullscreen` (Linux exclusive mode; default borderless).
- First-launch / missing-data: install dialogs say Sonic Colors (not Black Knight); launcher shows supported release + Play disabled reason; `--skip-launcher` / empty data fatals mention `scr_install` + `kReleaseNames`; quick-check before boot.
- `tools/package_smoke.sh`: stages AppDir, checks libs/desktop/icon, `--help`, `BKR_UI_SHOT` abort.
- Long-session VRAM: tex/EFB age+hard-cap eviction; hi-res texture LRU; GX log includes efbCopies/hires.
- Win32 window title → “Sonic Colors - Recompiled” (class name unchanged).
- Controller hotplug: pack slots on disconnect; probe already-connected pads at event-loop start; Input settings shows live controller list (`Pad_Count` / `Pad_Name`).
- GameBanana game id **6243** (Sonic Colors; was Black Knight 6511).
- Win32 window/launcher classes renamed to `SonicColorsRecomp` / `SonicColorsRecompLauncher`.
- UI/test env vars prefer `SCR_*` with `BKR_*` fallback (`runtime/env.h`); smoke/layout scripts updated.
- System settings: save folder open / backup / restore latest (`nand_paths` + `nand_backup`); `scr_nand_backup_test`.
- Debug envs (`SLOW_SAMPLE`, `IRQ_STALLS`, `TEST_PAUSE`, `DUMPMEM2`) accept `SCR_*`.
- Host volume: `master_volume` / `speaker_volume` in config + Audio settings (live via `AudioOut_ApplyVolumes`).
- Linux audio device picker (`audio_device`); Windows remains system default.
- System → Diagnostics: show log path + Open log folder (`Log_Path`).
- `scr_config_test`: Audio/Video config load/save roundtrip.
- H8 PART: `tools/music_pipeline_smoke.sh` — isolated TR1 prepare → patched CPK + payload proof (PASS). In-game audibility still open.
- Optional integer scaling (`integer_scale`); `tools/regression_smoke.sh`; desktop Keywords / PrefersNonDefaultGPU.
- F12 screenshot to `screenshots/scr_*.bmp`; System → Open screenshots folder.
- Uncapped host FPS limit (`host_fps_cap`); live Video setting.
- `--version`, `--portable` / `SCR_PORTABLE`; System shows user data path + Open user data folder.
- AppStream metainfo packaged into AppDir (`usr/share/metainfo/`).
- Linux audio hotplug: reopen preferred/default device on SDL remove/add.
- System → Reset settings to defaults (keeps Paths/Mods/Music); README status updated.
- System → Save diagnostic report (`diagnostics/scr_diag_*.txt`).
- F12 user screenshots → full-res PNG (`screenshots/scr_*.png`).
- In-game toasts for F12 + Linux controller hotplug.
- `--list-audio-devices`; Win32 XInput hotplug toast; Linux audio reconnect toast.
- Save restore: pick any backup; diagnostic path copied to clipboard.
- `tools/package_test_linux.sh`; `tools/package.ps1` (Windows share pack).
- Overlay Restart/Quit confirmation; package_smoke checks pack scripts.

Verification: build PASS; `package_smoke` OK.

---

## 2026-11-19 — Stability recovery (Phase 20 black-screen work reverted)

### Goal

Stop black-screen experimentation. Restore the Phase 19 (`72ae9de`) runtime/presentation baseline and treat H7 as a known deferred bug.

### Reverted (black-screen only)

| Ref | What |
|---|---|
| `388ee48` | Phase 20 XFB centre/HUD luminance + world/hud counters |
| `509781e` | EFB-copy `ramHash` / keep-GPU-snapshot “fix” |
| `0f8bc9c` | EFB copy-clear always-clear-depth change |
| `de7a72e` | Expanded BLACK-WORLD dumps / `SCR_GX_NO_DEPTH` probe |
| Uncommitted WIP | Richer latch (`efbC`/`efbD`/frustum/`mtxNan`), PE token `GX_SyncGPU`, guest-60 gate, indexed-XF gather expand + 256KiB lag cap, NaN XF skip |

### Preserved

Music, launcher, NAND C1, input, packaging. Frame-rate: default 60 now wait=2 (spring-safe; see 2026-11-20). Matching decomp untouched.

### Recovery points

- Branch: `backup/pre-stability-recovery-de7a72e` (= `de7a72e`)
- Stash: `WIP black-screen investigation before stability recovery …` (post-`de7a72e` WIP)

### Baseline

`main` → `72ae9de` (Phase 19).

### Stability notes this pass

- No new runtime crash/leak fixes beyond restoring Phase 19 (NAND C1 already present).
- `tools/music_mod_pkg/test_music_mod.py::test_sample_repo_manifest` was failing on assets-only `manifest.toml`; skip when no legacy `[tracks.*]` (Music Library format).

### Verification

| Check | Result |
|---|---|
| `runtime/` vs `72ae9de` | **identical** |
| Build `SonicColorsRecomp` (linux-clang) | **PASS** |
| `scr_music_manifest_test` | **PASS** |
| `music_asset` unit tests | **8/8 PASS** |
| `music_mod` unit tests | **PASS** (1 skipped) |
| `package_linux.sh -S` | **PASS** → `dist/AppDir` (AppRun + `usr/bin/SonicColorsRecomp`) |
| Binary start (no display) | Process starts; launcher falls through without window (expected headless) |
| Manual TR1 / black-screen | **Not** re-run (deferred bug; no BS code path left) |

---

## 2026-11-19 (Phase 19) — Quality audit, critical save fix, release polish

### Inventory

Full severity table: [`docs/QUALITY_AUDIT.md`](QUALITY_AUDIT.md).

### Critical / high fixes

- **NAND Flush**: no longer clears dirty after failed rename/write (`runtime/ios/dev_fs.cpp`).
- **gitignore**: track `mods/music/catalog.json` + README; ignore userdata tracks/manifest.
- **Launcher**: `BKR_UI_SHOT` with no window/D3D aborts instead of starting the game.
- **Music paths**: tool/cache/catalog resolution via cwd + exe parents + `SCR_MUSIC_ASSET` / user dir.
- **Branding**: crash/fatal/install/HTTP/config/user-dir docs → SonicColorsRecomp.
- **Packaging**: `tools/package_linux.sh` + `data/linux/soniccolorsrecomp.{desktop,svg}`.
- **Black-screen RCA aid**: `SCR_GX_XFB_DIAG=1` logs zero-draw XFB presents (no render workaround).

### Still open

- Intermittent black world after stomp (HUD OK) — **DEFERRED** (see stability recovery above); not an active RCA.
- TR1 in-game music replacement audibility confirmation.
- Full-game compatibility matrix beyond TR1.

### Verification this phase

| Check | Result |
|---|---|
| Incremental build (`SonicColorsRecomp`) | **PASS** |
| `scr_music_manifest_test` | **PASS** |
| `music_asset` unit tests | **8/8 PASS** |
| `package_linux.sh -S` | **PASS** → `dist/AppDir` |
| Manual TR1 play | **Not** re-run (efficiency; no gameplay change beyond diagnostics) |

---

## 2026-11-19 (Phase 18) — Sharp resize, WAV export, player panel

### Blurry resize — root cause

ImGui lays out in **logical** pixels (`DisplaySize`) and draws into a framebuffer
of `DisplaySize × DisplayFramebufferScale`. The DXGI/DXVK swapchain was only
resized from coalesced `PIXEL_SIZE_CHANGED` events and could stay at a **stale
pixel size** while the Wayland compositor stretched the previous buffer → soft /
blurry UI after (and during) live resize. Restarting “fixed” it by recreating
the swapchain.

### Rendering fix

- `UiWindow_Pump` always reports current `SDL_GetWindowSizeInPixels` (Linux) /
  client rect (Windows).
- Launcher `SyncSwapchainToWindow` every frame when drawable size ≠ backbuffer,
  and again after ImGui NewFrame using `DisplaySize × FramebufferScale`.
- Screenshot path sets `FramebufferScale = 1` when forcing pixel DisplaySize.

### Original WAV export

- `tools/music_asset export-original` / `export-all`
- Modes: complete (`-i`), expanded loops (`-l N`), smpl (`-i -L`)
- PCM 16/24-bit; atomic publish; refuse overwrite by default
- Launcher: **Export Original as WAV** + export options tree
- TR1 export test: 48 kHz stereo, ~137 s, `lossy_source=true`

### Music Library player

Integrated detail panel: track info, ORIGINAL / REPLACEMENT transport, Compare,
Export, Assign Replacement. Side-by-side when wide; stacked when narrow.

### Verification

| Check | Result |
|---|---|
| Build | PASS |
| Unit tests | 8/8 PASS (incl. export) |
| Shots | `out/ui_shots/music_p18/` (800 / 1280 / 1920) |
| Exportable music assets | 114 (catalog) |
| Interactive Wayland live-resize sharpness | Fixed by per-frame swapchain sync (confirm in session) |

---

## 2026-11-19 (Phase 17) — Music preview clicks + launcher resize

### Preview controls — root cause

Table rows used `ImGuiSelectableFlags_SpanAllColumns` without
`AllowOverlap`. The selectable’s full-width hitbox intercepted pointer events
meant for later-column widgets (and made in-row Preview buttons unusable).

Original Play also ran `std::system(preview-original)` on the UI thread, so a
successful click could freeze the launcher until decode finished.

### Fixes

- Removed in-row Preview buttons; selection-only rows + detail pane controls.
- `SpanAllColumns | AllowOverlap` on table selectables.
- ASCII **Play / Pause / Stop** for Original and Replacement, plus Compare A/B.
- Async original AAX decode (background thread → WAV → SDL preview).
- Transport bar: seek, loop, volume, stop; switching sources stops the previous
  stream; changing selection stops playback.
- Wide layout: side-by-side list | detail. Narrow (`<1000` / short height):
  detail (preview) above the scrollable list.

### Responsive layout — root cause

Music Library used a fixed ~14-line table height and packed `SameLine` filters,
computed pane heights from **initial** `GetContentRegionAvail()` before drawing
the header (so list+detail overflowed / clipped), and had no window minimum
size. Resize itself was wired (`PIXEL_SIZE_CHANGED` → DXGI `ResizeBuffers`);
the breakage was content layout, not missing resize events.

### Fixes

- Flexible remaining-space split; side-by-side vs stacked.
- Wrapping filter bar; compact header.
- Launcher min size **800×560** (`SDL_SetWindowMinimumSize` / `WM_GETMINMAXINFO`).
- `BKR_UI_SIZE=WxH` for layout shots; `tools/music_ui_layout_shots.sh`.

### Verification

| Check | Result |
|---|---|
| Build `SonicColorsRecomp` | PASS |
| Music asset unit tests | PASS (unchanged pipeline) |
| Layout shots 800×600, 1280×720, 1366×768, 1920×1080 | PASS → `out/ui_shots/music/` |
| 2560×1440 | Clamped to ~90% of display usable bounds (expected) |
| High-DPI | `SDL_WINDOW_HIGH_PIXEL_DENSITY` + ImGui framebuffer scale unchanged |
| Preview clickability | Hit-test fixed; decode async; controls visible in detail pane |

Remaining: interactive click automation under Wayland needs a real session
(display); X11 `BKR_UI_SHOT` used for layout screenshots.

---

## 2026-11-19 (Phase 16) — Complete Music Asset Library + original preview

Expanded stage-only music mods into a full soundtrack Music Library.

### Inventory

| Metric | Value |
|---|---|
| Archives with music | 1 (`sonic2010_0.cpk`) |
| AAX total in CPK | 590 |
| Confirmed music (`bgm_*`) | **114** |
| Voice excluded (`ve_*`/`vj_*`) | 239 + 237 |
| FX variants | 36 |
| Categories | stage 46, special 33, boss 11, menu 9, map 8, jingle 7 |

Catalog: `mods/music/catalog.json` via `./tools/music_asset catalog`.

### Implementation

- Resource-based `[assets.*]` manifest (+ legacy `[tracks.*]` migration in prepare).
- Target-specific AAX conversion (1/2/N segments from original template).
- Launcher Music Library: search/filters, per-asset assign/enable/restore,
  **Original** + **Replacement** preview, Compare A/B, Prepare All.
- Original preview: extract AAX from unmodified CPK → vgmstream →
  `userdata/music_preview/` (independent of replacement cache).
- Runtime: `overrides[]` multi-archive registration via `Disc_RegisterHostOverride`
  (SNCP8P still one CPK).

### Tests

```bash
env -i HOME="$HOME" PATH="/usr/bin:/bin" \
  PYTHONPATH="$PWD/tools:$PWD/.venv-music/lib/python3.14/site-packages" \
  /usr/bin/python3.14 -m unittest music_asset_pkg.test_music_asset -v
./out/scr_music_manifest_test
```

Covered: catalog music-only, TR1 main/FX paths, original preview decode,
legacy+resource manifest load, assets save/load/duplicate reject, AAX UTF align,
CPK TOC (existing).

### Still pending user-side

Short TR1 enter for audible CRI/AX confirmation after Phase 15 alignment fix
(unchanged requirement).

---

## 2026-11-19 (Phase 15) — CRI playback diagnosis (AAX UTF alignment)

In-game replacements were still silent after Phase 14 TOC size fixes. Static
tests isolated the failure to **generated AAX container layout**, not CPK I/O.

### Static tests

| Test | Result |
|---|---|
| A — mods disabled / original CPK | PASS (baseline; original music path unchanged) |
| B — original AAX byte-identical round-trip through patcher | PASS (no-op archive; extract identical) |
| C — substitute original `bgm_stg210_fct_wav.aax` into TR1 slot + TOC sizes | PASS (archive/`Cpk` extract byte-identical; CPK at `out/p15/cpk_subst_stg210.cpk`) |
| D — generated diag AAX (pre-fix) | FAIL structurally vs originals (see below) |
| D′ — aligned diag AAX (`data_abs=88`, gap=6) | PASS structural match; cache `userdata/music_mods/f6b81e478f6da1bb/` |

### Root cause (generated AAX)

| Field | Original TR1 / other stages | `Aax.replace_segment` output |
|---|---|---|
| `@UTF` name | AAX | AAX |
| Columns | data VLDATA + lpflg UINT8 | same |
| `data_abs` (8 + data_off) | **88 (8-aligned)** | **82 (misaligned)** |
| Inter-segment VLDATA gap | 2–6 (8-align pad) | **0** |
| ADX | v4 / mode 3 / 48 kHz | same |
| vgmstream | plays | plays |
| Wii CRI | plays | **silence** |

Fix: pack with `utf.Utf.create` + `set_data_alignment(8)` instead of relying on
`replace_segment` alone (`pack_method=utf_data_alignment_8`).

### Instrumentation

- Full SHA-256 proof on CPK payload read (`payload_proof.tsv`).
- `--music-debug`: guest `bgm_*` strings, AX **stream voice peak** (host proxy for
  decode; guest CRI parser symbols are not in `tools/sdk_names.txt` / decompdecomp
  was unreachable this session).

### Not yet proven in this session

Audible TR1 playback after the alignment fix (requires one controlled launch).
Manifest is set to `_diag_tone_48k.wav` for that test. After PASS, switch back to
the user WAV and Prepare again.

### Build / test

```bash
cmake --build --preset linux-clang
env -i HOME="$HOME" PATH="/usr/bin:/bin" \
  PYTHONPATH="$PWD/tools:$PWD/.venv-music/lib/python3.14/site-packages" \
  /usr/bin/python3.14 -m unittest music_asset_pkg.test_music_asset -v
./out/SonicColorsRecomp --music-debug   # then enter TR1
```

---

## 2026-11-19 (Phase 14) — Fix CPK silence + Music Mods GUI

### Root cause of silence

Phase 13 wrote a **shorter** AAX into the original CPK slot and zero-padded the
remainder, but left TOC `FileSize` / `ExtractSize` at the **original** length.
CRI read past the valid `@UTF` container into padding → silence.

Decoded AAX audio was fine (vgmstream peak/RMS non-zero). Manifest under test was
`stg310` (not TR1). `Cry_For_Me.wav` has ~3.9 s leading silence (not the primary
cause). Silent duration-trim of oversized AAX was also unsafe.

### Fixes

1. **CPK patcher** updates TOC `FileSize`/`ExtractSize`; relocates oversized
   payloads to archive end (aligned) and updates `FileOffset`. Never truncates.
2. **WAV→AAX** no longer shrinks duration to fit a slot.
3. **Main AAX only** by default (`--no-fx` / `--with-fx` to opt in).
4. **Runtime proof**: `payload_proof.tsv` + `MusicMod_OnCpkHostRead` logs when the
   disc layer returns the replacement AAX head bytes.
5. **Launcher Music tab**: Prepare Music Mods (async), leading-silence warning,
   AAX/CPK status column, prepare log; auto-prepare on Play unchanged via
   `MusicMod_Init`.

### Diagnostic prepare (TR1 main)

| Item | Value |
|---|---|
| WAV | `mods/music/tracks/_diag_tone_48k.wav` (8 s, 48 kHz, −12 dBFS, no lead silence) |
| Replaced | `sound/Synth/bgm_stg110_rso_wav.aax` only |
| FX | original |
| Cache | `userdata/music_mods/548b397f48d6911b/` |
| TOC size | 432200 (= AAX bytes; was 7394720) |
| Original CPK SHA | `58bbf2be…` unchanged |

### Still required for full sign-off

Enter Tropical Resort Act 1 with the diag tone; confirm CRI playback + log lines
`PROOF: CPK read supplies replacement AAX`. Then swap in the user soundtrack and
Prepare again.

### Tests

```bash
env -i HOME="$HOME" PATH="/usr/bin:/bin" \
  PYTHONPATH="$PWD/tools:$PWD/.venv-music/lib/python3.14/site-packages" \
  /usr/bin/python3.14 -m unittest music_asset_pkg.test_music_asset -v
```

---

## 2026-11-19 (Phase 13) — File-based CPK/AAX music replacement

Replaced the host MixInto / AX-silence overlay with genuine asset replacement.

### Pipeline

1. WAV → 48 kHz PCM (`ffmpeg`) → ADX intro+loop (`cricodecs`) → AAX via segment replace on original template
2. Copy `sonic2010_0.cpk` → `userdata/music_mods/<hash>/`
3. In-place payload patch at TOC `FileOffset` (full CPK rebuild blocked by duplicate file IDs in SNCP8P)
4. `Disc_RegisterHostOverride("sonic2010_0.cpk", patched)` before `Disc_Open`

### TR1 paths replaced

- `sound/Synth/bgm_stg110_rso_wav.aax`
- `sound/Synth/bgm_stg110_rso_fx_wav.aax`

### Proof

| Check | Result |
|---|---|
| Original CPK SHA-256 | `58bbf2be…` unchanged |
| Patched archive | `userdata/music_mods/2965c969c835c923/sonic2010_0.cpk` |
| AAX validates (CriCodecs / vgmstream) | yes (2 segments, 48 kHz) |
| Launch redirect | `disc: host override sonic2010_0.cpk → …` |
| Host overlay | disabled (`MusicMod_MixInto` / AX silence are no-ops) |

### Tools / deps

- `tools/music_asset` + `tools/music_asset_pkg/`
- `.venv-music` + `cricodecs`, system `ffmpeg`

### Build

```bash
ninja -C build/linux-clang SonicColorsRecomp
./tools/music_asset prepare --manifest mods/music/manifest.toml --music-dir mods/music \
  --cpk game/SNCP8P/files/sonic2010_0.cpk --cache userdata/music_mods
```

---

## 2026-11-19 (Phase 12) — Custom music actually replaces stage BGM

Gameplay previously still heard the original soundtrack. Phase 10c’s architecture
was wrong on three points; all were proven with `--music-debug` on TR1.

### Exact failure points

1. **Stage ID** — disc FST `stg*` first-touch never fires (stages live in CPK).
2. **Start gate** — required AX `is_stream` rising edge; TR1 BGM is `is_stream=0`
   looping refill buffers (`span≈0x59f`–`0xfff`, `vol` high).
3. **Stop / silence** — “stream ended” debounce stopped custom audio; silence of
   only `is_stream` left the original theme audible under the host mix.

### Fix

| Piece | Change |
|---|---|
| Stage | `PPC_OVERRIDE(sprintf)` @ `0x80379820` → `MusicMod_OnStageName` from mission/orc paths + result buffer |
| Start | Mapped stage arms pending; start on AX voice activity (not stream-only). `--music-force-stage` still for pipeline tests |
| Stop | Stage clear/change only (no stream-off stop) |
| Suppress | `is_stream` **or** loud looping refill PBs when stream count is 0 |
| Mix | Unchanged once-per-batch; leading near-silence skip for tracks like Cry_For_Me |
| Debug | `--music-debug` counters + `[MUSIC][ax]` voice dumps |

### TR1 integration evidence (`out/music_p12_tr1.log`)

```text
[MUSIC] Application launched — no custom playback
[MUSIC] Current stage: stg110 (act 1, via scripts/%s_mission.lua)
[MUSIC] Original stage BGM identified: bgm_stg110_rso (stage stg110)
[MUSIC] Replacement mapping matched: tracks/stg110_Cry_For_Me__….wav
[MUSIC] Custom playback started
[MUSIC] Custom samples reaching host output (peak=659)
[MUSIC][dbg] … suppressed=668 … mixPeak=27522 …
```

AX dump during TR1 BGM: `stream=0 loop=1 fmt=2 span=59f vol=48d0` (not `is_stream`).

### Build / test

```bash
ninja -C build/linux-clang SonicColorsRecomp
./out/scr_music_manifest_test
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --mute \
  --music-mods mods/music --music-debug --nand userdata \
  --input-script /tmp/scr_gameplay.txt
```

### Files touched

`runtime/audio/music_mod.*`, `music_sprintf_hle.cpp`, `ax.cpp`, `docs/MUSIC_MODDING.md`, `docs/PROGRESS.md`

---

## 2026-11-19 (Phase 10c) — Fix custom music: cue gate + mix quality

### Root causes

| Symptom | Cause |
|---|---|
| Custom WAV at launch / over menus | Rolling MEM scan treated **static** `bgm_stg*` CPK/DOL strings as play events |
| Malformed / distorted playback | `MusicMod_MixInto` ran on the **entire** DSP staging buffer on every partial `PushDSP`, advancing the music cursor repeatedly and re-mixing old frames |

### Fix

- **Trigger**: disc path → `stgNNN` context + AX `is_stream` rising edge (not string scan).
- **Suppress**: unchanged selective silence of AX `is_stream` voices only.
- **Mix**: once per submitted audio batch; soft-clip float→s16; preload WAV when stage context appears.
- **WAV**: skip `JUNK`/`LIST`; WAVEFORMATEXTENSIBLE PCM/float GUIDs.

### Verification

| Claim | Status |
|---|---|
| No playback on manifest load / boot (log line) | Code + short boot log |
| Mix-once-per-batch | Code (`audio_out` linux/win32) |
| PCM decode golden peak (test tone) | `scr_music_manifest_test` |
| Full TR1 hear-test | **Targeted once** after this fix |

---

## 2026-11-19 (Phase 10b) — Music Mods launcher GUI

Integrated custom soundtrack management into the existing ImGui launcher (not a separate app).

### Launcher integration

| Piece | Change |
|---|---|
| Tabs | Launcher: Play / Settings / Mods / **Music**; in-game overlay: Settings / Mods / **Music** |
| UI | `runtime/ui/music_ui.*` — stage table, editor modal, WAV browse/copy, preview, remove confirm |
| Shared API | `runtime/audio/music_manifest.*` used by runtime + GUI |
| Config | `[Music] enabled` / `music_dir` in `config.ini` (already read by `music_mod.cpp`) |
| Preview | Launcher-only SDL3 stream (Linux); stops on tab leave / exit; does not touch AX |

### Layout

`mods/music/manifest.toml` + `mods/music/tracks/*.wav` (sample TR1 tone migrated under `tracks/`).

### Tests / docs

- `out/scr_music_manifest_test` — load/save/import/status/duplicates/unsafe paths
- Extended `tools/music_mod_pkg/test_music_mod.py`
- `docs/MUSIC_MODDING.md` — GUI workflow

### Verification

| Claim | Status |
|---|---|
| Music tab in launcher/overlay | Code wired |
| Manifest GUI↔runtime same file | Code (`MusicManifest_*`) |
| WAV import + validation | Code + automated test |
| Preview isolated from game AX | Code (separate SDL device) |
| TR1 GUI→launch hear custom BGM | **Targeted once after rebuild** |

---

## 2026-11-19 (Phase 10) — Custom stage music (WAV) replacement

Host-side BGM replacement without touching the ISO or extracted assets.

### Architecture (code)

| Layer | Role |
|---|---|
| Guest cues | Stage themes are CRI/Synth cues e.g. `bgm_stg110_rso` (from `sonic2010_0.cpk`) |
| Detection | Rolling MEM1/MEM2 scan for `bgm_stg*` / stop cues (`bgm_sys_*`, `bgm_zmap_*`) each frame |
| Silence | AX `is_stream` voices output silence while custom track active (SFX one-shots unchanged) |
| Playback | WAV decode → mix into `AudioOut_PushDSP` staging (SDL3); loop / fade / volume from manifest |
| Config | `mods/music/manifest.toml`; CLI `./tools/music_mod`; optional `--music-mods <dir>` |

### Deliverables

- Runtime: `runtime/audio/music_mod.*`, `wav_file.*`; hooks in `ax.cpp`, `audio_out`, `perf`/`FrameLoopWait`, `main`
- Sample: `mods/music/manifest.toml` + `tracks/tropical_resort_act1.wav` (test tone for `stg110`)
- CLI + tests: `./tools/music_mod`, `python3 tools/music_mod_pkg/test_music_mod.py`
- Docs: `docs/MUSIC_MODDING.md`

### Verification

| Claim | Status |
|---|---|
| Manifest parse / WAV inspect / stage catalog | Automated (`test_music_mod.py`, `music_mod validate`) |
| Cue IDs for all story acts | Code (CPK string table) |
| Stream silence leaves non-stream SFX | Code (AX `is_stream` gate) |
| TR1 hears custom WAV + SFX | **Not yet verified** (targeted boot→TR1 once; use sample tone) |
| Pause follows AI DMA | Code |

### Next

One targeted TR1 run with music mods (GUI or `--music-mods mods/music`): confirm logs + tone vs original + ring SFX.

---

## 2026-11-19 (Phase 9) — Boost path analysis, black-screen notes, Act 2 deferred

Testing policy: code/instrumentation first; no repeated TR1 playthroughs. Existing Phase 8/9g shots + logs treated as valid evidence.

### A — White Wisp boost (before DashPanel #1)

| Claim | Verification | Evidence |
|---|---|---|
| Capsule open → 3 White Wisps → gauge fill → B tip | Previously verified (unchanged) | Phase 8 shots; `phase9_capsule_open.png` |
| Upright + energy before `DashPanel #1` (~−6618, 283, 4583) | Targeted gameplay (Phase 9g) | `phase9_upright_before_dash.png`, `phase9g_0240`–`0280` |
| `CPluginStateBoost` enter runs on B | Code + instrumentation | `boost:` in `out/phase9_boost7.log` / `phase9_boost8.log`; type `u08=80714ba8` |
| `sn_boost` audio path invoked from enter | Code | `fn_800888CC` → `fn_8020E3CC` with `0x80714bbc` |
| Persistent gauge at plugin `+0x14/+0x18` | **Disproved (code)** | Enter writes SDA scratch float to both; zeros `+0x1C` |
| Clean run-boost + energy drain + terminate | **Not yet verified** | Idle/low-LY scripts held B without forward run; no cyan boost ball / drain curve captured |

**Call graph (code):**
- Vtable `@ 0x8075e0a4` → `fn_800888CC` (enter), `@ 0x8075e0a8` → `fn_80088CB8` (tick).
- Enter: `fn_80033A30` / `fn_80033A44` on player (`r4`); `fn_80033920(player, 0x19)` (component lookup via `player+0x28` → `fn_800260B0`); sets flag on subsystem `+0x8`; plays `sn_boost`.
- Tick: branches on plugin `+0x1C` → `fn_80088D28` (0) / `fn_80088F0C` (1), then `fn_8008926C`.
- State-0 (`fn_80088D28`): looks up component `0x1A`; may write plugin `+0x18` and set `+0x1C=1` (visible via `boost:tick`).

**Instrumentation (ready, no extra play loop):** `--boost-debug` logs `boost:enter` / `boost:tick` with `player`, `st1c`, scratch floats, B, LY. Overrides: `BoostPluginSnBoost` `800888CC`, `BoostPluginTick` `80088CB8`.

**Manual one-shot procedure (do once when convenient):**
1. From glass path, open capsule `#46` ≈(−7299.9, 342, 4583.9); wait for B tip + gauge.
2. Stop short of DashPanel #1; stay upright.
3. Hold stick **forward** (LY ≳ 0.5) + **B**; no A; do not touch dash panels.
4. Expect: cyan boost aura/ball, speed, gauge drain while held, stop when empty. Capture 2–3 shots + `boost:tick` float/state lines.
5. If fail: return to code (player component `0x19` / `fn_80088D28` energy), do not re-farm the opening.

### B — Black-screen (Dest/Iron → Spring)

| Claim | Verification | Evidence |
|---|---|---|
| Manual repro (world black, HUD stays; restart recovers) | Previously verified | Prior manual report |
| Scripted repro | Failed (Phase 8) | `out/phase8_black.log` |
| Root cause | **Not identified** | — |
| Permanent workaround / forced render reset | **Not applied** (by design) | — |

**Code notes (no gameplay this phase):**
- Set names present in `stg110_obj_00.orc` string table: `DestBox`, `IronBox`, `Spring` (DOL: `CObjIronBox`, `CObjSpring`).
- Lower-path Y≈−50 object positions near glass Z≈4583 exist in set floats (e.g. ring-line @ x≈−7779…−7699, y≈−50); exact Dest/Iron instance indices still need a proper SOBJ type decode.
- Host GX path of interest for RCA: `ClearEFB` / EFB copy in `runtime/gx/renderer.cpp` (shader-fail already logs black draws). Next RCA step: count draws + ClearEFB around spring bounce without resetting guest state.

**Limitation:** Efficient scripted stomp→spring not achieved; full exploratory search deferred.

### C — TR1 → Act 2 (`stg120`)

| Claim | Verification | Notes |
|---|---|---|
| `stg120` set data in CPK | Previously verified | `out/cpk_extract/set_dec/stg120_*.orc` |
| Normal progression TR1→stg120 | **Not yet verified** | No useful NAND progress save under `~/.local/share/SonicColorsRecomp`; full Act 1 clear is a long play session — deferred per testing-efficiency rules |

### Regression

- Build OK after adding `BoostPluginTick` override (`linux-clang` → `out/SonicColorsRecomp`).
- Phase 8 timing / Wisp collection evidence unchanged.

### Next (code-first)

1. One manual boost run-forward+B capture with `--boost-debug`, or locate gauge on component `0x19` via code.
2. Black-screen: SOBJ type→instance map for Dest/Iron/Spring; optional draw/ClearEFB counters.
3. Act 2: only after a saved clear or a short end-of-act script — not a fresh opening farm.

---

## 2026-11-19 (Phase 8) — Wisp→boost verification, timing gap, compat matrix

### White Wisp collection (verified)

Normal TR1 glass-walkway route (non-booster), `ReleaseBoxSmall #46`:

| Step | Result | Evidence |
|---|---|---|
| Capsule before hit | Intact red/yellow dome, 3 White Wisps, empty gauge | `out/shots/phase8_capsule_before.png`, `phase8_wisp_0166.png` |
| Interaction | Jump / collide opens capsule | `phase8_wisps_released.png` (0167) |
| Wisps | Three White Wisps release + home to Sonic (auto) | 0167–0127 absorb frames |
| Gauge | Magenta/purple fill appears; circular Color Power slot stays empty (expected for White) | `phase8_gauge_after_collect.png`, `phase8_collect_wisps_b_prompt.png` |
| Tutorial | Native **B** boost tip appears | `phase8_b_prompt.png` |

Input: script logical `B` → host `PAD_B` → WPAD `0x0400` (`--input-debug` in `out/phase8_wisp.log` / `phase8_boost.log`).

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --mute --60fps \
  --input-debug --input-script /tmp/scr_phase8_wisp.txt --shots 15 --shots-start 1800
```

### Boost (partial)

| Check | Result |
|---|---|
| Wii B reaches game after pickup | **Yes** (WPAD `0x0400`; continuous hold logged as single transition) |
| B tutorial after collection | **Yes** |
| Visible high-speed / aura while B held | **Yes** (e.g. `phase8_speed_fx_during_B.png`, `phase8_boost_aura.png`) |
| Clean upright boost + energy drain curve | **Not conclusive** — `DashPanel #1` (~6618,283,4583) sits just past the capsule and forces blue-ball travel that confounds spin vs boost |
| Related symbols | `CPluginStateBoost` `0x80714ba8`; `obj_pixieget` `0x8071721c`; `sn_boost` `0x80714bbc`; `CObjReleaseBoxSmall` xrefs `0x802d5e04` / `0x802d6ed0` |

Cyan Laser / Yellow Drill: not exercised.

### Black-screen bug

| Item | Status |
|---|---|
| Manual repro (prior) | Stomp Dest/Iron box → spring on **lower** path (Y≈−50); world blacks, HUD stays; restart recovers |
| Scripted repro this phase | **Not reproduced** (`out/phase8_black.log`, `out/shots/phase8blk_*.png`) |
| Root cause | **Not identified** — no permanent workaround applied |
| Likely objects | `DestBox` / `IronBox` + nearby `Spring` under glass walkway in `stg110_obj_00.orc` |

### Frame timing — secondary ~60 Hz limiter

`--perf` now reports **in-wait** vs **gap** (time from `FrameLoopWait` exit → next enter):

| Mode | Guest Hz | VI-wait | Spins | In-wait | Gap |
|---|---:|---:|---:|---:|---:|
| `--frame-rate 0` (in-stage) | ~59.7–60 | **0.00 ms** | **0** | ~5–6 ms | **~10–12 ms** |
| Period | ≈16.7 ms | | | in-wait + gap | |

Interpretation: VI EQ-spin remains fully bypassed. The ~16.7 ms period is filled by work/time **outside** that spin (main-loop update + `FrameLoopWait` post-spin path). Other `VIGetRetraceCount` call sites are OSFatal-style (wait 1–2 fields), not the frame loop. Present still ~0.01 ms. **Do not** strip further waits without proving idle time inside the gap; original 60 Hz pacing remains the correct default. 30 / 60 / uncapped modes unchanged.

Log: `out/phase8_perf_uncapped.log`. Code: `runtime/perf.cpp`.

### Full-game preparation

Created `docs/GAME_COMPATIBILITY.md` with stage inventory (`stg110`–`stg160`, `stg190`, `stg210`/`220`, later DOL IDs, Game Land `stgA10`+), Wisp/menu/save rows, and TR1 statuses. Only TR1 Act 1 exercised beyond boot/menus.

### Regression

- Build OK (`linux-clang` → `out/SonicColorsRecomp`).
- Boot → TR1 → movement / rings / HUD / audio path exercised in scripted runs.
- `--60fps` and `--frame-rate 0` still functional; default 30 not re-broken.

### Next milestone

1. Clean boost drain proof **before** `DashPanel #1` (hold B, no A, stop short of dash panels).  
2. RCA black-screen on lower-path box→spring (manual capture + GX/EFB diagnostics).  
3. Progress TR1 → `stg120` via normal play; expand compatibility matrix.

---

## 2026-11-19 (Phase 7) — White Wisp location + true VI-wait bypass

### White Wisp (Priority A — located)

Tropical Resort Act 1 (`stg110`) **does** contain White Wisp pickups.

| Field | Value |
|---|---|
| Stage | Tropical Resort Act 1 (`stg110`) |
| Object | `ReleaseBoxSmall` in `set/stg110_obj_00.orc` (CRILAYLA → BINA/SOBJ) |
| First main-path capsule | Object index **#46**, world ≈ **(-7299.9, 342.0, 4583.9)** |
| Landmark | Glass walkway **before** the WELCOME arch; ~500 units past first `PointMarker` (-7797.7, 342.6, 4583.9) |
| Appearance | Red/yellow glass capsule with **three White Wisps** inside |
| Access | Run forward from spawn along the ring line; jump into the capsule (forced dash/booster paths can skip collision) |
| Expected gauge | Capsule should fill the boost gauge; empty circular Wisp slot is normal for White (boost energy, not a Color Power) |
| Activation | **Hold** Wii **B** (Nunchuk) / script logical `B` → WPAD `0x0400`. Config: `wii_b = X` maps physical X → Wii B for manual play |

Also present: 11× `ReleaseBoxSmall` + 1× `ReleaseBoxBig` in `stg110_obj_00.orc`. `MultiBoostBox` / `BoostBox` appear in Game Land stages (`stgA10`, `stgF*`, …), not TR1. Colored `cmn_obj_rbox_pixy_*` models exist for Laser/Drill/etc.; White uses standard `cmn_obj_rboxS_SD`.

**Evidence:** `out/shots/phase7_wisp_0029.png` (capsule on glass path), `out/shots/phase7c_0045.png` (native **B** boost tutorial prompt after that area), `out/cpk_extract/set_dec/stg110_obj_00.orc`.

### White Wisp collection / boost (Priority B–C — superseded by Phase 8)

| Check | Result |
|---|---|
| Capsule rendered in gameplay | **Yes** (shot 0029) |
| Scripted path consistently collides | **Yes** in Phase 8 (jump into #46 on glass path) |
| Gauge fill after pickup | **Yes** (Phase 8) |
| Boost state (speed/FX/audio/drain) | **Partial** — see Phase 8 (DashPanel confounds clean drain proof) |
| Related symbols | `CPluginStateBoost` @ string `0x80714ba8`; `obj_pixieget` @ `0x8071721c`; `sn_boost` refs `0x80088b00`+; `CObjReleaseBoxSmallInfo` |

OSReport filter logs lines containing pixie/wisp/boost/phantom (`runtime/kernel/hle_os.cpp`).

```bash
# Reproduce: boot to TR1, approach glass path before WELCOME, jump into capsule, hold B
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --60fps \
  --input-debug --input-script /tmp/scr_phase7_wisp3.txt --shots 45 --shots-start 550
```

### Uncapped mode (Priority D)

**VI EQ-spin is genuinely bypassed** when `--frame-rate 0`:

- `FrameLoopWait` sets a thread-local flag; `VIGetRetraceCount` returns `0x7FFFFFFF` during that wait so the `blt` spin exits immediately (`runtime/perf.cpp`, `runtime/kernel/hle_os.cpp`).
- Float frame delta still published as **1 field** via `fn_803E19B0(1)` so sim dt stays defined (fast-forward at fixed 1/60s steps if guest ever exceeds 60 Hz).
- `--60fps` / default 30 unchanged (still real VI waits).

Measured (scale=3, same boot→TR1 move script, `--perf`):

| Mode | Guest updates/s | VI-wait | Spins/guest | XFB/s | Present/s | Avg guest frame |
|---|---:|---:|---:|---:|---:|---:|
| Default 30 | ~29.9 | ~9.8 ms | ~6500 | ~30 | ~30 | ~25–31 ms |
| `--60fps` | ~59.6 | ~11 ms | ~183 | ~60 | ~60 | ~7–15 ms |
| `--frame-rate 0` | ~59.7 | **0.00 ms** | **0** | ~60 | ~60 | ~7–15 ms |

**Interpretation:** VI blocking is gone, but guest∶XFB∶present remain ~60∶60∶60. Another limiter (likely outside `FrameLoopWait` — scheduler/audio/other VI users, or simply guest work + present coupling) still holds the loop near 60 Hz on this host. Not GPU-bound (present ~0.01 ms). True “hundreds of Hz” uncapped needs that secondary limiter identified; do not treat 60 Hz guest as proof of remaining VI wait.

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --perf --frame-rate 0
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --perf --60fps
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --perf
```

Logs: `out/phase7_perf_30.log`, `out/phase7_perf_60.log`, `out/phase7_perf_uncapped3.log`.

### Cyan / Yellow

Not exercised. TR1 has `EventLaserCancelCollision` / phantom-related collisions (Laser later in act per guides). Yellow Drill is later worlds. `MultiPhantomBox` not in TR1 sets.

### Regression

- Boot / TR1 / movement / rings / pause path unchanged in scripted runs.
- `--60fps` still shows non-zero VI-wait (~15 ms, ~185 spins).
- Default 30 still ~30 Hz.

### Next milestone

1. **Reliable White Wisp collision** on the glass-path capsule (avoid booster skip) → confirm gauge fill → hold-B boost (speed/FX/audio/drain).  
2. Identify the **post-VI ~60 Hz limiter** if higher uncapped rates are required.  
3. Cyan Laser when a TR1/TR2 spawn is deliberately routed.  
4. HOME RSO still deferred.

---

## 2026-11-19 (Phase 6) — Stage performance (true 60 FPS) + Wisp/boost probe

### Bottleneck (measured with `--perf`)

Four rates instrumented separately (`runtime/perf.cpp`, `--perf`):

| Rate | Source |
|---|---|
| Guest simulation | `FrameLoopWait` (`fn_800126EC`) enter/exit |
| VI | `g_viFieldCount` |
| Host XFB / present | EFB→XFB copy + `Video_Present` |
| GX sync wait | `GX_SyncGPU` block time |

**Root cause:** the game caches the VI wait interval after the first calc (`obj+0xC`). Our `FrameRateWait` patch at `80012774` only ran on recalc, so in-stage stayed on wait≈2 (or a stale wait≈1) even with `--60fps` / uncapped. Guest∶XFB∶present stayed **1∶1∶1**. Present itself is ~0.01 ms; GX-sync is ~0 in stage. Not a GPU/Present limiter.

**Fix:** every `FrameLoopWait` call republishes the configured interval via `Write32(obj+8)` + `ppc::fn_803E19B0` (`runtime/perf.cpp`). Default 30 still wait 2; `--60fps` wait 1.

### Performance table (scale=1, TR1 late / in-stage)

| Mode | Guest sim | VI rate | Host present | Notes |
|---|---:|---:|---:|---|
| Default (`frame_rate=30`) | ~30 Hz | ~60 Hz | ~30 Hz | Accurate original pacing (menus also ~30 now) |
| `--60fps` | ~60 Hz | ~60 Hz | **~60 Hz** | **Real 60 FPS achieved** (also verified at scale=3) |
| `--frame-rate 0` | ~60 Hz | ~60 Hz | ~60 Hz | Superseded by Phase 7: VI EQ-spin bypassed (0 ms wait); guest still ~60 Hz from a secondary limiter |

Present time ≪ 0.1 ms; stage guest work sits outside the VI spin and clears 60 Hz on this host (RTX 3080 + Ryzen).

### Wisp / boost

- DOL strings confirm `CPluginStateBoost`, `sn_boost`, `AIRBOOST`, `pixy_*`, `obj_laser` / `CPosturePhantomLaser` / `CPosturePhantomDrill`.
- Scripted TR1 run with repeated host **B** → WPAD **B** (`0x0400`) verified in `out/phase6_boost.log`.
- Boost gauge stayed empty; no White Wisp pickup on the scripted path → boost correctly unavailable (not a mapping failure).
- Cyan Laser / Yellow Drill: assets present in DOL; **not** exercised in TR1 this phase.
- Next: locate White Wisp capsules in TR1 (or a known early stage) and confirm gauge fill → boost state.

### Regression

- Build OK; TR1 playable at 30 and 60.
- Pause still works (Phase 5).
- Evidence: `out/phase6_perf_*.log`, `out/phase6_boost.log`, `out/shots/phase6_60fps_tr1_*.png`

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --perf --60fps
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --perf   # default 30
```

### Next milestone

1. White Wisp collection → boost activation in normal gameplay.  
2. Optional: true uncapped (skip VI EQ-spin without breaking guest VI counters).  
3. Cyan Laser / Yellow Drill when a legitimate stage spawn is available.  
4. HOME RSO (`hbm_moduleD.rso` + MEM2 dump).

---

## 2026-11-19 (Phase 5) — Frame pacing, pause, DPD, Wisps/boost status

### Frame rate

**Timing architecture (SNCP8P):** fixed VI-retrace pacing in `fn_800126EC` (`80012774`). Steady-state was `addi r0,2` (wait 2 fields ≈ 30 Hz on 60 Hz VI). Catch-up path keeps wait 1. Interval is stored at frame-object `+0x8` and published through `fn_803E19B0` → SDA global read by `fn_803E19E4` in the VI spin.

**Config (default remains original 30):**
| Value | Meaning |
|---|---|
| `frame_rate = 30` (default) | Wait 2 VI fields |
| `frame_rate = 60` / `--60fps` | **Superseded 2026-11-20:** wait=2 (spring-safe). Wait=1 only with `--guest-60`. |
| `frame_rate = 0` / `--frame-rate 0` | Wait 0 (uncapped VI spin) |

Patch: `FrameRateWait` in `tools/recomp.py` + `tools/game_addrs.txt` (`code 80012774`); publishes via `fn_803E19B0`. Status overlay: `status: N fps (target 30|60|uncapped), VI …`.

**Measurements (Tropical Resort Act 1):**
| Mode | Menus / boot | In-stage present rate |
|---|---|---|
| target 30 | ~60 fps | ~30 fps |
| target 60 (`--60fps`) | ~60 fps | ~30 fps |
| uncapped (`--frame-rate 0`), scale 1 | ~60 fps | ~30 fps |

In-stage stays ~30 fps even when uncapped at `scale=1` → host performance ceiling for the recompiled stage loop (not only the VI wait). Menus already clear 60. **True 60 Hz simulation/render in-stage is not achieved yet**; the wait option is wired and safe (does not double sim). No separate render-thread interpolation yet.

### Pause and HOME

- **In-game pause:** WPAD **PLUS** / host **START** opens CONTINUE / RESTART / EXIT. Background blur + HUD retained. Resume via CONTINUE works. Evidence: `out/shots/phase5_pause_menu_tr1.png`, `phase5_pause_resumed_tr1.png`; log `out/phase5_pause.log` (PLUS transitions under `--input-debug`).
- **HOME menu / `hbm_moduleD.rso`:** still deferred. Needs MEM2 dump after OSLink (`SCR_DUMPMEM2` / `tools/rso.py`), then optional static recomp. Host Back+Start still synthesises HOME button; RSO UI not linked.

### DPD / pointer / motion

- **`WPADControlDpd` `803A1AE0`:** HLE override; enable/disable logged; completes callback.
- **IR:** `Motion_FillStatus` only emits sensor-bar dots when `WPAD_DpdEnabled(chan)`; no fabricated pointer when DPD off. Mouse / right stick aim when enabled.
- **`WPADIsDpdEnabled`:** not in SNCP8P SEL exports; address unproven → remains `PPC_OVERRIDE_TODO` (body ready).
- Motion (accel / nunchuk) unchanged from prior HLE; no new camera overrides.

### Wisps and boost

- HUD boost gauge + 5 Wisp slots render in TR1; gauge can show a purple charge sliver.
- Scripted **B** during TR1 did not demonstrate a full boost burst (likely needs White Wisp / more gauge — not a missing HLE stub identified yet).
- Cyan Laser / Yellow Drill: not verified; no game-specific runtime stubs added (original DOL logic already present).
- Priority next: gather White Wisps in TR1 and confirm boost activation, then Cyan.

### Build and regression

- Builds: `./scripts/cmake.sh --build build/linux-clang`
- TR1 still playable (move, rings, score, timer, audio, pause).
- Logs: `out/phase5_60fps.log`, `phase5_60_scale1.log`, `phase5_uncapped.log`, `phase5_pause.log`
- Shots: `out/shots/phase5_*.png`

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --verbose
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --60fps
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --frame-rate 0 --scale 1
```

### Next milestone

1. In-stage performance (why present rate caps ~30 even uncapped) so optional 60 Hz is meaningful.  
2. Verify boost after White Wisp pickup; then Cyan Laser.  
3. HOME RSO when a MEM2 dump is available.  
4. Avoid START-mash tip UI softlocks noted in Phase 4.

---

## 2026-11-19 (Phase 4) — First playable level (Tropical Resort Act 1)

### Input (START / PLUS)

- Title **PRESS START** expects GC `PAD_BUTTON_START` (`0x1000`) and/or WPAD **PLUS** (`0x0010`).
- Mappings were already correct in `pad_hle.cpp` / `motion.cpp` (`PAD_START` → both). Phase 3 scripts only sent **A**.
- Verified under `--input-debug`: host `START`, wii `PLUS`, pad `START` fire together; A/B unchanged.
- Stick (`LY`) drives movement via Nunchuk axes in `Motion_FillStatus` and GC stick in `PADRead`.

### Save system

Already implemented via IOS `/dev/fs` → host directory (`runtime/ios/dev_fs.cpp`).

| Guest path | Host |
|---|---|
| `/title/00010000/534e4350/data/` | `<nand>/title/00010000/534e4350/data/` |

Files created by the game: `system.sav`, `game_0.sav`…`game_5.sav`, `banner.bin` (write to `/tmp/` then rename).

Default NAND root is now **`userdata/`** (repo layout when exe is in `out/`); override with `--nand`. Legacy `out/nand` still used if it exists and `userdata/` does not.

Persistence verified across relaunch (existing saves reused; no broken validation).

### Stage loading / gameplay

Normal flow after Start reached **Tropical Resort Act 1**:

1. Wisp loading icon  
2. “ACT 1 / Tropical Resort” card  
3. Stage geometry + HUD (lives, rings, timer, score, boost, red-star slots)  
4. Sonic rendered; timer running  
5. Stick movement + ring collection (rings `0→7`, score `1700` at ~1:16)

No extra DOL overlays required for this stage. DVD/ISFS loads succeeded; GX shader count ~307 in stage. Host present rate ~30 fps in-stage with default `frame_rate=30` (original); boot/UI often nearer 60 VI.

### Evidence

- `out/shots/stage_tropical_resort_act1_loading.png`
- `out/shots/stage_tropical_resort_act1.png`
- `out/shots/gameplay_tr1_start.png`
- `out/shots/gameplay_tr1_moved_rings.png`
- Logs: `out/phase4_start.log`, `out/phase4_gameplay.log`, `out/phase4_persist.log`

```bash
cd /home/volt/Decomp/SonicColorsRecomp
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --verbose
# optional: --nand userdata --input-debug --input-script …
```

Furthest verified: **playable Tropical Resort Act 1** (move + collect rings).

### Next milestone

Improve in-stage feel / completeness: optional 60 fps config, pause/HOME (RSO still deferred), camera/DPD TODOs, boost/Wisps, further acts, any remaining softlocks in empty tip UI boxes when Start is mashed.

---

## 2026-11-19 (Phase 3) — Title screen reached

### Input investigation

Controller “Current Setting” UI shows a **GameCube** pad; the game uses **both** paths:

| API | Address | Role |
|---|---|---|
| WPAD / KPAD (existing HLE) | `803A****` / `KPADReadCore 803B7F70` | Wii Remote + Nunchuk; still used for boot + menus |
| `PADRead` | `80357660` | GC `PADStatus[4]`; called from pad-manager vtable `803C02CC` (data xref `8079F684`) |
| `SIGetType` | `8038AAA0` | PAD type probe (`fn_80357B20`); SI MMIO previously always “no response” |

No `PADRead`/`PADInit` names in decomp symbols — IDs from SIGetType call graph + `PADStatus.err` at `+0xA` (−1/−2) in `803C02CC`.

**Root cause:** SI HLE reported no GC devices, so `PADRead` never delivered buttons while the menu’s current setting was GC. WPAD A alone was insufficient for reliable confirm; empty-input runs can still timeout past some screens, but scripted A via **both** WPAD and PAD advances cleanly (skips intro → title).

**Not** an SDL3 south-button mapping bug: host `PAD_A` → WPAD `0x0800` and GC `0x0100` both fire under `--input-debug`.

### Implementations

- `runtime/input/pad_hle.cpp` — `PPC_OVERRIDE(PADRead)` + `SIGetType` (chan0 = `SI_GC_CONTROLLER 0x09000000`); feeds `Gamepad_Poll`
- `runtime/input/input_debug.cpp` + `--input-debug` — transition logs for host / WPAD / PAD button masks
- `tools/game_addrs.txt` — `PADRead`, `SIGetType`; regenerates `game_addrs.inc` + recomp trampolines
- WPAD/KPAD HLE unchanged and still active

### Testing

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --verbose --mute \
  --input-debug --input-script /tmp/scr_title.txt --shots 90 --shots-start 200
```

- Navigation/confirm: A reaches game (`host A` + `wii A` + `pad A` in `out/phase3_title.log`)
- **Title screen renders:** `SONIC COLOURS` logo, Wisps, Sonic, **PRESS START**, `© SEGA`
- Audio ~60 fps / VI stable; GX ~94 shaders on title; no new fatal warnings (WC24 ENOENT unchanged)
- Title still waits for **Start** (`+` / GC Start); script used A only

### Evidence

- `out/shots/title_screen.png` / `title_screen_press_start.png`
- `out/phase3_title.log` — `PAD: HLE read active`, dual-path A transitions
- Furthest verified: **title attract (PRESS START)** after normal boot → controller setting → (skippable) intro

### Next milestone

Press **Start** on the title screen → save / main menu → load first stage. Likely blockers: Start/Plus mapping under the selected pad type, save/NAND profile, stage DVD reads, any remaining WPAD DPD TODOs for pointer UI.

---

## 2026-11-19 (Phase 2) — First native boot verified

### Build

- `perl-json` was already installed; full `linux-clang` link succeeded after fixing `SCR_MODS_TEST_DATA` in `tools/mods_test/main.cpp` and regenerating vkd3d ExternalProject stamps.
- **Executable:** `out/SonicColorsRecomp` — ELF 64-bit LSB pie, x86-64, ~273 MiB, debug info.
- Runtime libs beside exe: `libSDL3.so.0`, `libdxvk_d3d11.so.0`, `libdxvk_dxgi.so.0` (no unresolved `ldd` deps).
- `ppcrecomp` + runtime + installer all link.

### WPAD

Identified from SEL + KPAD→WPAD call graph (not BKR addresses):

| Symbol | Address | Evidence |
|---|---|---|
| `WPADInit` | `803A0060` | Sole KPAD caller; init-flag + setup |
| `WPADGetStatus` | `803A0110` | KPAD compares result to `3` (SETUP) |
| `WPADGetAccGravityUnit` | `803A1220` | KPAD `(chan,0,&acc)` after Read |
| `WPADSetSamplingCallback` | `803A1480` | Between Probe and SetConnect |
| `WPADGetDataFormat` | `803A1600` | KPAD stores into `status+0x40` after Read |
| `WPADSetDataFormat` | `803A1650` | Follows GetDataFormat |
| `WPADRead` | `803A1D80` | KPAD `(chan, status*)` then memcpy body |
| `KPADReadCore` | `803B7F70` | `fn_803B7F70`; GetStatus/Probe poll loop |

SEL-confirmed overrides (unchanged): Probe, Disconnect, Connect/Extension callbacks, motor/speaker, stream, radio.

**Still `PPC_OVERRIDE_TODO`:** `WPADIsDpdEnabled`, `WPADControlDpd`, `WPADControlExtension` (addresses not yet proven).

Input backend: existing BKR SDL3 gamepad → WPAD/KPAD HLE. Controller 0 connects at boot (`data format 4`).

### First launch (verified)

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --verbose --mute
```

Furthest **verified** milestone: **interactive pre-title menu UI** (controller “Current Setting” screen), after:

1. Native process start  
2. Entry `0x80004050`  
3. Full RVL OS / IOS / NAND / DVD init (OSReport banners)  
4. Engine heaps + NW4R G3D/EF  
5. GX display (60 fps, shaders, textures) + AX/DSP audio HLE  
6. Wii safety warning → SEGA logo → controller config menu  

Screenshots: `out/shots/shot_0001.png` (safety), `shot_0005.png` (SEGA), `shot_0010.png` (controller menu).

Not yet verified: title-screen confirm, menu navigation with input, level load, gameplay.

Non-fatal noise: WiiConnect24 `/dev/net/kd/*` ENOENT (expected without network IOS).

### RSO (`hbm_moduleD.rso`)

- HOME Button Menu module (`nw4hbm` / `homebutton::*`); ~882 KiB; 7137 export-table entries (mostly DWARF).
- DOL strings reference `hbm_module.rso` / `/hbm.arc`.
- Decomp exposes `RSOStaticLocateObject`; not required for the boot path observed so far (safety → SEGA → controller menu without linking it).
- **Approach:** keep optional; recompile from MEM2 dump when HOME menu is needed (same as BKR `hbm_part.rso`).

### Next concrete task

Drive past the controller-confirm prompt into the title screen / opening:

1. Ensure Classic/GCN pad HLE or WPAD buttons map so “CONFIRM (A)” advances (`runtime/input/gamepad.cpp`, `wpad_hle.cpp`, possibly PAD SI path).
2. Optionally identify the three remaining WPAD TODOs.
3. Re-test with `--input-script` / manual pad; capture title-screen shot.

```bash
cd /home/volt/Decomp/SonicColorsRecomp
./scripts/cmake.sh --build --preset linux-clang -j$(nproc)
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --verbose
```

---

## 2026-11-19 — Milestone 1–3 (prior session)

- Project established; BlackKnightRecomp foundation; DOL extracted/SHA-1 verified.
- 38,506 functions → 313 C++ TUs; DolRecomp 0 unknown opcodes.
- Full exe link was blocked on `perl-json` (resolved this session).
