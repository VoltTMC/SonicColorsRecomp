#pragma once
#include <cstdint>

void DSP_PushMail(uint32_t mail, bool interrupt, uint64_t delayTicks);
void DSP_SetAISampleRate(uint32_t rate);
bool AI_DmaEnabled();   // AI DMA bit15 — false when the game has stopped the mixer
