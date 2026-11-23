#pragma once
#include <cstddef>
#include <cstdint>

// Approach A3o/A3r — decoupled 2× presentation scheduler (opt-in).
// SCR_DECOUPLED_PRESENT=1       enable scheduler (off by default)
// SCR_DECOUPLED_PRESENT_RATE=2  presents per sim update (default 2)
// SCR_PRESENT_TRACE=1           bounded scheduling diagnostics
// SCR_PRESENT_LATENCY_MS=<f>    interpolation delay (default ≈½ sim period)
//
// Guest sim/audio cadence unchanged. Unique mid-frames require a ready A2b/A2c
// archive (SCR_A2B_FIFO_CAPTURE=1 + dense/env pass). Without archive, mid slots
// re-present the last XFB for pacing only and are counted as duplicates.
//
// A3q: SCR_MID_POSE_TRACE=1 — bounded correlated mid pose/camera records (off by default).
// A3r: production mid attempted once per sim on guest pace enter (not only wait residual).
//      SCR_MID_HASH=1 — enable CPU image hash on mid present (off; expensive).
//      SCR_MID_PROFILE=1 — emit bounded mid stage ms in status overlay path.

struct DecoupledPresentStats
{
    uint64_t simTicks = 0;
    uint64_t scheduleAttempts = 0;
    uint64_t midAttempts = 0;
    uint64_t midSelected = 0;         // archive pin/eval started
    uint64_t uniqueMid = 0;
    uint64_t uniqueMidPoseOk = 0;     // UNIQUE + PoseInterp build succeeded
    uint64_t uniqueMidPoseGx = 0;     // UNIQUE + gxOverrides consumed > 0
    uint64_t duplicatePace = 0;
    uint64_t midFail = 0;
    uint64_t midFailNoArchive = 0;
    uint64_t midFailNotReady = 0;
    uint64_t midFailNoPose = 0;       // UNIQUE without poseOk (archive-only mid)
    uint64_t midSkipBudget = 0;       // skipped to protect sim deadline
    uint64_t timeAlphaOk = 0;
    uint64_t timeAlphaFail = 0;
    uint64_t missedDeadline = 0;
    uint64_t midNsTotal = 0;          // CPU wall for TryUniqueMid
    uint64_t midNsPeak = 0;
    float alphaLast = 0;
    float alphaMin = 1.f;
    float alphaMax = 0.f;
    float latencyMs = 0;
    float targetRate = 2.f;
    uint32_t lastMidHash = 0;
    uint32_t lastGuestHash = 0;
    uint32_t distinctMidVsGuest = 0;
    uint32_t lastGxConsumed = 0;
    uint16_t lastNodesInterp = 0;
    float lastMidMs = 0;
};

// Bounded A3q mid-attempt record (pose= field in logs is poseOk 0/1, not a count).
struct MidPoseTrace
{
    uint64_t corrId = 0;
    uint64_t targetTicks = 0;
    uint64_t seqPrev = 0;
    uint64_t seqCurr = 0;
    float alpha = 0;
    uint8_t poseOk = 0;
    uint8_t uniqueOk = 0;
    uint8_t poseFb = 0;  // PoseInterpFallback
    uint16_t nodesInterp = 0;
    uint16_t nView = 0;
    uint32_t viewBaseEa = 0;
    uint32_t viewActiveEa = 0;
    uint32_t archiveXfb = 0;
    uint32_t draws = 0;
    uint32_t colorHash = 0;
    uint64_t gxConsumed = 0;
    char detail[160]{};
};

bool DecoupledPresent_Enabled();
float DecoupledPresent_TargetRate();
// Called from PaceUncappedHost while waiting for the next sim deadline.
// ctx/base unused today; reserved for future guest-thread work.
void DecoupledPresent_OnPaceWait(uint64_t nowTicks, uint64_t simDeadlineTicks, uint64_t freq);
// Guest-thread entry once per uncapped frame (before wait). Consumes
// SCR_MID_POSE_TRACE force-mid requests; safe for GX_SyncGPU.
// simDeadlineTicks: next sim deadline (0 = unknown). Soft budget unless
// SCR_MID_IGNORE_BUDGET=1.
void DecoupledPresent_OnGuestPaceEnter(uint64_t nowTicks, uint64_t freq,
                                       uint64_t simDeadlineTicks);
// After each guest XFB — record cadence anchors.
void DecoupledPresent_OnXfb();
void DecoupledPresent_GetStats(DecoupledPresentStats& out);
void DecoupledPresent_OverlayLine(char* buf, size_t bufSize);
bool DecoupledPresent_MidPoseTraceEnabled();
uint32_t DecoupledPresent_MidPoseTraceCount();
bool DecoupledPresent_GetMidPoseTrace(uint32_t index, MidPoseTrace& out);
