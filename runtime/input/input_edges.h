#pragma once
// I1: preserve short press/release transitions between guest input samples.
// Host backends still report levels; this layer latches rising edges (and optional
// OS key events) until Gamepad_Poll publishes a per-guest-frame snapshot.
#include <cstdint>
#include <cstddef>

void InputEdges_ResetAll();
void InputEdges_OnFocusChange(bool focused);

// Pad level sample (call after SDL_UpdateJoysticks / Pad_Read). Rising edges stick
// until the next guest-frame publish for that slot.
void InputEdges_NotePadButtons(int slot, uint16_t buttons);

// OS keyboard events (WM_KEY* / SDL_EVENT_KEY_*). Auto-repeat must not call Down again.
void InputEdges_KeyDown(int vk);
void InputEdges_KeyUp(int vk);
// Effective key for bindings: live Key_Down OR sticky-until-published.
bool InputEdges_KeyEffectiveDown(int vk);

// High-rate pad sampling between guest polls (FrameLoopWait / Present pacing).
void InputEdges_SamplePads();

// Once per guest frame index: compose published button mask for slot.
// live = current physical pad buttons (pre-binding). Returns live | stickyDown.
uint16_t InputEdges_PublishPad(int slot, uint16_t live, uint64_t guestFrame);
// After keyboard bindings applied for slot 0, clear sticky keys that were observed.
void InputEdges_PublishKeys(uint64_t guestFrame);

// Opt-in diagnostics (SCR_INPUT_I1=1 / --input-i1).
bool InputEdges_DiagEnabled();
void InputEdges_SetDiagEnabled(bool on);
void InputEdges_DiagOverlay(char* buf, size_t bufSize);

struct InputEdgesStats
{
    uint64_t hostPadSamples = 0;
    uint64_t hostKeyDown = 0;
    uint64_t hostKeyUp = 0;
    uint64_t risingLatched = 0;
    uint64_t publishes = 0;
    uint64_t stickyConsumed = 0;
    uint64_t focusClears = 0;
    uint64_t overflow = 0; // reserved (bounded stickies never grow unboundedly)
};
InputEdgesStats InputEdges_Stats();
