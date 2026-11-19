#pragma once
#include <cstdint>
#include <string_view>

// Transition-based input diagnostics (--input-debug). Identical per-frame spam is suppressed.
extern bool g_inputDebug;

void InputDebug_SetEnabled(bool on);
void InputDebug_Host(int chan, uint16_t buttons, bool connected, std::string_view src);
void InputDebug_Wii(int chan, uint16_t wpadButtons, uint8_t devType, std::string_view src);
void InputDebug_Pad(int chan, uint16_t padButtons, int8_t err, std::string_view src);
void InputDebug_Note(std::string_view msg);
