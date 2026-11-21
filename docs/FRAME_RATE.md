# Frame rate — Sonic Colors Recomp

Guest VI wait fields and derived `dt` (`fn_803E19B0` → SDA `0x808F2D98` / `lbl_808F2D98`).

| Config | Wait fields | Simulation rate | Presentation | Notes |
|--------|-------------|-----------------|--------------|-------|
| `frame_rate=30` | 2 | 30 Hz | ~1∶1 with sim | Original pacing |
| `frame_rate=60` / `--60fps` | 1 | 60 Hz | ~1∶1 with sim | Spring cooldown gate on by default |
| `--guest-60` / `SCR_GUEST_60=1` | 1 | 60 Hz | ~1∶1 with sim | Forces wait=1 even if `frame_rate` is 30 |
| `frame_rate=0` | 0 (VI waits skipped) | **60 Hz fixed** | Uncapped re-present | Optional `host_fps_cap` |

`SCR_SPRING_CD_GATE=0` disables the WideSpring activate hold.

## Semantics of `lbl_808F2D98` (SDA float dt)

From the SNCP8P decomp (`fn_803E19B0` / `fn_800115BC`):

- Integer wait fields `N` are stored at SDA `0x808F2D94` (`lbl_808F2D94`).
- Float `dt = N / 60` (seconds per guest step) is stored at `0x808F2D98`.
- `fn_800115BC` passes **both** the float (`sp.x`) and a tick count derived from `fn_803E19E4()` into the update path (`fn_80014AA8`).

So the float is a **per-simulation-step duration in seconds**, published once per `FrameLoopWait`, not a host-frame multiplier. Motion that multiplies by `dt` stays wall-time correct when `N` and the loop rate stay matched (e.g. wait=1 at 60 Hz → `dt=1/60`).

Many guest systems still advance **once per loop iteration** (frame counters, collide-then-activate, scripted path steps). Those do **not** become frame-rate independent just because `0x808F2D98` is overwritten with wall-clock elapsed time.

## Uncapped mode (`--frame-rate 0` / `frame_rate=0`)

### Why aacec3a accelerated gameplay

1. VI EQ spin + `VIFlushAndWait` + `VIWaitNextRetrace` were skipped → guest loop reached hundreds of Hz.
2. Wall-clock overwrite of `0x808F2D98` only helped dt-scaled integrators.
3. Per-iteration systems still ran once per loop → Sonic / timers / collisions advanced too fast.

### Architecture (sim vs present)

```
each FrameLoopWait (uncapped):
  publish N=1, dt=1/60          # same sim step as --60fps
  skip VI EQ / flush / retrace  # no 60 Hz presentation lock
  PaceUncappedHost:
    until next 60 Hz sim deadline:
      Renderer_RepeatLastXfb()  # extra presents, no guest update
    return                      # one sim step per 1/60 s wall time
```

| Quantity | Uncapped behaviour |
|----------|--------------------|
| Simulation (`FrameLoopWait` / guest update) | Locked to **60 Hz** wall clock |
| Unique XFB copies | ~60 Hz (one per sim step) |
| Present calls | Can exceed 60 via re-presents (see metrics) |
| Audio (AI DMA / AX) | Pumped during the pace wait via `Sched_RunDue` + `ppc_check_irq` (same as VI spins) |
| `host_fps_cap` | Caps **re-present** rate when VSync is off (0 ≈ ~1000 Hz floor) |
| VSync | Independent of frame-rate mode; follows **host** refresh |

## Metrics (do not conflate)

Status line and FPS overlay report separate rates:

| Metric | Meaning |
|--------|---------|
| `sim` | Guest `FrameLoopWait` / simulation steps per second |
| `xfb` | Unique guest XFB copies submitted for blit |
| `present` | Host `IDXGISwapChain::Present` calls (includes uncapped re-presents of the last XFB) |
| `guest_vi` | Emulated Wii VI field rate (~60 Hz NTSC); **not** the PC monitor |
| `display` | Host monitor mode refresh (`Window_DisplayRefreshHz`) |
| `vsync` | `Present(1)` vs `Present(0[, ALLOW_TEARING])` |

A high `present` on a 60 Hz panel does **not** mean the LCD refreshes faster — only that frames were submitted. With **VSync on**, Present blocks to the host refresh (60/120/165/…); with **VSync off**, submission can exceed the panel rate (tearing possible). Guest VI stays ~60 either way.

`--perf` also logs these. Healthy uncapped: **sim ≈ 60**, **xfb ≈ 60**, **present** may be ≫ 60 when VSync is off, **audio repeats ≈ 0**.

### Audio vs rendering

30/60 modes keep AI DMA and AX alive during VI waits (`PPC_CHECK_IRQ` → `Sched_RunDue` pushes AI buffers; guest AID handlers refill via AX). Skipping those waits without an equivalent pump starved audio for most of each 16.7 ms slot → identical AI blocks (`g_audioRepeats`), underruns, and broken playback.

`PaceUncappedHost` therefore runs `Sched_RunDue` / `Irq_Update` / `ppc_check_irq` every iteration of the re-present loop so audio timing stays on the emulated AI sample clock, independent of present FPS.

### What uncapped is not

- Not “run the guest as fast as possible” (that fast-forwards per-frame logic).
- Not variable-dt chaos across the whole engine.
- Not a return of VI-locked 60 Hz presentation.

Use **30** / **60** for fixed VI-paced modes; use **uncapped** for 60 Hz simulation with unlocked presentation.

## Spring soft-lock (TR1 WideSpring) — FIXED

WideSpring collide re-activated every guest frame while overlapped. Gate holds
activate for 0.5s wall time at wait=1. See `runtime/debug/spring_60.cpp`.

## Aquarium Park Act 1 spiral (`stg510`) — DEFERRED

**Do not investigate, patch, or re-introduce spiral/OOC/ForcePath gates.**

Opening tube at wait=1 remains an open compatibility issue. Prior experimental
hooks (`runtime/debug/spiral_60.cpp` and related overrides) were **removed**.
Stage ID: **`stg510`** (BGM `qua`); `stg410` is Planet Wisp (`pla`).

Details of earlier RCA attempts are historical only; treat the issue as
deferred indefinitely. Prefer other 60 FPS / PC work.

## Broader 60 FPS compatibility

[`docs/FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md`](FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md).
