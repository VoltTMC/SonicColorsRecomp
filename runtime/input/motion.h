#pragma once
#include <cstdint>

// Synthesises Wii Remote + Nunchuk samples from the host gamepad.
void Motion_Step(int chan, float dt);
void Motion_FillStatus(int chan, uint32_t guestStatus, bool connected);
int Motion_GravityUnit(uint32_t type);

// WPAD DPD/IR enable state (from WPADControlDpd HLE).
bool WPAD_DpdEnabled(int chan);
