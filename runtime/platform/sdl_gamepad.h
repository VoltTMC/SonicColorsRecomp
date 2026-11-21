#pragma once
// Shared SDL3 gamepad backend (Linux windowing + Windows/Wine HID / XInput via SDL).
#include "input.h"

#include <SDL3/SDL.h>

// Call before SDL_Init(GAMEPAD) so HIDAPI backends are selected.
void SdlGamepad_ApplyHints();
// Call once after SDL_Init includes SDL_INIT_GAMEPAD.
void SdlGamepad_Init();
void SdlGamepad_Shutdown();

// Linux: from the window event loop. Windows: Input_PollHotplug pumps SDL events.
void SdlGamepad_HandleEvent(const SDL_Event& e);
void SdlGamepad_ProbeConnected(bool toast);
// Windows: SDL_PumpEvents + drain GAMEPAD_ADDED/REMOVED (and optional joystick events).
void SdlGamepad_PumpHotplug();

int SdlGamepad_Count();
const char* SdlGamepad_Name(int index);
bool SdlGamepad_Read(int index, PadState& out);
void SdlGamepad_SetRumble(int index, uint16_t low, uint16_t high);
