# Controller input

## Architecture

| Platform | Backend | Player slots |
|----------|---------|--------------|
| Linux | SDL3 gamepads (`SDL_OpenGamepad`) | Packed by connect order (hotplug compact) |
| Windows | Dynamic XInput (`LoadLibrary` xinput1_4 → 1_3 → 9_1_0) | Packed by ascending XInput **user index** → slots 0..n−1 |

Both expose the same host layout (`PadState` / `PAD_*` = XInput button bits and stick ranges). `Gamepad_Poll` applies bindings, dead zones, keyboard fallback (player 1), then WPAD/PAD HLE.

Launcher / settings overlay: `Ui_FeedGamepadFromPads()` feeds ImGui nav from **packed slot 0** (Windows). Linux uses `ImGui_ImplSDL3` gamepad support (feed is a no-op to avoid duplicates).

## Windows notes

- Do **not** hard-link `XINPUT1_3.dll`; the process loads whichever XInput DLL is present.
- A pad on XInput user 1 must still appear as player 1 (slot 0) after packing.
- Xbox and XInput-compatible pads: supported path.
- DualSense / DualShock 4: need Steam Input, DS4Windows, or another XInput wrapper until a native HID/SDL Windows path exists.
- Diagnostics: `SCR_INPUT_DIAG=1` (throttled). Inventory: `--list-gamepads`.

## Validation labels

| Label | Meaning |
|-------|---------|
| Linux tested | Physical / SDL pads exercised on Linux |
| Windows compiled | MinGW/MSVC build succeeds |
| Windows API tested | `--list-gamepads` / dynamic load under Wine or similar |
| Windows hardware tested | Physical pad on native Windows launcher + TR1 gameplay |
| Unsupported | Device class not covered without a wrapper |

## Native Windows test procedure

1. Build or copy `SonicColorsRecomp.exe` (+ `libwinpthread-1.dll` if MinGW).
2. `SonicColorsRecomp.exe --list-gamepads` with Xbox pad connected → slot `0:` line with non-zero sticks/buttons when moved.
3. Launch launcher: D-pad/stick moves focus; A confirms.
4. In-game: jump / boost / camera / pause with the same pad.
5. Unplug → toast; keyboard still drives player 1; replug → toast and control returns.
6. Optional second pad → slot 1.
7. Record: Windows version, pad model, USB/BT, which XInput DLL logged at startup.
