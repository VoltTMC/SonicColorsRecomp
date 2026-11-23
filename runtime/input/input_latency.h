#pragma once
#include <cstddef>
#include <cstdint>

// Opt-in host→guest controller timing (SCR_INPUT_LATENCY=1 or --input-latency).
// Measures backend acquisition-to-consumption only — not physical button-to-OS latency.
// Off by default; near-zero cost when disabled.

bool InputLatency_Enabled();
void InputLatency_SetEnabled(bool on);

// Called after SDL_UpdateJoysticks + PadState sample (host acquisition).
void InputLatency_OnAcquire(int slot, uint16_t buttons, int16_t lx, int16_t ly, uint8_t lt,
                            uint8_t rt, uint64_t seq);

// Called when Gamepad_Poll publishes state for guest WPAD/PAD/motion.
void InputLatency_OnPublish(int slot, uint64_t acquireSeq);

// Periodic stats log (throttled). Call from Gamepad_Poll when enabled.
void InputLatency_Tick();

void InputLatency_OverlayLine(char* buf, size_t bufSize);
