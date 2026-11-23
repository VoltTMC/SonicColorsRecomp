#pragma once
#include "pose_capture.h"
#include <cstddef>
#include <cstdint>

// Approach A3n/A3o — live TR1 Sonic pose interpolation (opt-in).
// SCR_POSE_INTERP=1            enable consumer + GX ViewPos override
// SCR_POSE_INTERP_ALPHA=<0..1> fixed alpha debug (default 0.5); ignored when time-driven
// SCR_POSE_INTERP_TRACE=1      bounded summary logs
// SCR_POSE_INTERP_VALIDATE=1   residual checks on reconstructed ViewPos
// SCR_DECOUPLED_PRESENT=1      time-driven alpha from presentation clock (A3o)
// Off by default.

enum PoseInterpFallback : uint16_t
{
    PoseInterpFb_None = 0,
    PoseInterpFb_Disabled = 1,
    PoseInterpFb_NoPair = 2,
    PoseInterpFb_PairReject = 3,
    PoseInterpFb_Decompose = 4,
    PoseInterpFb_Validate = 5,
    PoseInterpFb_Unbracketed = 6,
};

struct PoseInterpStats
{
    uint64_t attempts = 0;
    uint64_t applied = 0;
    uint64_t fallback = 0;
    uint64_t fbReason[8]{};
    uint64_t pairReject[16]{};
    uint64_t nodesInterp = 0;
    uint64_t viewRebuilt = 0;
    uint64_t gxOverrides = 0;
    uint64_t gxMiss = 0;
    uint64_t timeDriven = 0;
    float alphaLast = 0;
    float alphaMin = 1.f;
    float alphaMax = 0.f;
    float relResMax = 0;
    float endpointErr0 = 0;
    float endpointErr1 = 0;
};

bool PoseInterp_Enabled();
void PoseInterp_OnXfb();  // after PoseCapture_OnXfb — build output or fallback
// A3o: rebuild published output for an explicit alpha using latest qualified pair.
bool PoseInterp_BuildAtAlpha(float alpha);
// A3o: bracket by hostTicks target; fails closed if unbracketed (no clamp-to-hide).
bool PoseInterp_BuildForHostTime(uint64_t hostTicks, float* outAlpha);
// GPU thread: substitute ViewPos matrix for indexed XF if EA matches active output.
bool PoseInterp_OverrideIndexed(uint32_t guestEa, uint32_t index, uint32_t stride,
                                float outMtx12[12]);
bool PoseInterp_Active();
bool PoseInterp_TimeDriven();
void PoseInterp_GetStats(PoseInterpStats& out);
void PoseInterp_OverlayLine(char* buf, size_t bufSize);
