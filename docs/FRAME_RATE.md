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
| `unique_xfb` | Unique guest XFB copies (new game images available to blit) |
| `present_calls` | Host `IDXGISwapChain::Present` invocations (may re-show the **same** XFB) |
| `guest_vi_hz` | Emulated Wii VI field rate (~60 Hz NTSC) |
| `display_refresh_hz` | Physical monitor mode from the OS (`Window_DisplayRefreshHz`) |
| `vsync` | `Present(1)` vs `Present(0[, ALLOW_TEARING])` |

**`present_calls` is not unique game FPS.** On a 120 Hz panel with 60 Hz `unique_xfb`, ~120 `present_calls` means each image may occupy two refreshes — not 120 distinct rendered game frames.

Actual panel refreshes are not counted separately (compositor feedback is unavailable / unreliable across Wayland + DXGI/vkd3d). We report `display_refresh_hz` from the OS mode instead of inventing a “displayed FPS.”

## Semantics of `lbl_808F2D98` (SDA float dt)

From the SNCP8P decomp (`fn_803E19B0` / `fn_800115BC`):

- Integer wait fields `N` are stored at SDA `0x808F2D94`.
- Float `dt = N / 60` (seconds per guest step) at `0x808F2D98`.
- Many guest systems still advance **once per loop iteration**; wall-clock dt alone does not make the engine frame-rate independent.

## Uncapped mode (`--frame-rate 0`)

### Architecture

```
each FrameLoopWait (uncapped):
  publish N=1, dt=1/60
  skip VI EQ / flush / retrace waits
  PaceUncappedHost:
    until next 60 Hz sim deadline:
      pump Sched_RunDue + ppc_check_irq   # audio
      Present latest XFB at display_refresh_hz (or host_fps_cap)
```

| Setting | Present cadence |
|---------|-----------------|
| VSync on | One `Present(1)` per unique XFB (synced to **host** refresh edges). Pace loop pumps audio only — no re-Present(1) (would starve AI/AX). Compositor holds the image across refreshes (e.g. 60 Hz content on 120 Hz). |
| VSync off | `Present(0)` + tearing if supported; latest-frame re-Present paced to ≈ display refresh |
| `host_fps_cap` > 0 | Cap `present_calls` when VSync is off (still not unique_xfb) |

Default uncapped no longer spam-Presents at ~1000 Hz. That inflated `present_calls` without new game images and burned CPU/GPU.

With VSync on a 120 Hz panel: expect `present_calls` ≈ `unique_xfb` ≈ 60, `display_refresh_hz` = 120, Present duration ≈ one host refresh (~8 ms), not a hidden guest-VI 16.7 ms cap.

### Can we get genuine 120 unique game frames/s?

**Not with the current XFB architecture**, without a major redesign:

- Guest GX finishes into an XFB once per simulation step (~60 Hz in uncapped / `--60fps`).
- Host presentation blits that completed XFB; re-Present does not re-run GX.
- Raising sim above 60 Hz accelerates per-iteration logic (see prior uncapped speedup RCA).
- Safe visual interpolation (player/camera only) is **not** implemented; it would need retained guest transforms, careful side-effect analysis, and must not touch physics/collision/audio.

Until such work exists, **honest claim:** uncapped / high-refresh = up to `display_refresh_hz` presentations of ~60 Hz unique content, with correct sim/audio timing.

### Audio

`PaceUncappedHost` pumps `Sched_RunDue` / `ppc_check_irq` like VI spins so AI DMA/AX stay fed. See progress log 2026-11-21.

## Spring soft-lock (TR1 WideSpring) — FIXED

See `runtime/debug/spring_60.cpp`.

## Aquarium Park Act 1 spiral (`stg510`) — DEFERRED

Do not investigate or re-introduce spiral gates. Prefer other PC / 60 FPS work.

## Broader 60 FPS compatibility

[`docs/FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md`](FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md).
