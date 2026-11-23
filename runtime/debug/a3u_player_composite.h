#pragma once
#include <cstdint>
#include <cstddef>

// Approach A3u/A3v — retained pre-player background + sparse player-only mid composite.
// Opt-in: SCR_A3U_PLAYER_COMPOSITE=1
//          SCR_A3U_AUDIT=1
//          SCR_A3U_PLAN_PROFILE=1 / SCR_A3U_BUDGET_TRACE=1 / SCR_A3U_OCCLUSION_TRACE=1
// Disabled by default. Does not advance sim/audio. Environment remains sample-held.

struct A3uStats
{
    uint64_t xfbSeen = 0;
    uint64_t bgCaptureOk = 0;
    uint64_t bgCaptureFail = 0;
    uint64_t midAttempt = 0;
    uint64_t midOk = 0;
    uint64_t midFailNoBg = 0;
    uint64_t midFailNoSeg = 0;
    uint64_t midFailOther = 0;
    uint32_t drawsBefore = 0;
    uint32_t drawsPlayer = 0;
    uint32_t drawsAfter = 0;          // non-player after first player (interleaved+trailing)
    uint32_t drawsAfterLast = 0;      // trailing only (after final player draw)
    uint32_t afterOpaque = 0;         // trailing heuristic: z-enabled, no blend
    uint32_t afterBlend = 0;          // trailing heuristic: blend enable
    uint32_t afterOther = 0;
    uint32_t playerFifoOff = 0;
    uint32_t playerCmdEnd = 0;        // archive byte end of last player draw
    uint32_t playerCmdBytes = 0;
    uint32_t playerDix = 0;
    uint32_t planBytes = 0;
    uint32_t planDrawKept = 0;
    uint32_t planDrawSkipped = 0;
    uint64_t planBuildNs = 0;
    uint64_t planHits = 0;
    uint64_t planMisses = 0;
    float lastMidGpuMs = 0;
    float lastMidCpuMs = 0;
    float lastWorkMs = 0;             // mid wall excluding GX_SyncGPU (budget input)
    uint32_t scnMdlEa = 0;
    uint32_t viewBaseEa = 0;
    uint8_t playerContiguous = 0;
    uint8_t bgValid = 0;
};

bool A3u_Enabled();
bool A3u_AuditOn();

void A3u_OnIndexedXf(uint32_t guestEa, uint32_t xfAddr, uint32_t floatCount);
void A3u_NotePlayerXfRow(uint32_t xfAddr, uint32_t floatCount);
bool A3u_OnDrawClassify(bool posMtxDirect, uint8_t defRow, const uint8_t* hostVtxMtx0,
                        uint32_t vtxCount, uint32_t vtxStride);
void A3u_OnXfb();

void A3u_BeginPlayerOnlyMid();
void A3u_EndPlayerOnlyMid();
bool A3u_PlayerOnlyMidActive();
bool A3u_ShouldSkipDraw(bool posMtxDirect, uint8_t defRow, const uint8_t* hostVtxMtx0,
                        uint32_t vtxCount, uint32_t vtxStride);

bool A3u_PrepareMid(uint64_t xfbIndex, uint32_t* outFifoOff, uint32_t* outCmdBytes);
// Build or reuse sparse plan from archive cmd[fifoOff..playerCmdEnd]. Returns pointer/size.
bool A3u_EnsureSparsePlan(uint64_t xfbIndex, const uint8_t* archiveCmd, size_t archiveBytes,
                          const uint8_t** outPtr, size_t* outBytes);
bool A3u_ScratchBeginFromBg(uint32_t* outW, uint32_t* outH);
const void* A3u_GxAtPlayer();
void A3u_NoteMidResult(bool ok, uint32_t dix, float cpuMs, float gpuMs, const char* reason);
// Budget helper: last successful player-pass work ms (excludes SyncGPU when reported).
float A3u_LastWorkMs();
void A3u_NoteWorkMs(float workMs);

void A3u_GetStats(A3uStats& out);
void A3u_OverlayLine(char* buf, size_t bufSize);
