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
| A2 | Debug-only host warp/composite from two captures | Visual check; **not** counted as `unique_xfb` | Later |
| A2b | Optional FIFO archive (flag) + IRQ-safe replay spike | Can re-issue last frame’s draws into EFB under flag | Later |
| A3 | Interpolated second XFB without guest update | `unique_xfb` >60 behind debug flag | Later |
| A4 | Quality gate; promote to uncapped | Honest unique>60 in production | Later |

### Audio

`PaceUncappedHost` pumps `Sched_RunDue` / `ppc_check_irq` like VI spins so AI DMA/AX stay fed. Measured boot-path uncapped underruns/repeats stay near zero and are not worse than `--60fps` on the same host (see `docs/PROGRESS.md` 2026-11-21).

## Spring soft-lock (TR1 WideSpring) — FIXED

See `runtime/debug/spring_60.cpp`.

## Aquarium Park Act 1 spiral (`stg510`) — DEFERRED

Do not investigate or re-introduce spiral gates. Prefer other PC / 60 FPS work.

## Broader 60 FPS compatibility

[`docs/FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md`](FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md).
