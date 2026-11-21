#pragma once
#include <cstddef>
#include <cstdint>

// Approach A2c/A2d — resource closure + isolated scratch-EFB replay.
// Requires SCR_A2B_FIFO_CAPTURE=1. Replay: SCR_A2C_SCRATCH_REPLAY=1 (default off).
// A2d: seed scratch from captured incoming EFB (color+depth); exact live-vs-scratch compare.
// Never writes the live game XFB; suppresses PE IRQs; restores GX/guest RAM after.

enum A2cReadyFlags : uint32_t
{
    A2cReady_CmdComplete = 1u << 0,
    A2cReady_SubsComplete = 1u << 1,
    A2cReady_GxStartPresent = 1u << 2,
    A2cReady_VertexComplete = 1u << 3,
    A2cReady_TextureComplete = 1u << 4,
    A2cReady_DlComplete = 1u << 5,
    A2cReady_NoOverflow = 1u << 6,
    A2cReady_PassSelected = 1u << 7,
    A2cReady_EfbIncoming = 1u << 8,   // A2d: pass-entry EFB snapshot present
    A2cReady_All = 0x1FFu,
};

struct A2cPassReport
{
    uint64_t xfbIndex = 0;
    uint64_t simFrameId = 0;
    uint32_t passBytes = 0;       // cmd prefix before first toXFB BP 0x52 (exclusive)
    uint32_t passHash = 0;
    uint32_t drawCount = 0;
    uint32_t depCount = 0;
    uint32_t depBytes = 0;
    uint32_t unresolvedRequired = 0;
    uint32_t readyFlags = 0;
    uint32_t failFlags = 0;
    uint32_t efbInColorHash = 0;
    uint32_t efbInDepthHash = 0;
    bool efbIncomingValid = false;
    bool replayReady = false;
    char reason[160]{};
};

struct A2cReplayResult
{
    bool attempted = false;
    bool completed = false;
    bool isolationOk = false;
    bool stateRestored = false;
    bool imageCompared = false;
    bool imageMatch = false;       // exact color + depth (when depthCompared)
    bool colorExact = false;
    bool depthExact = false;
    bool depthCompared = false;
    bool seededFromIncoming = false;
    uint32_t liveEfbHash = 0;
    uint32_t scratchEfbHash = 0;
    uint32_t liveDepthHash = 0;
    uint32_t scratchDepthHash = 0;
    uint32_t efbInColorHash = 0;
    uint32_t efbInDepthHash = 0;
    uint32_t colorMismatch = 0;
    uint32_t depthMismatch = 0;
    uint32_t maxChannelDelta = 0;
    uint32_t comparedPixels = 0;
    uint32_t passHash = 0;
    uint32_t scratchW = 0;
    uint32_t scratchH = 0;
    char detail[192]{};
};

bool A2c_ScratchReplayEnabled();
bool A2c_ClosureCaptureEnabled();

void A2c_OnDraw(int vat, uint32_t count, const uint8_t* vtxStream, uint32_t streamBytes);
void A2c_OnTexture(uint32_t physAddr, uint32_t byteSize, uint32_t fmt, uint32_t w, uint32_t h,
                   uint32_t tlutTmemOff, uint32_t tlutBytes);
void A2c_OnTlutRam(uint32_t physAddr, uint32_t byteSize);

void A2c_OnArchivedFrame();

bool A2c_GetPassReport(A2cPassReport& out);
bool A2c_GetLastReplay(A2cReplayResult& out);
void A2c_OverlayLine(char* buf, size_t bufSize);
