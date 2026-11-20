#pragma once
// Controllers, keyboard and mouse. Controllers are reported in XInput's layout and
// ranges whatever the platform uses (Linux: SDL3 gamepads, which include Steam Input's).
#include <cstdint>

#include "keys.h"

struct PadState
{
    uint16_t buttons = 0;          // PAD_* bits (input/gamepad.h), XInput's values
    int16_t lx = 0, ly = 0;        // sticks, -32768..32767, up is positive
    int16_t rx = 0, ry = 0;
    uint8_t lt = 0, rt = 0;        // triggers 0..255
};

// XInput's suggested dead zones (sticks)
constexpr int16_t PAD_LEFT_THUMB_DEADZONE = 7849;
constexpr int16_t PAD_RIGHT_THUMB_DEADZONE = 8689;

// Controller in player slot 0-3 (false: none).
bool Pad_Read(int index, PadState& out);
// Rumble motors (0 = off .. 65535).
void Pad_SetRumble(int index, uint16_t low, uint16_t high);
// How many physical pads are currently open (0–4).
int Pad_Count();
// Human-readable name for slot 0–3, or nullptr if empty.
const char* Pad_Name(int index);
// Linux: SDL probe of already-connected pads (no toasts). Windows: no-op (XInput is polled).
void Input_EnsurePadsProbed();
// Windows: poll XInput connect/disconnect and show Overlay_Toast. Linux: no-op (SDL events).
void Input_PollHotplug();
// A keyboard key or mouse button (Windows virtual-key code, see keys.h) is held now.
bool Key_Down(int vk);
