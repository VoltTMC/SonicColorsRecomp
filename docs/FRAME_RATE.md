# Frame rate — Sonic Colors Recomp

Guest VI wait fields and derived `dt` (`fn_803E19B0` → SDA `0x808F2D98`).

| Config | Wait fields | Guest update rate | Notes |
|--------|-------------|-------------------|-------|
| `frame_rate=30` | 2 | 30 Hz | Original pacing |
| `frame_rate=60` / `--60fps` | 1 | 60 Hz | Spring cooldown gate on by default |
| `--guest-60` / `SCR_GUEST_60=1` | 1 | 60 Hz | Forces wait=1 even if `frame_rate` is 30 |
| `frame_rate=0` | 0 (uncapped) | host-limited | Wall-clock `dt`; optional `host_fps_cap` |

`SCR_SPRING_CD_GATE=0` disables the WideSpring activate hold.

## Uncapped mode (`--frame-rate 0` / `frame_rate=0`)

### Root cause of the former ~60 FPS lock

Bypassing only the `VIGetRetraceCount` EQ spin in `FrameLoopWait` (`fn_800126EC`) was **not** enough. After that spin the guest still called:

1. **`fn_80354320` (`VIFlushAndWait`)** — drains pending VI register writes; IRQ-paced on hardware.
2. **`fn_80353360` (`VIWaitNextRetrace`)** — busy-waits until the SDA retrace counter (same cell as `VIGetRetraceCount`) increments.

Both synchronise to the emulated 60 Hz VI field. `--perf` showed **0 EQ spins** but a hard **~16.7 ms** guest∶XFB∶present period (`in-wait + gap`). Present/VSync were not the limiter (`Present` ~0.01 ms with default `vsync=false`).

### Fix

When uncapped, those two post-EQ barriers return immediately (`runtime/kernel/hle_os.cpp`), same as the EQ-spin bypass. Host XFB presentation does not need VI scanout latch timing.

### Simulation vs presentation

- **Integer** wait field published as **1** (compatibility with paths that read field counts).
- **Float** `dt` at SDA `0x808F2D98` is overwritten with **wall-clock elapsed seconds** (clamped to ~1/300…1/15 s) so gameplay keeps real-time speed while the loop can exceed 60 Hz.
- Present stays coupled ~1∶1 with guest updates (no separate interpolate/extrapolate path).
- Optional `host_fps_cap` (0 = unlimited) paces guest updates after the wait.
- **VSync** (`vsync` setting / Present swap interval) is independent; uncapped does not force it off, and 30/60 modes do not change VSync.

On a display above 60 Hz with `vsync=false` (or a matching refresh with `vsync=true`), presented FPS can exceed 60 when the CPU/GPU allow.

Use **30** or **60** for the compatibility-safe fixed field pacing; use **uncapped** when you want host-limited rate with wall-clock `dt`.

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
