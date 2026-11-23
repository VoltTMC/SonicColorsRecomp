# Windows Xbox controller input latency

**Date:** 2026-11-23  
**Starting commit:** `a84be36`  
**Reports:** Two GitHub issues citing Xbox controllers on Windows with almost no other detail (model / USB vs BT / severity unknown).

## Summary

| Finding | Status |
|---|---|
| Reproducible on native Windows Xbox hardware | **UNVERIFIED** (no native Windows GPU/pad machine in this investigation) |
| Source-level cause identified | **PASS** |
| Fix implemented | **PASS** (`SDL_UpdateJoysticks` every `SdlGamepad_Read`) |
| Xbox-specific vs all Windows SDL pads | **PARTIAL** — bug affects all Windows SDL gamepads; Xbox uses XInput path that only refreshes on `UpdateJoysticks` |
| Linux impact | **PASS** — Linux already pumps SDL every frame; explicit update is redundant but correct |
| Physical end-to-end latency measured | **UNVERIFIED** |

## Architecture (audit)

```
Windows: EnsureSdlGamepad → SDL_INIT_GAMEPAD (no video)
       → SdlGamepad_Read → SDL_UpdateJoysticks (XInputGetState / HIDAPI)
       → SDL_GetGamepadButton/Axis → PadState
Linux:   SDL window loop PumpEvents every frame
       → same SdlGamepad_Read path

Gamepad_Poll → Pad_Read → bindings/deadzones
WPAD HLE / PAD HLE / motion → Gamepad_Poll (guest consumption)
```

| Layer | Implementation |
|---|---|
| Backend | SDL3 shared (`runtime/platform/sdl_gamepad.*`) |
| Xbox on Windows | SDL XInput joystick driver |
| DualShock/DualSense | SDL HIDAPI |
| Hotplug (Win) | `Input_PollHotplug` → `SDL_PumpEvents` **throttled to 250 ms** |
| Guest | WPAD/KPAD/PAD HLE read host `Gamepad_Poll` |

## Root cause

Commit `4205982` (“Remove per-frame Windows input and renderer waste”) throttled **`SDL_PumpEvents`** to 250 ms because `Gamepad_Poll` runs several times per emulated frame.

SDL only refreshes open joystick/gamepad state inside **`SDL_UpdateJoysticks()`**, which the event loop normally runs from `PumpEvents` when joystick events are enabled (`SDL_HINT_AUTO_UPDATE_JOYSTICKS` default `"1"` = update when getting input events).

After the throttle, Windows stopped calling `UpdateJoysticks` except ~4 Hz. Cached XInput/HIDAPI state could remain **stale for up to ~250 ms**. That matches “excessive latency” reports for Xbox pads on Windows.

Linux was unaffected: the SDL window thread pumps events every frame.

This is **not** guest sim speed, audio timing, or presentation latency. It is host acquisition freshness.

## Fix

In `SdlGamepad_Read`:

1. Call `SDL_UpdateJoysticks()` before reading buttons/axes.
2. Keep Windows hotplug `PumpEvents` throttled at 250 ms (connect/disconnect only).

No new dependencies. No polling thread. No sim/audio/timing changes.

## Instrumentation

| Switch | Role |
|---|---|
| `SCR_INPUT_LATENCY=1` | Enable |
| `--input-latency` | Same |

Logs every ~2 s (slot 0+): acquire-interval avg/p50/p95/p99/max, acquire→publish avg/max/last. Overlay line when `--perf` HUD is on.

**These measure backend acquisition → guest publish only**, not physical button → Windows.

## Test matrix

| Platform | Controller | Connection | Result |
|---|---|---|---|
| Windows native | Xbox USB/BT/Adapter | — | **UNVERIFIED** |
| Windows Wine | any | — | API smoke only; not latency proof |
| Linux | whatever is connected | — | Build + smoke; latency tool runnable |
| Non-Xbox Windows | — | — | **UNVERIFIED** (same fix path) |

## Future report checklist

Please include:

1. Controller model (e.g. Xbox Series X|S, Xbox One)
2. Connection: USB / Bluetooth / Xbox Wireless Adapter
3. Windows version
4. Build / commit
5. Log line: `SDL3 gamepad backend ready`
6. Optional: run with `--input-latency --perf` and attach ~10 s of `input-latency slot0:` lines
7. Frame-rate mode / display Hz / VSync
8. Whether a non-Xbox pad feels the same

## Regression

- `regression_smoke` OK  
- Linux + MinGW builds OK  
- Mappings / deadzones / WPAD/PAD HLE unchanged  
- Diagnostics off by default  
