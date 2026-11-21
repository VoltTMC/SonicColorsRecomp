# Frame rate — Sonic Colors Recomp

Guest VI wait fields and derived `dt` (`fn_803E19B0` → SDA `0x808F2D98` / `lbl_808F2D98`).

| Config | Wait fields | Simulation rate | Presentation | Notes |
|--------|-------------|-----------------|--------------|-------|
| `frame_rate=30` | 2 | 30 Hz | ~1∶1 with sim | Original pacing |
| `frame_rate=60` / `--60fps` | 1 | 60 Hz | ~1∶1 with sim | Spring cooldown gate on by default |
| `--guest-60` / `SCR_GUEST_60=1` | 1 | 60 Hz | ~1∶1 with sim | Forces wait=1 even if `frame_rate` is 30 |
| `frame_rate=0` | 0 (VI waits skipped) | **60 Hz fixed** | Latest-frame at display refresh | Optional `host_fps_cap` |

`SCR_SPRING_CD_GATE=0` disables the WideSpring activate hold.

CLI: `--vsync` / `--no-vsync` override config for one run.

## Metrics (read carefully)

| Metric | What it measures |
|--------|------------------|
| `simulation_hz` | Guest `FrameLoopWait` / simulation steps per second |
| `unique_xfb` | Unique guest XFB copies (new game images from EFB→XFB) |
| `unique_frames_presented` | First Present of each new XFB (never counts re-Presents) |
| `present_calls` | Host `IDXGISwapChain::Present` invocations (may re-show the **same** XFB) |
| `guest_vi_hz` | Emulated Wii VI field rate (~60 Hz NTSC) |
| `display_refresh_hz` | Physical monitor mode from the OS (`Window_DisplayRefreshHz`) |
| `vsync` | `Present(1)` vs `Present(0[, ALLOW_TEARING])` |

**`present_calls` is not unique game FPS.** On a 120 Hz panel with 60 Hz `unique_xfb`, ~120 `present_calls` means each image may occupy two refreshes — not 120 distinct rendered game frames.

**`unique_frames_presented` ≈ `unique_xfb` today** (each XFB Present is immediate). Re-Presents from `Renderer_RepeatLastXfb` inflate `present_calls` only.

Actual panel refreshes are not counted separately (compositor feedback is unavailable / unreliable across Wayland + DXGI/vkd3d). We report `display_refresh_hz` from the OS mode instead of inventing a “displayed FPS.”

## Semantics of `lbl_808F2D98` (SDA float dt)

From the SNCP8P decomp (`fn_803E19B0` / `fn_800115BC`):

- Integer wait fields `N` are stored at SDA `0x808F2D94`.
- Float `dt = N / 60` (seconds per guest step) at `0x808F2D98`.
- Many guest systems still advance **once per loop iteration**; wall-clock dt alone does not make the engine frame-rate independent.

## Pipeline (why unique XFB stays ~60 Hz)

```
Guest VI (~60 Hz)
    → FrameLoopWait (pace / skip VI barriers when uncapped)
    → guest update + draw (fn_800115BC → fn_80014AA8 → fn_80014BD0)
    → GX FIFO consume (not retained for replay)
    → EFB → XFB copy (BP 0x52)     ← unique_xfb++
    → Host Present                   ← unique_frames_presented++ (first time)
PaceUncappedHost (uncapped, VSync off)
    → re-Present same XFB at display_refresh_hz   ← present_calls only
```

| Stage | Behaviour |
|-------|-----------|
| VI / `FrameLoopWait` | Uncapped skips VI EQ/flush/retrace but **holds sim to 60 Hz** with `dt=1/60` |
| Guest update | One update+draw dispatch per wait — no separate render-only entry |
| GX | FIFO is consumed on the GPU thread; no safe retained stream to replay |
| XFB | One unique copy per guest EFB copy; host blit cannot invent new game images |
| Prior VI-bypass without 60 Hz pace | Sim ran at Present rate → **accelerated gameplay**; pacing must stay |

VI bypasses, fixed `dt`, and the sim/present split are **necessary** for correct wall-clock gameplay and audio. Removing them without a new architecture reintroduces speedup or re-locks presentation to VI.

## Uncapped mode (`--frame-rate 0`)

### Current architecture (presentation uncapped only)

```
each FrameLoopWait (uncapped):
  publish N=1, dt=1/60
  skip VI EQ / flush / retrace waits
  PaceUncappedHost:
    until next 60 Hz sim deadline:
      pump Sched_RunDue + ppc_check_irq   # audio
      Present latest XFB at display_refresh_hz (or host_fps_cap)  # VSync off only
```

| Setting | Present cadence |
|---------|-----------------|
| VSync on | One `Present(1)` per unique XFB (synced to **host** refresh edges). Pace loop pumps audio only — no re-Present(1) (would starve AI/AX). Compositor holds the image across refreshes (e.g. 60 Hz content on 120 Hz). |
| VSync off | `Present(0)` + tearing if supported; latest-frame re-Present paced to ≈ display refresh |
| `host_fps_cap` > 0 | Cap `present_calls` when VSync is off (still not unique_xfb) |

**Honest claim today:** uncapped / high-refresh = up to `display_refresh_hz` presentations of ~60 Hz unique content, with correct sim/audio timing. **Not** genuine unique game frames above 60 Hz.

## Approach A1 — Renderer feasibility (2026-11-21)

Investigation only: can we emit **>60 unique XFBs/s** while sim/audio stay at 60 Hz?  
**Verdict:** not with a safe short prototype. Recommended long-term path is **host-side pose capture → interpolated second render**, after FIFO-archive + matrix instrumentation. Guest redraw and blind FIFO replay are blocked.

### Pipeline map (source references)

```
fn_800123A0                          # main frame driver (decomp text_800113C0.c)
  → fn_800115BC                      # one game step: dt + phase globals
       → fn_803E19B0                 # wait → lbl_808F2D98 dt
       → fn_80014AA8                 # update dispatch (fn_80043F1C, r4=0)
            → fn_80014BD0            # NOT draw-only: may call update slot 3
                 → fn_8053E8C0       # vtable +0x18, r4=3
                 → fn_8053F07C → fn_8053FCA4   # render walk, draw vtables
  → fn_80012670                      # GX/VI housekeeping
  → FrameLoopWait (fn_800126EC)      # runtime/perf.cpp override
       → PaceUncappedHost            # re-Present only (Renderer_RepeatLastXfb)

Guest GXFIFO MMIO
  → GPFifo_Write* (runtime/hw/mmio.cpp)
  → Gather → s_ring (runtime/gx/gx_fifo.cpp)
  → GpuThread → ProcessFifo → RunCommands
       → LoadXF / LoadBP / Render_Draw
  → BP 0x52 → DoEFBCopy (runtime/gx/renderer.cpp)
       → s_xfbTex + Video_Present     # unique_xfb++ / unique_frames_presented
```

| Stage | Files / symbols | Sync / ownership |
|-------|-----------------|------------------|
| Sim step | `fn_800115BC`, `lbl_808F2D98`, `lbl_808F3360` phase | Once per `FrameLoopWait` |
| Update+draw | `fn_80014AA8`, `fn_80014BD0`, `fn_8053FCA4` | Coupled; draw path can still hit update slot 3 |
| FIFO ingest | `gx_fifo.cpp` `Gather`, `s_ring` 32 MiB | CPU publishes; GPU consumes |
| FIFO lifetime | `ProcessFifo` **erases** consumed bytes | **No host replay buffer** |
| GX registers | `g_gx` in `gx_state.h` | Persist across frames; cleared only in `GX_Init` |
| Geometry batches | `s_verts` / `s_indices` in `renderer.cpp` | Cleared in `Flush()` — not retained |
| EFB/XFB | `s_efbColor`, `s_xfbTex` | XFB blit = last completed image |
| Re-present | `Renderer_RepeatLastXfb` | Present only; **zero new draws** |
| Display lists | Opcode `0x40` `CallDisplayList` | Guest RAM only; nested DL ignored |
| PE interrupts | `GX_PE_Finish` / token on BP `0x45`/`0x47`/`0x48` | Replay would risk double IRQs |

### A/B/C/D comparison

| Approach | Feasible? | Unique frames? | Evidence |
|----------|-----------|----------------|----------|
| **A. Re-render retained guest state** | **No (unsafe)** | Would, if redraw were pure | `fn_80014BD0` still calls `fn_8053E8C0` (update slot 3). `fn_8053FCA4` walks registries and `bctrl`s draw vtables — second call can double FIFO traffic / advance draw-tied anim/FX. No render-only entry in the chain. |
| **B. Capture & replay GX FIFO** | **Not today** | Identical replay ≠ new image; patched matrices could | `ProcessFifo` erases `s_fifo`; ring advances `s_ringR`. No archive API. Replay of identical commands reproduces the same pixels. PE finish/token IRQs must be suppressed on replay. Vertex data often lives in guest RAM (still valid) but draw list is gone. |
| **C. Host frame interpolation** | **Long-term yes, hard** | Yes (new images) | No motion vectors/depth export for a full warper. `g_gx` XF projection/viewport (`XF_PROJECTION`/`XF_VIEWPORT`) are readable at XFB time (`LoadXF` → `g_gx`). Player/camera world matrices are **not** in `game_addrs.txt`; need actor lookup (`lbl_808F3378` / type `0x88C0` leads). Particles/UI/transparency break simple warp. |
| **D. Alternatives** | Partial | — | (1) Host debug XF capture — **shipped as A1 groundwork**. (2) Future FIFO ring archive + matrix patch — prerequisite for B→A hybrid. (3) Temporal XFB blend — unique pixels but not game-correct; rejected as “FPS”. (4) Approach B sim Hz — **rejected**. |

#### Terminology (not equivalent)

| Phrase | Meaning here |
|--------|----------------|
| Re-present existing framebuffer | `Renderer_RepeatLastXfb` — inflates `present_calls` only |
| Same sim state drawn again | Guest `fn_8053FCA4` twice, or FIFO replay without matrix change — unsafe / identical |
| Interpolate between sim states | Host blends poses from two captures, then renders — Approach A target |
| Higher-frequency guest drawing | Extra `fn_80014BD0` / full steps — rejected without side-effect audit |

### Recommendation

**Safest practical architecture:** keep **60 Hz guest sim + audio**; build **Approach A** as:

1. **Capture** (A1+): XF projection/viewport every XFB; then player/camera world matrices from guest actors.  
2. **Archive** (prerequisite): optional ring of FIFO bytes between XFB copies (debug flag), with PE IRQ suppression on replay.  
3. **Second XFB** (A3): either (preferred) host/patched replay with **interpolated** camera/player XF writes, or a narrow host path that only moves known matrices — **never** blind guest redraw.  
4. Promote only after TR1 timing / audio / HUD checks (A4).

**Genuine 120 unique FPS** is **not** achievable with the existing “consume FIFO → one XFB → re-Present” path. It needs **substantial renderer instrumentation** (capture + archive + safe second pass), not a present-loop tweak. It does **not** require raising simulation frequency.

### A1 groundwork shipped (not a unique-frame prototype)

| Item | Detail |
|------|--------|
| Code | [`runtime/debug/a1_xf_capture.cpp`](../runtime/debug/a1_xf_capture.cpp) |
| Enable | `SCR_A1_XF_CAPTURE=1` |
| Behaviour | On each `Perf_OnXfbCopy`, snapshot `XF_PROJECTION` (7 floats) + `XF_VIEWPORT` (6). FPS overlay shows snap count / proj change. |
| Does **not** | Change sim Hz, emit extra XFBs, replay FIFO, or alter Present when disabled |

**Blocked for a unique-frame prototype this phase:** no safe way to emit a *visually distinct* second XFB without either (unsafe) guest redraw, (identical) FIFO replay, or (not yet built) pose interpolation into a new render pass.

### Approach A1b — guest pose / camera-state capture (2026-11-21)

Opt-in immutable snapshots at completed sim-frame boundaries for future host interpolation. **No extra unique XFBs, no guest redraw, no FIFO replay.**

| Item | Detail |
|------|--------|
| Code | [`runtime/debug/a1b_pose_capture.cpp`](../runtime/debug/a1b_pose_capture.cpp) |
| Enable | `SCR_A1B_POSE_CAPTURE=1` (default off); `SCR_A1B_POSE_DUMP=1` for log lines |
| Ring | `A1bPoseSnap` current + previous; `seq` ++ once per `Perf_OnGuestFrameEnd` |

#### Guest sources (decomp-verified)

| Field | Source | Notes |
|-------|--------|-------|
| Player shell | `PlayerGetPos` `fn_80033C34` **r4**; also `PlayerGetWorldMtxPtr` `fn_80033C78` **r3**; `BoostPluginTick` **r4** | MEM1/MEM2 (`0x8…` / `0x9…`) |
| Body | shell `+0x28` → `+0x84` | Same walk as `fn_80033C78` |
| World mtx | body `+0x0C` (3×4 GC MTX) | Pointer from `fn_80033C78`; may need `fn_80053718` refresh before translation is non-zero |
| Position | body `+0x168` | `fn_80033C34` |
| Camera mgr | `CameraManagerTick` `fn_80571720` **r3** | Contains `PushWorldUpVector` @ `0x80571B44` |
| Camera node | mgr`+0xBC`→`+0x20`→`+0xC068`, else list head mgr`+0xC0` | Tick’s r27 path / `fn_805C1540` |
| Cam pos | node `+0x90` / alt `+0xA8` | Written/read in `fn_80570FF0` / Tick |
| View mtx | First XF LoadXF into pos-mtx 0 each GPU frame | Via `Render_OnXF` → `A1b_OnXfPosMtx0` |
| Projection / viewport | `XF_PROJECTION` / `XF_VIEWPORT` at XFB | Reuses A1 registers |

**Not the player shell:** `PlayerSingletonPtr` / `lbl_808F3378` (type `0x88C0` from `fn_80015E84`) — `+0x28` is not the shell chain.

#### Matrix / coordinate conventions

- Guest matrices are GameCube **3×4 row-major** `MTX`: rows `[r0 r1 r2 tx]`, `[r3 r4 r5 ty]`, `[r6 r7 r8 tz]`.
- Handedness / multiply order match GX (right-handed view, column vectors on the right when expanded to 4×4).
- Host D3D expects conversion at draw time (existing renderer path); capture stores **guest** floats unmodified.
- `XF_VIEWPORT` is **not** a view matrix (A1); use `viewMtx` / `gxPosMtx0` for orientation.

#### Scene-wide interpolation requirements (not implemented in A1b)

| Category | Status |
|----------|--------|
| Sonic root / body pose | **Capturable** (this phase) |
| Gameplay camera / view / projection | **Capturable** (mgr + XF view + A1 proj) |
| Enemies / NPCs | **Needs guest instrumentation** (actor registry walk / per-type hooks) |
| Animated props / moving platforms | **Needs instrumentation** (object update vtables) |
| Skeletal bones | **Needs instrumentation** (`cPoseAccumulator::GetNodeMatrix` paths; not yet typed) |
| Particles / FX | **Unsuitable** for pure transform interp (spawn/lifetime/GPU state) |
| Transparent sorting | **Unsuitable** without re-submitting draw order |
| HUD / menus | **Unsuitable** (2D / screen-space; keep at sim rate or separate pass) |

**What still blocks a full interpolated scene pass:** no FIFO archive, no safe second draw, no actor/bone capture, particles/UI not transform-warpable, and player body `+0x0C` translation may stay identity until the guest refresh path is hooked read-only.

#### Validation (TR1 path, `userdata` NAND + input script)

- Capture **off**: no `A1b:` logs; normal 60 Hz present.
- Capture **on**: `player=ok` after stage entry; `camMgr`/`view` live; `seq` tracks `sim`; `pair=yes` with `prevSeq=seq-1`; `xf=ok` on XFB dump; `simulation_hz`≈60; underruns≈0 in-stage; A1 XF still works when co-enabled.
- **No unique-frame claim.**

### Approach A roadmap

| Phase | Goal | Exit criteria | Status |
|-------|------|---------------|--------|
| A0 | Honest metrics + docs | No claim of unique>60 | Done |
| A1 | XF / camera capture investigation + XF snap groundwork | Overlay shows XF snaps; zero gameplay change; docs of A–D | Done |
| A1b | Resolve player/camera world matrices in guest RAM | Stable addresses or type-walk; debug overlay poses | **Done** |
| A1c | Scene-wide GX pos-mtx + body refresh capture | Bounded opt-in dumps; coverage/correspondence report | **Done** |
| A2 | Debug-only host warp/composite from two captures | Visual check; **not** counted as `unique_xfb` | Later |
| A2b | Opt-in bounded FIFO archive (no replay) | Exact pre-`ProcessFifo` bytes + resource inventory; dumps/compare | **Done (archive only)** |
| A2c | Resource closure + isolated scratch-EFB replay | Replay-ready gate; PE-suppressed scratch replay; no live XFB | **Done (experiment)** |
| A2d | EFB initial-state + host-state closure | Live-vs-scratch pixel MATCH for selected pass | **Done (experiment)** |
| A2e | Second independent static/UI pass MATCH | Two structurally different families both MATCH | **Done (experiment)** |
| A2f | Controlled EFB→tex feedback replay | Scratch-sourced copy chain MATCH | **Done (experiment)** |
| A2g | Cross-frame EFB texture retain/replay | Sequential scratch retain MATCH | **Done (experiment)** |
| A2g-Natural | Live cross-frame EFB sample discovery | Trace-only; 0 natural next/multi (title/menu) | **Done (report)** |
| A2g-StageCoverage | Isolated NAND + TR1 natural search | TR1 entered; 0 natural next/multi in gameplay | **Done (report)** |
| A3 | Interpolated second XFB without guest update | `unique_xfb` >60 behind debug flag | Later |
| A4 | Quality gate; promote to uncapped | Honest unique>60 in production | Later |

### Approach A1c — scene-wide GX transform capture (2026-11-21)

Instrumentation only: observe the position matrices the GX pipeline actually uses, plus guest body world-mtx refreshes. **No interpolation, FIFO replay, or extra XFBs.**

| Item | Detail |
|------|--------|
| Code | [`runtime/debug/a1c_gx_capture.cpp`](../runtime/debug/a1c_gx_capture.cpp) |
| Enable | `SCR_A1C_GX_CAPTURE=1`; `SCR_A1C_GX_DUMP=1` for periodic logs |
| Bounds | ≤128 pos-mtx loads stored/frame; 10 PNMTX slots tracked; ≤48 body EAs |

#### Player transform investigation (`fn_80053718`)

| Fact | Evidence |
|------|----------|
| Role | `BodyGetWorldMtx(body)`: if `body+0x2DD8` bit0 set → `fn_800533E8` composes `+0xA4`/`+0xB0` into `+0x0C`, clears bit0; **returns `body+0x0C`** (`text_80046CB8.c`) |
| Timing | Called by many systems when they need a world mtx; after guest returns, `+0x0C` is authoritative |
| Why A1b saw T≈0 | `PlayerGetWorldMtxPtr` returns `+0x0C` **without** refresh; even after natural `BodyGetWorldMtx`, **translation at `+0x0C` often stays 0** while rotation/hash changes |
| Position space | `PlayerGetPos` / `body+0x168` is a separate Vec3 (not the MTX translation) |
| Capture rule | Hook `BodyGetWorldMtx` **after** `PPC_ORIGINAL` — never call `fn_80053718`/`fn_800533E8` ourselves |

**Layers:** shell → body; `+0x168` position; `+0x0C` body world mtx (orientation-heavy); GX PNMTX loads = **rendered** modelview (full T). Do not treat these as one interchangeable matrix.

#### GX position-matrix pipeline

```
Guest GXLoadPosMtxImm / indexed XF
  → FIFO opcode 0x10 LoadXF  / 0x20–0x38 LoadIndexedXF
  → gx_fifo LoadXF / LoadIndexedXF → g_gx.xfmem[addr..]
  → Render_OnXF(addr, count, indexed)
       → A1c_OnXfLoad (slots addr/12 for addr<120)
  → draws use CP_MATINDEX_A / per-vertex pos mtx index
```

- Slots **GX_PNMTX0..9** = XF words `0..119` (12 floats each).
- First slot-0 load ≠ always camera (menus often identity / 2D; A1b still snapshots first slot-0 separately).
- Slot index is a **draw-time register**, not a stable object id.

#### Scene coverage (TR1, `userdata` + input script)

| Category | Classification |
|----------|----------------|
| Sonic body via `BodyGetWorldMtx` | **Captured and identifiable** (stable `bodyEa`; hash moves across frames) |
| Sonic `+0x0C` translation | Often 0 — **not** the rendered world T; use `+0x168` / GX |
| Sonic skeletal bones | **Requires separate instrumentation** (not this accessor) |
| Enemies / other actors (guest body API) | **Sparse / absent** in TR1 window (often only player EA) — **needs more guest hooks** |
| Dynamic scene geometry (GX slots 1–9) | **Captured but correspondence unresolved** (slot hashes change every frame; T moves with gameplay) |
| Moving env / platforms | Appear as GX slot traffic — **not EA-tagged** |
| Particles / transparency / HUD | **Unsuitable** for object-transform interp (slot0 UI/2D noise; particles not body mtx) |

#### Architecture decision (A1c → A2)

1. **Cross-frame object identity via slot index alone: no.** Slots are reused; hashes churn even when T is similar.
2. **Skeletal via same BodyGetWorldMtx path: no.** Need pose-accumulator / bone hooks.
3. **Camera vs object:** A1b view/first-slot0 + A1c slot change masks help; not a hard classifier yet.
4. **Missing dynamics:** Most non-player actors never hit `BodyGetWorldMtx` in the measured TR1 path; GX still sees their draws.
5. **FIFO archive must preserve:** raw XF load bytes (addr/count/payload), indexed XF source addresses, CP mat-index, DL nesting, and preferably a guest-side object tag when available.
6. **Minimum before A2:** optional FIFO ring of XF+draw ops between XFBs (debug flag); PE IRQ suppression plan; do **not** rely on PNMTX slot as object id.

**Verdict:** GX matrix capture is a **viable observation layer** for proving live scene motion and for designing archive contents. It is **not** by itself a complete object-stable pose stream for scene-wide interpolation. Proceed to **A2b FIFO archive** next (A2 warp can stay later).

### Approach A2b — bounded GX FIFO archive (2026-11-21)

Capture/validate the GPU command stream **before** `ProcessFifo` consumes it. **No replay, no guest redraw, no second XFB, no unique-FPS claim.**

| Item | Detail |
|------|--------|
| Code | [`runtime/debug/a2b_fifo_archive.cpp`](../runtime/debug/a2b_fifo_archive.cpp) |
| Enable | `SCR_A2B_FIFO_CAPTURE=1` (default off) |
| Dump | `SCR_A2B_FIFO_DUMP=1` (periodic logs + curr/prev delta); `SCR_A2B_FIFO_DUMP_DIR=<path>` for `.bin`/`.txt`/ `_res.bin` exports |
| Bounds | ≤4 MiB cmd/frame; ≤512 coalesced subs; ≤256 KiB/sub; ≤96 resources; ≤1 MiB resource copies (≤64 KiB each) |

#### FIFO lifecycle (actual)

```
Guest GX* → GPFifo_Write* → Gather
  ├─ RecordingToRAM (PI≠CP): write guest DL buffer; not archived as FIFO
  └─ else: 32 MiB ring (GPU thread) or direct s_fifo (single-core)
       → GpuThread copies ring → s_fifo
       → A2b_OnFifoSubmission(new bytes)   ← archive point (before erase)
       → ProcessFifo → RunCommands → erase consumed prefix of s_fifo
CallDisplayList(addr,size) → A2b_OnDisplayList (copy DL bytes)
LoadIndexedXF → A2b_OnIndexedXf (copy indexed XF payload)
Perf_OnXfbCopy → finalize frame (GX snap + A1b/A1c context + CP inventory)
```

| Property | Behaviour |
|----------|-----------|
| Wrap | Guest PI FIFO wrap is emulated for SDK progress; host ring is linear modulo `RING_SIZE` |
| Partial cmds | `ProcessFifo` may leave incomplete trailing bytes in `s_fifo`; next drain appends — archive preserves drain order |
| Multi-sub/frame | Many ring drains per XFB; A2b **coalesces** consecutive drains into ≤256 KiB submissions (exact bytes, order preserved) |
| External refs | DLs + indexed XF copied when sized; CP array bases inventoried (extent unknown → unresolved); TMEM/EFB marked unresolved |

Overflow never silently truncates a submission: whole chunk is skipped and `A2bFrame_Incomplete` is set (`skippedSubs` / `skippedBytes`).

#### Archive contents

| Layer | Captured | Gap |
|-------|----------|-----|
| Command bytes | Yes (pre-erase), hashed per sub + whole frame | None for frames under 4 MiB |
| Submission order | Yes (coalesced drain groups) | Not 1:1 with each 32 B gather publish |
| XF inline loads | Inside cmd bytes; A1c correlation via `SnapContext` | Full XF mem beyond PNMTX0–9 only hashed at XFB |
| Indexed XF / DL RAM | Copied when under caps | Large DLs truncated (`A2bRes_Truncated`) |
| Vertex/index arrays | CP base+stride inventory only | Contents **not** copied (size unknown) |
| Textures / TMEM | Placeholder unresolved | Must snapshot or bind-track for replay |
| EFB / BP copy | BP sample + unresolved marker | Framebuffer contents not archived |
| Guest tags | A1b player/cam + A1c load/body counts when those flags on | Matrix slot ≠ object id (unchanged) |

#### Validation (TR1 + boot)

| Check | Result (`out/phase18_a2b_*.log`) |
|-------|----------------------------------|
| Capture off silent | No `A2b:` lines |
| Capture on | Dumps + optional `out/a2b_dumps*` exports; bin size == `totalBytes` |
| sim / unique_xfb | ≈60 with capture on/off; underruns ≈0 in-stage |
| Busy frames | Resource cap (96) → `Incomplete` common; **cmd bytes complete** (no skip after coalesce) |
| Context | `a1b=yes` / `ply=yes` / high `a1cLoads` when A1b/A1c also enabled |

#### Readiness for replay (A2c) — not claimed safe

1. Command stream: **complete** under 4 MiB (typical TR1 ~10–300 KiB).
2. External resources: **partial** — DLs/indexed XF often copied; vertex arrays/textures/EFB **unresolved**.
3. Lifetimes: frame-local; RAM may change before a deferred pass.
4. GX state: end-of-frame snap + bytes in stream; mid-frame BP/CP evolution is in the stream only.
5. Boundaries: coalesced subs reconstructable; PE finish / XFB still via BP opcodes in stream.
6. Non-repeatable ops: EFB copies, PE tokens/IRQs, clears, texture invalidates — need isolation.
7. Side effects: naive replay would re-raise PE IRQs and rewrite EFB/XFB paths.
8. Isolation needed: shadow EFB, suppress PE IRQs, resolve/copy vertex+texture deps, patch matrices from A1b/A1c.
9. Smallest safe experiment: replay **one** archived UI/static DL into a scratch EFB with IRQs off — not production.

**Verdict:** A2b archive is **useful groundwork** for a future render pass (ordered cmd bytes + DL/XF copies + guest correlation). It is **not** sufficient alone for safe FIFO reuse. Next: **A2c** resource-complete replay spike behind a separate flag.

### Approach A2c — resource closure + scratch-EFB replay (2026-11-21)

Experimental milestone: close dependencies for a **selected title/boot pass**, then replay into an isolated scratch EFB. **No second game XFB, no interpolation, no unique-FPS claim.**

| Item | Detail |
|------|--------|
| Code | [`runtime/debug/a2c_scratch_replay.cpp`](../runtime/debug/a2c_scratch_replay.cpp) (+ A2b resource extensions) |
| Capture | Same `SCR_A2B_FIFO_CAPTURE=1` (closure hooks active with A2b) |
| Replay | `SCR_A2C_SCRATCH_REPLAY=1` (default off); requires replay-ready gate |
| Selected pass | Title/boot idle frames ≈5.5 KiB cmds, ~70 draws, 1 DL (64 B) + 1 texture (~150 KiB) |

#### Why this pass

- Clear submission boundary: coalesced frame cmds up to first **toXFB** BP `0x52`.
- Manageable size; no player/enemy motion dependency.
- Reproducible on boot without TR1 input.
- Still not assumed fully self-contained for pixel parity (see DIFF).

#### Resource closure (vs A2b)

| Resource | A2c behaviour |
|----------|---------------|
| Vertex/index arrays | Draw-time index walk → sized `A2bRes_VertexArray` copies |
| Textures | `GetTexture` snapshots source RAM (`A2bRes_Texture`) |
| TLUT / TMEM | CI paths snapshot TMEM window + optional TLUT RAM |
| Display lists / indexed XF | Required + copied (deduped, refcounted) |
| Unique capacity | 256 resources / 4 MiB res bytes / 256 KiB per copy |
| Placeholders | Removed bogus unresolved CP×16 / empty TMEM markers |

#### Replay-readiness gate

Per archived frame: `A2cPassReport` with bitflags for cmd/subs/gxStart/vertex/texture/DL/overflow/selection. **`replayReady` only if every required dependency is fully copied** and the pass is selected. Incomplete archives are never replayed.

#### Isolation

- Separate scratch color/depth RTVs; `SetEFBTarget` redirects while `Render_SetReplayIsolation`.
- `GX_SetPeSuppress` blocks PE finish/token IRQs.
- `DoEFBCopy` toXFB returns immediately under isolation (no Present / no `Perf_OnXfbCopy`).
- Guest RAM regions for copied resources saved/restored; full `g_gx` + TMEM + tev konst restored.
- Texture cache cleared for replay so loads use restored RAM.

#### Validation (`out/phase19_a2c_*.log`)

| Check | Result |
|-------|--------|
| Capture off | No `A2c` / `A2b` lines |
| Ready title frames | `ready=yes`, `reqUnres=0`, texture copied |
| Scratch replay | `det=1` (two identical scratch hashes), `drew=1` (≠ cleared black) |
| Live vs scratch fingerprint | **DIFF** (full-EFB hash) — not pixel-parity yet |
| Sim / unique_xfb | ≈60 with replay; underruns ≈0 after settle |
| Live XFB path | Unchanged; replay never presents |

**Pixel MATCH blocker:** scratch starts cleared; live EFB may retain prior content / host blend-sampler residual; gxStart is post-previous-XFB but not a perfect world-state clone. Treat DIFF as expected until A2d.

#### Architecture next (A2d / A3)

1. Selected title pass is **resource-closed and isolatable**, not proven pixel-identical.
2. Full TR1 scenes still exceed practical unique-resource budgets / readiness.
3. Hardest leftovers: EFB-as-texture chains, mid-frame copies, residual host cache state.
4. GX isolation is workable for an experiment; not yet a general second pass.
5. Recommend **A2d**: EFB start snapshot + host-state dirtying + region fingerprint until MATCH; only then consider matrix-patched replay toward A3.

**Verdict:** A2c **succeeds as an isolation/closure experiment** (deterministic non-empty scratch from archive-only deps). It does **not** prove scene-wide replay or pixel-identical reconstruction.

### Approach A2d — EFB initial-state + host-state closure (2026-11-21)

Close the A2c DIFF by seeding scratch from the live pass-entry EFB and comparing at a matched boundary. **Still opt-in, still no second XFB / no interpolation / no unique-FPS claim.**

| Item | Detail |
|------|--------|
| Code | `renderer.cpp` EFB slots + compare; `a2b_fifo_archive.cpp` `A2b_OnPassEntryEfb`; `a2c_scratch_replay.cpp` |
| Enable | `SCR_A2B_FIFO_CAPTURE=1` + `SCR_A2C_SCRATCH_REPLAY=1` (default off) |
| Selected pass | Same title/boot idle (~5504 B, ~70 draws, 1 DL + 1 tex) |

#### Comparison boundary

| Point | Definition |
|-------|------------|
| Pass commands | `[0, passEnd)` — first **toXFB** BP `0x52` is the terminator (**excluded**) |
| Mid-pass EFB copies | Non-toXFB `0x52` remain **inside** the pass (scanner skips only toXFB) |
| Pass entry (incoming EFB) | After post-XFB `ClearEFB` (or immediately after toXFB when clear bit off) via `A2b_OnPassEntryEfb` |
| Pass exit (live reference) | EFB after archived draws, sampled inside `A2b_OnXfbCopy` / `A2c_OnArchivedFrame` **before** post-XFB clear |
| Scratch exit | After `GX_ReplayCommands(passBytes)` into scratch RTVs seeded from incoming |

Do not compare a cleared scratch against a carry-in live EFB, or a post-clear live sample against a pre-clear scratch result.

#### Incoming EFB snapshot

- Lossless GPU `CopyResource` of live colour (`R8G8B8A8_UNORM`) + depth (`R32_TYPELESS` / `D32_FLOAT` DSV) into refcounted slots (independent of live EFB).
- Records dimensions, colour/depth hashes (`everyPixel` mode), validity.
- Scratch seeds via `CopyResource` into scratch colour/depth — **no clear after seed**.
- Bounded slots with steal-on-overflow; payloads stay off the 4 MiB command archive.

#### Host graphics-state coverage (D3D11 backend)

| State | Source on replay |
|-------|------------------|
| RTV/DSV | Scratch targets while isolation on; restored to live after |
| Viewport / scissor / cull | Derived each `Flush` from `g_gx` XF/BP |
| Blend / depth-stencil / raster | Built from BP; object cache keyed by register values |
| Shaders / input layout | From VAT/TEV keys + vertex data |
| XF constants | `s_xfDirty` forced; uploaded from restored `g_gx.xfmem` |
| Textures / samplers | Replay drops tex + EFB-copy caches; rebuild from restored guest RAM |
| PE IRQs | Suppressed for the replay window |
| Sticky host leftovers | Avoided by `gxStart` restore + `Render_ResetHostDrawState` + cache drop |

#### Completeness dimensions

| Dimension | Title/boot pass |
|-----------|-----------------|
| Command completeness | Yes (`reqUnres=0`, selected size) |
| Resource completeness | Yes (DL + texture copied) |
| Initial-state completeness | Yes (incoming colour+depth seeded) |
| Host-state completeness | Yes for this backend path (state realised from `g_gx` + archives) |
| Replay determinism | Yes (`det=1`, two identical scratch colour hashes) |
| Live-vs-scratch pixel equivalence | **MATCH** (colour + depth exact; `colorMis=0`, `depthMis=0`) |

#### EFB feedback (this pass only)

- Pass **reads** pre-pass EFB pixels (incoming seed required — proven by A2c DIFF without seed).
- No evidence of EFB-as-texture / prior-copy dependency beyond archived RAM texture: ready with `deps=2`, and MATCH without extra EFB-copy capture.
- Depth is preserved through the pass (`inD == liveD == scrD` on idle frames); colour is rewritten by draws.
- General mid-frame EFB feedback for gameplay scenes remains **unsolved** and out of scope.

#### Validation (`out/phase20_a2d_*.log`)

| Check | Result |
|-------|--------|
| Capture off | No A2b/A2c/A2d lines; sim/unique_xfb ≈60 |
| Ready + seed | `efbIn=yes`, `seed=1`, `flags=1ff` |
| Pixel compare | `dCmp=1 MATCH` — `liveC==scrC`, `liveD==scrD` |
| Isolation | PE suppress; scratch-only draws; `g_gx`/TMEM/tev/guest RAM restored |
| Audio | Underruns ≈0 after settle |
| `regression_smoke` | OK |

#### Next milestone (after A2d)

Answered by **A2e** below (second independent family MATCH).

**Verdict:** A2d **achieves pixel-identical scratch replay** for the selected title/boot pass under equivalent initial conditions. Not a licence for scene-wide replay or unique-FPS claims.

### Approach A2e — second independent static/UI pass (2026-11-21)

Prove the A2c/A2d archive→seed→scratch path is not a one-pass special case. Same infrastructure; structural family classification only (no hard-coded EAs).

| Item | Detail |
|------|--------|
| Code | `a2c_scratch_replay.*` family classify + per-family results; shared A2b/A2d path |
| Enable | `SCR_A2B_FIFO_CAPTURE=1` + `SCR_A2C_SCRATCH_REPLAY=1` (default off) |
| Boundary | Identical to A2d: `[0, first toXFB 0x52)`; incoming after post-XFB clear; live at `OnXfbCopy` |

#### Why the second pass is independent

| | A2d-idle | A2e-multitex |
|--|----------|--------------|
| Role | Title/boot idle HUD | Richer static/UI layer (multi-texture) |
| Cmd bytes | ~5.4–5.8 KiB | ~12.1–12.5 KiB |
| Draws | ~70–71 | ~80–82 |
| Required deps | 1–3 (typically 1 DL + 1 tex) | 8–10 (**all textures**) |
| Classification | bytes 4–7 KiB, draws 60–75, deps 1–3 | bytes 10–16 KiB, draws 75–100, deps ≥6 |

Not the same pass at another frame: different size, draw count, and dependency mix.

#### Comparison table (`out/phase21_a2e_boot.log`)

| Family | passBytes | draws | deps | efbIn | seed | det | colorMis | depthMis | dCmp | Result |
|--------|----------:|------:|-----:|:----:|:---:|:---:|---------:|---------:|:----:|:------:|
| A2d-idle | 5387–5595 | 70–71 | 1–2 | yes | 1/1 | 1 | 0 | 0 | 1 | **MATCH** |
| A2e-multitex | 12100–12480 | 80–82 | 8–10 | yes | 1/1 | 1 | 0 | 0 | 1 | **MATCH** |

`liveC==scrC`, `liveD==scrD`, `reqUnres=0` on replayed ready frames. A2d still MATCH after A2e runs (ordering/isolation).

#### Generality answers

1. **Generic now:** FIFO archive, DL/tex/vert/TMEM deps, incoming EFB colour+depth, scratch seed, host state from `g_gx`, PE suppress, restore, everyPixel MATCH gate, family classify.
2. **Pass-specific:** Only structural family thresholds for scheduling/logging — not addresses or rendering hacks.
3. **Multi-pass sequence:** Yes — per-family reports/replays; A2d then A2e without state leak.
4. **EFB initial-state:** Consistent (`seed=1/1` both families).
5. **Host-state determinism:** Holds across idle HUD and multi-tex UI workloads.
6. **Third static/UI pass:** Needs another distinct profile (e.g. pause) still without actors/EFB-feedback; classification can grow.
7. **Gameplay complications:** Moving actors, skeletal/matrix interp, EFB-as-texture chains, resource-budget overflow, PE timing.
8. **Next milestone:** Prefer a **controlled EFB-feedback** experiment (non-toXFB copy → sample) over broad UI spray — still no scene-wide / no interpolation.

#### Isolation notes

- Capture/replay **off**: silent; sim/unique_xfb ≈60 (`out/phase21_a2e_off.log`).
- Capture/replay **on**: full EFB CPU compare on the GPU thread temporarily lowers uncapped sim Hz (~33–36 while actively replaying); after both families MATCH, replay backs off (10s). Not a guest-timing change — diagnostic cost only.
- `regression_smoke` OK. Windows: source compiles via existing mingw preset (no native GPU A2e run in this phase).

**Verdict:** Two genuinely different static/UI passes independently MATCH on the same archive/replay architecture. **Not** arbitrary GX replay; **not** unique-FPS.

### Approach A2f — controlled EFB-feedback capture/replay (2026-11-21)

Extend A2d/A2e so mid-pass **EFB→texture** feedback is archived, reconstructed on scratch, and still MATCHES live.

| Item | Detail |
|------|--------|
| Selected pass | Title/boot idle HUD **mip-chain** (same ~5.5 KiB / ~70 draws as A2d) with **5 sampled EFB copies** |
| Evidence | `out/shots/capture.txt`: `ctl=010063` copies 640×480→160×120→80×60→40×30→20×15 then sample |
| Enable | `SCR_A2B_FIFO_CAPTURE=1` + `SCR_A2C_SCRATCH_REPLAY=1`; `SCR_A2C_COMPARE_SYNC=1` forces sync everyPixel |

#### Dependency chain

1. Producer draws into EFB (scratch under isolation).
2. Non-toXFB BP `0x52` copies regions into host `s_efbCopies[dest]` (**from scratch EFB when isolated** — was incorrectly reading live before A2f).
3. `GetTexture` hits those dests → `A2b_OnEfbCopySampled`.
4. Consumer draws sample the scratch-produced copies.
5. Pass ends at first toXFB `0x52` (excluded).

Archive: `A2bRes_EfbCopy` metadata (dest, region, fmt, ctl, Sampled). Copy **result** is reproduced by replaying the BP ops into scratch, not by substituting live textures.

#### Gate / family

- `A2cReady_EfbFeedback`: no copies, or ≥1 sampled with metadata-complete copies.
- Family `A2f-efbfb`: idle-scale + `efbC sampled≥1`.
- Flags `3ff` when fully ready.

#### Async compare (Phase 1)

- `Render_CompareBeginAsync` / `Poll`: staging CopyResource + event query; Map/CPU walk deferred.
- Status: pending / completed / skipped / failed — **skipped ≠ MATCH**.
- First N attempts + mismatch + `SCR_A2C_COMPARE_SYNC=1` force sync everyPixel.
- Bounded queue (2 slots).

#### Results (`out/phase22_a2f_*.log`)

| Check | Result |
|-------|--------|
| A2f-efbfb | `efbC=5/5`, `seed=1/1`, `det=1`, `colorMis=0`, `depthMis=0`, `dCmp=1` **MATCH** |
| A2e-multitex under A2f | `pass=12112`, `efbC=5/5`, same everyPixel **MATCH** (`phase22_a2f_long.log`) |
| Scratch-sourced copies | Proven: MATCH after DoEFBCopy reads scratch under isolation |
| Capture off | Silent; sim/unique_xfb ≈60 (`phase22_a2f_off.log`) |
| Capture+replay async | ~35–37 Hz sim while validating; underruns≈0 after settle (`phase22_a2f_async.log`) |
| Capture+replay sync | ~33–37 Hz sim (`phase22_a2f_long.log`); async does not fully remove capture/replay cost |
| Queue | `q=0/0` in MATCH lines; skipped ≠ MATCH |
| `regression_smoke` | OK |
| Windows GPU A2f | MinGW links (`out-windows/SonicColorsRecomp.exe`); native GPU replay unverified |

#### Evaluation answers

1. EFB copy from archived inputs? **Yes** — via scratch EFB + archived BP copy cmds.
2. Consumer samples correctly? **Yes** (`efbC=5/5` sampled + MATCH).
3. Producer→copy→consumer generic? **Yes** (metadata + Sampled flag; no hard-coded EAs).
4. Pass-specific addresses? **No** — structural family only.
5. Unsupported modes: exotic depth-copy consumers, EFB copy as persistent cross-frame texture without in-pass sample, multi-frame feedback.
6. Depth copies: infrastructure flags `DepthSrc`; this pass uses colour mip path (`pf=0`).
7. Multiple copies/pass: **Yes** (5 in the mip chain).
8. Gameplay blockers: actors/matrices, cross-frame EFB textures, resource caps, PE timing.
9. **Next:** third static/UI **or** smallest cross-frame EFB-texture lifetime test — still no scene-wide / no interp.

**Verdict:** A2f proves isolated scratch **generates and samples its own EFB-derived textures** with exact live MATCH on the title mip-chain pass.

### Approach A2g — cross-frame EFB texture persistence (2026-11-21)

Prove scratch can **retain** EFB→tex generations produced in guest/present frame N and **sample them** in frame N+1 without live fallback.

| Item | Detail |
|------|--------|
| Lifetime audit | Host `s_efbCopiesLive` persists across presents (age/cap eviction only); GetTexture reuses when RAM hash matches |
| Live observation | Title/menu: **0** cross-frame samples in >200k EFB hits — game always re-copies in the same present before sampling |
| Controlled test | Consecutive A2f idle passes; producer replay fills `s_efbCopiesScratch`; consumer begins with **retain**; consumer non-toXFB copies are no-ops under isolation (`Render_SetScratchSkipEfbTexCopy`) so draws sample retained gens |
| Identity | `genId` + canonical dest key; multi-frame `A2b_ClonePreviousPacket` + `A2bCrossEfbEdge` |
| Gate | `A2cReady_CrossFrame`; family `A2g-cross`; sequence ready only when every edge resolves to a producer copy |

#### Results (`out/phase23_a2g_selftest2.log`)

| Check | Result |
|-------|--------|
| A2g-cross | `edges=5/5`, `copyH=5/5`, `retain=1`, `self=1`, `colorMis=0`, `depthMis=0`, `dCmp=1`, `liveC==scrC` **MATCH** (e.g. xfb `4←3`) |
| A2f / A2e under A2g | Still **MATCH** |
| Natural live cross-sample | Not observed on title/menu (blocker for non-selftest path) |
| Capture off | ~60 Hz (unchanged) |
| Capture+replay on | Still ~33–37 Hz (workload cost; async compare alone insufficient) |

#### Evaluation answers

1. Host EFB copies survive frame boundaries? **Yes** (map persistence); title **replaces** before sample.
2. Archive lifecycle? **Yes** — genId, createXfb, edges, prev/curr packets.
3. Multiple generations? **Yes** — gen counter; same dest ≠ same gen.
4. Longer history? Ring is prev+curr; extend later.
5. Persistent effects reconstructible? **Scratch retain proven**; natural live cross-sample still needed for non-selftest.
6. Unsupported: depth-copy consumers, true multi-frame history buffers, gameplay actors.
7. Gameplay: need a scene that samples an unrecreated prior-frame EFB tex.
8. **Next:** find natural live cross-frame sample **or** longer history chain — still no scene-wide / no interp.

**Verdict:** A2g proves scratch **independently generates, retains across a frame boundary, and samples** EFB-derived textures with exact MATCH (controlled dual-frame idle selftest).

### Approach A2g-Natural — live cross-frame discovery (2026-11-21)

Opt-in `SCR_A2G_NATURAL_TRACE=1` observes unmodified live EFB→tex create / replace / GetTexture-bind (draw sample) / invalidate. **Does not** suppress copies or force retention. Distinct from `SCR_A2G_SELFTEST` synthetic retain.

| Classification | Meaning |
|----------------|---------|
| Same-frame | Sample present == create present |
| Next-XFB | First sample on the next present |
| Multi-XFB | First sample ≥2 presents later |
| Replacement | New gen at same canonical dest |

**Sampling event:** `GetTexture` returning a live `s_efbCopiesLive` SRV for a draw bind (not a bare cache poke).

#### Scenes / evidence (`out/phase24_nat_*.log`)

| Scene | Presents | Creates | Samples (binds) | Same-frame gens | Next-XFB | Multi-XFB | CROSS lines |
|-------|--------:|--------:|----------------:|----------------:|---------:|----------:|------------:|
| Title idle (~35s) | ~1600 | ~24k | ~110k | all | **0** | **0** | **0** |
| TAS title→menu mash (~150s) | ~8600 | ~129k | ~593k | all | **0** | **0** | **0** |

Final TAS snapshot: `presents≈8600`, `creates≈129058`, `samples≈593331`, `sameFrame≈111787`, `nextXfb=0`, `multiXfb=0`, `replacements≈129041`, `crossEdges=0`. Trace overhead: sim stays ~60 Hz. Capture off remains silent.

**Natural producer/consumer:** **none found** on inspected title/menu paths. Host copies persist in the map but are **replaced every present before any later-frame sample**. Title/menu alone is **not** evidence about gameplay.

**Synthetic selftest** (`SCR_A2G_SELFTEST=1`, default with scratch replay): still `edges=5/5 retain=1 MATCH` — labeled synthetic, not natural.

### Approach A2g-StageCoverage — TR1 natural search (2026-11-21)

Isolated disposable NAND + verified save fixture + deterministic TR1 entry; then natural trace on real gameplay. See `docs/NAND_SAVE_AUDIT.md`.

| Piece | Detail |
|-------|--------|
| NAND isolation | `--nand out/test_nand` / `SCR_TEST_NAND_DIR`; boot log `ISOLATED test override` |
| Fixture | `tools/setup_a2g_test_nand.sh` copies verified `userdata/…/data` → `out/test_nand/` (gitignored); **no** invented save flags |
| Stage entry | `tools/a2g_tr1_enter.txt` (phase8-proven timing); marker `A2g-Stage: entered 'stg110'` + `[MUSIC] stage context 'stg110'` |
| Trace | `SCR_A2G_NATURAL_TRACE=1` `SCR_A2G_SELFTEST=0` — no forced retention / no copy suppression |

#### TR1 evidence (`out/phase25_tr1_nat_full.log`)

| Coverage | Result |
|----------|--------|
| Stage entry | **Yes** — `stg110` via normal menus + input script |
| Activities | Walk/run, jump, boost (B), camera RX, pause/resume, sustained movement |
| Presents (full run) | ~8000 (~2000 menu + ~6000 in/near TR1) |
| Creates / samples | ~130k / ~564k |
| Same-frame gens | ~114k |
| Next-XFB / multi-XFB | **0 / 0** |
| CROSS edges | **0** |
| Longest life (XFB) | **0** (replaced before later-frame sample) |
| Live `efbCopies` | ~6 concurrent (replaced each present) |
| Userdata SHA | Unchanged before/after test NAND runs |

**Natural producer/consumer on TR1:** **none found.** Generations are created and sampled in the same present, then replaced. Synthetic multi-frame history stress was **not** added — A2g retain selftest already covers cross-frame MATCH; natural TR1 shows no lifetime window that would need a longer synthetic chain.

**Reproduce:**

```bash
./tools/setup_a2g_test_nand.sh
SCR_A2G_NATURAL_TRACE=1 SCR_A2G_SELFTEST=0 ./out/SonicColorsRecomp \
  --data game/SNCP8P --skip-launcher --nobox --mute --perf --scale 3 --60fps \
  --nand out/test_nand --input-script tools/a2g_tr1_enter.txt --music-debug
```

**Next:** Other stages / effects-heavy scenes if TR1’s mip-chain-dominated EFB pattern is not representative — still no scene-wide / no interp / no unique-FPS. Do **not** treat synthetic retain as natural.

### Audio

`PaceUncappedHost` pumps `Sched_RunDue` / `ppc_check_irq` like VI spins so AI DMA/AX stay fed. Measured boot-path uncapped underruns/repeats stay near zero and are not worse than `--60fps` on the same host (see `docs/PROGRESS.md` 2026-11-21).

## Spring soft-lock (TR1 WideSpring) — FIXED

See `runtime/debug/spring_60.cpp`.

## Aquarium Park Act 1 spiral (`stg510`) — DEFERRED

Do not investigate or re-introduce spiral gates. Prefer other PC / 60 FPS work.

## Broader 60 FPS compatibility

[`docs/FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md`](FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md).
