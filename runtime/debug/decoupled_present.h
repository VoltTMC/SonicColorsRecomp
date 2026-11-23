#pragma once
#include <cstddef>
#include <cstdint>

// Approach A3o — decoupled 2× presentation scheduler (opt-in).
// SCR_DECOUPLED_PRESENT=1       enable scheduler (off by default)
// SCR_DECOUPLED_PRESENT_RATE=2  presents per sim update (default 2)
// SCR_PRESENT_TRACE=1           bounded scheduling diagnostics
// SCR_PRESENT_LATENCY_MS=<f>    interpolation delay (default ≈½ sim period)
//
// Guest sim/audio cadence unchanged. Unique mid-frames require a ready A2b/A2c
// archive (SCR_A2B_FIFO_CAPTURE=1 + dense/env pass). Without archive, mid slots
// re-present the last XFB for pacing only and are counted as duplicates.

struct DecoupledPresentStats
{
    uint64_t simTicks = 0;
    uint64_t scheduleAttempts = 0;
    uint64_t uniqueMid = 0;
    uint64_t duplicatePace = 0;
    uint64_t midFail = 0;
    uint64_t timeAlphaOk = 0;
    uint64_t timeAlphaFail = 0;
    uint64_t missedDeadline = 0;
    float alphaLast = 0;
    float alphaMin = 1.f;
    float alphaMax = 0.f;
    float latencyMs = 0;
    float targetRate = 2.f;
    uint32_t lastMidHash = 0;
    uint32_t lastGuestHash = 0;
    uint32_t distinctMidVsGuest = 0;
};

bool DecoupledPresent_Enabled();
float DecoupledPresent_TargetRate();
// Called from PaceUncappedHost while waiting for the next sim deadline.
// ctx/base unused today; reserved for future guest-thread work.
void DecoupledPresent_OnPaceWait(uint64_t nowTicks, uint64_t simDeadlineTicks, uint64_t freq);
// After each guest XFB — record cadence anchors.
void DecoupledPresent_OnXfb();
void DecoupledPresent_GetStats(DecoupledPresentStats& out);
void DecoupledPresent_OverlayLine(char* buf, size_t bufSize);
