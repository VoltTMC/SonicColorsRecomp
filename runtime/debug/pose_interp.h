#pragma once
#include "pose_capture.h"
#include <cstddef>
#include <cstdint>

// Approach A3n — first live TR1 Sonic pose interpolation (opt-in).
// SCR_POSE_INTERP=1            enable consumer + GX ViewPos override
// SCR_POSE_INTERP_ALPHA=<0..1> fixed alpha (default 0.5 mid-bracket latency)
// SCR_POSE_INTERP_TRACE=1      bounded summary logs
// SCR_POSE_INTERP_VALIDATE=1   residual checks on reconstructed ViewPos
// Off by default. One pose per existing present; no extra XFBs / no FPS claim.

enum PoseInterpFallback : uint16_t
{
    PoseInterpFb_None = 0,
    PoseInterpFb_Disabled = 1,
    PoseInterpFb_NoPair = 2,
    PoseInterpFb_PairReject = 3,
    PoseInterpFb_Decompose = 4,
    PoseInterpFb_Validate = 5,
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
    float alphaLast = 0;
    float relResMax = 0;
    float endpointErr0 = 0;
    float endpointErr1 = 0;
};

bool PoseInterp_Enabled();
void PoseInterp_OnXfb();  // after PoseCapture_OnXfb — build output or fallback
// GPU thread: substitute ViewPos matrix for indexed XF if EA matches active output.
bool PoseInterp_OverrideIndexed(uint32_t guestEa, uint32_t index, uint32_t stride,
                                float outMtx12[12]);
bool PoseInterp_Active();
void PoseInterp_GetStats(PoseInterpStats& out);
void PoseInterp_OverlayLine(char* buf, size_t bufSize);
