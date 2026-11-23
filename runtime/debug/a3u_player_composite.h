#pragma once
#include <cstdint>

// Approach A3u — retained pre-player background + player-only mid composite.
// Opt-in: SCR_A3U_PLAYER_COMPOSITE=1 (mid uses bg+player segment).
//          SCR_A3U_AUDIT=1 (bounded ordering / cost logs).
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
    uint32_t drawsBefore = 0;   // last captured XFB
    uint32_t drawsPlayer = 0;
    uint32_t drawsAfter = 0;
    uint32_t playerFifoOff = 0;
    uint32_t playerCmdBytes = 0;
    uint32_t playerDix = 0;     // DrawIndexed in last mid
    float lastMidGpuMs = 0;
    float lastMidCpuMs = 0;
    uint32_t scnMdlEa = 0;
    uint32_t viewBaseEa = 0;
    uint8_t playerContiguous = 0;  // 1 if no non-player draw between first/last player
    uint8_t bgValid = 0;
};

bool A3u_Enabled();
bool A3u_AuditOn();

// Live GPU thread: ViewPos-range indexed XF → mark player XF rows.
void A3u_OnIndexedXf(uint32_t guestEa, uint32_t xfAddr, uint32_t floatCount);
// Mid: PoseInterp override landed into xfmem[xfAddr] — mark row as player.
void A3u_NotePlayerXfRow(uint32_t xfAddr, uint32_t floatCount);
// Live: before decoding a draw — may capture pre-player EFB. Returns true if
// this draw is classified as player-owned (for audit counters).
bool A3u_OnDrawClassify(bool posMtxDirect, uint8_t defRow, const uint8_t* hostVtxMtx0,
                        uint32_t vtxCount, uint32_t vtxStride);
void A3u_OnXfb();  // finalize audit counters for the XFB

// Mid replay: activate player-segment filter / skip non-player draws.
void A3u_BeginPlayerOnlyMid();
void A3u_EndPlayerOnlyMid();
bool A3u_PlayerOnlyMidActive();
// True → Render_Draw should no-op (env geometry during player-only mid).
bool A3u_ShouldSkipDraw(bool posMtxDirect, uint8_t defRow, const uint8_t* hostVtxMtx0,
                        uint32_t vtxCount, uint32_t vtxStride);

// Lookup bg + fifo offset for the current mid archive xfbIndex.
bool A3u_PrepareMid(uint64_t xfbIndex, uint32_t* outFifoOff, uint32_t* outCmdBytes);
bool A3u_ScratchBeginFromBg(uint32_t* outW, uint32_t* outH);
const void* A3u_GxAtPlayer();  // GXState* or null
void A3u_NoteMidResult(bool ok, uint32_t dix, float cpuMs, float gpuMs, const char* reason);

void A3u_GetStats(A3uStats& out);
void A3u_OverlayLine(char* buf, size_t bufSize);
