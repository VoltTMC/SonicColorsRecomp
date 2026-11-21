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

### Approach A vs B (genuine unique frames)

#### Approach A — Decoupled simulation and rendering (recommended long-term)

Keep sim at 60 Hz; emit additional XFBs between steps from a host-owned path that does **not** advance gameplay.

| Factor | Assessment |
|--------|------------|
| Unique frames? | Yes, if a second render path emits new XFBs without guest update |
| Complexity | High — no clean guest render-only API; FIFO not retained; matrices inside consumed XF/BP traffic |
| Gameplay risk | Low for sim if sim stays 60 Hz; medium for visual correctness |
| GX | Must not re-run guest draw that mutates anim/particles/one-shots |
| Audio | Keep IRQ/pace pumps; avoid Present(1) storms on the guest thread |

Naive variants rejected: blind guest draw replay (side effects); FIFO replay as-is (identical image unless matrices patched).

#### Approach B — Higher-frequency guest sim+render (rejected for >60)

Raising `FrameLoopWait` / VI-equivalent rate looks small but is unsafe: many systems advance **per iteration**, not `* dt`. Even wait=1 (60 Hz) has known/potential bugs (see compatibility audit). 120 Hz doubles that pressure vs 60.

**Rejected** for uncapped unique-frame goals.

### Approach A roadmap

| Phase | Goal | Exit criteria |
|-------|------|----------------|
| A0 (current) | Honest metrics + docs | No claim of unique>60 |
| A1 | Capture stable camera + player world matrices each sim step | Debug overlay shows poses; zero gameplay change |
| A2 | Debug-only host warp/composite from two captured poses | Visual check; **not** counted as `unique_xfb` |
| A3 | Safe second XFB without guest update | `unique_xfb` can exceed 60 behind a debug flag |
| A4 | Quality gate (physics/audio/TR1 timing); promote to uncapped | Honest unique>60 in production uncapped |

### Audio

`PaceUncappedHost` pumps `Sched_RunDue` / `ppc_check_irq` like VI spins so AI DMA/AX stay fed. Measured boot-path uncapped underruns/repeats stay near zero and are not worse than `--60fps` on the same host (see `docs/PROGRESS.md` 2026-11-21).

## Spring soft-lock (TR1 WideSpring) — FIXED

See `runtime/debug/spring_60.cpp`.

## Aquarium Park Act 1 spiral (`stg510`) — DEFERRED

Do not investigate or re-introduce spiral gates. Prefer other PC / 60 FPS work.

## Broader 60 FPS compatibility

[`docs/FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md`](FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md).
