# Controller input

## Architecture

| Platform | Backend | Player slots |
|----------|---------|--------------|
| Linux | SDL3 (`runtime/platform/sdl_gamepad.*`) | Packed by connect order (hotplug compact) |
| Windows / Wine | Same SDL3 backend (HIDAPI DualShock/DualSense + XInput Xbox); Win32 window stays native | Packed by connect order |

Both expose the same host layout (`PadState` / `PAD_*` = XInput button bits and stick ranges). `Gamepad_Poll` applies bindings, dead zones, keyboard fallback (player 1), then WPAD/PAD HLE.

Launcher / settings overlay: `Ui_FeedGamepadFromPads()` feeds ImGui nav from **packed slot 0** (Windows). Linux uses `ImGui_ImplSDL3` gamepad support (feed is a no-op to avoid duplicates).

## Windows notes

- Ship `SDL3.dll` next to the exe (POST_BUILD + `package_windows_cross.sh` copies `out-windows/*.dll`).
- No hard-link of `XINPUT1_3.dll`; Xbox pads come through SDL’s XInput joystick driver.
- DualShock 4 / DualSense: HIDAPI via SDL (no DS4Windows / Steam Input required).
- Wine: `wine … --list-gamepads` is an API smoke test only — native Windows hardware still required for release “done”.
- Diagnostics: `SCR_INPUT_DIAG=1` (throttled). Inventory: `--list-gamepads`.
- Latency: `SCR_INPUT_LATENCY=1` / `--input-latency` (see `docs/WINDOWS_XBOX_INPUT_LATENCY.md`).
- **Freshness:** `SdlGamepad_Read` always calls `SDL_UpdateJoysticks()` so XInput/HIDAPI state is not tied to the 250 ms hotplug `PumpEvents` throttle.

## Validation labels

| Label | Meaning |
|-------|---------|
| Linux tested | Physical / SDL pads exercised on Linux |
| Windows compiled | MinGW/MSVC build succeeds |
| Windows API tested | `--list-gamepads` under Wine (SDL3.dll + HID/XInput) |
| Windows hardware tested | Physical pad on native Windows launcher + TR1 gameplay |
| Unsupported | Device class not covered without a wrapper |

## Native Windows test procedure

1. Build or copy `SonicColorsRecomp.exe` + `SDL3.dll` (+ `libwinpthread-1.dll` if MinGW).
2. `SonicColorsRecomp.exe --list-gamepads` with pad connected → slot `0:` name + non-zero sticks/buttons when moved (Xbox or DS4/DualSense).
3. Launch launcher: D-pad/stick moves focus; A confirms.
4. In-game: jump / boost / camera / pause with the same pad.
5. Unplug → toast; keyboard still drives player 1; replug → toast and control returns.
6. Optional second pad → slot 1.
7. Record: Windows version, pad model, USB/BT, log line `SDL3 gamepad backend ready`.
