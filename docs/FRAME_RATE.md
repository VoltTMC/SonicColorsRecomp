# Frame rate — Sonic Colors Recomp

Guest VI wait fields and derived `dt` (`fn_803E19B0` → SDA `0x808F2D98`).

| Config | Wait fields | Guest update rate | Notes |
|--------|-------------|-------------------|-------|
| `frame_rate=30` | 2 | 30 Hz | Original pacing |
| `frame_rate=60` / `--60fps` | 1 | 60 Hz | Spring cooldown gate on by default |
| `--guest-60` / `SCR_GUEST_60=1` | 1 | 60 Hz | Forces wait=1 even if `frame_rate` is 30 |
| `frame_rate=0` | 0 (uncapped) | host-limited | Optional `host_fps_cap` paces guest |

`SCR_SPRING_CD_GATE=0` disables the WideSpring activate hold.

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
