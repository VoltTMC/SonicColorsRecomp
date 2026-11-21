#pragma once
#include <cstddef>
#include <cstdint>

// Approach A2c/A2d/A2e — resource closure + isolated scratch-EFB replay.
// Requires SCR_A2B_FIFO_CAPTURE=1. Replay: SCR_A2C_SCRATCH_REPLAY=1 (default off).
// A2d: title/boot idle pass (~5.5 KiB / ~70 draws / ~2 deps) — MATCH baseline.
// A2e: second independent static/UI family (~12 KiB / ~82 draws / ~10 deps).
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
    A2cReady_EfbIncoming = 1u << 8,   // A2d/A2e: pass-entry EFB snapshot present
    A2cReady_All = 0x1FFu,
};

// Structural families — classified by cmd/draw/dep profile, not addresses.
enum A2cPassFamily : uint32_t
{
    A2cFamily_None = 0,
    A2cFamily_A2dIdle = 1,    // title/boot idle HUD (~5–7 KiB, ~70 draws, 1–3 deps)
    A2cFamily_A2eMultiTex = 2, // richer static/UI (~10–16 KiB, ~80+ draws, ≥6 deps)
    A2cFamily_Other = 3,      // selected but outside named families
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
    uint32_t depDl = 0;
    uint32_t depTex = 0;
    uint32_t depVert = 0;
    uint32_t depTmem = 0;
    uint32_t depOther = 0;
    uint32_t unresolvedRequired = 0;
    uint32_t readyFlags = 0;
    uint32_t failFlags = 0;
    uint32_t efbInColorHash = 0;
    uint32_t efbInDepthHash = 0;
    uint32_t family = A2cFamily_None;
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
    bool seedColorMatch = false;   // scratch after seed == incoming colour hash
    bool seedDepthMatch = false;
    uint32_t family = A2cFamily_None;
    uint32_t liveEfbHash = 0;
    uint32_t scratchEfbHash = 0;
    uint32_t liveDepthHash = 0;
    uint32_t scratchDepthHash = 0;
    uint32_t efbInColorHash = 0;
    uint32_t efbInDepthHash = 0;
    uint32_t seedColorHash = 0;
    uint32_t seedDepthHash = 0;
    uint32_t colorMismatch = 0;
    uint32_t depthMismatch = 0;
    uint32_t maxChannelDelta = 0;
    uint32_t comparedPixels = 0;
    uint32_t passHash = 0;
    uint32_t passBytes = 0;
    uint32_t drawCount = 0;
    uint32_t depCount = 0;
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
bool A2c_GetFamilyPassReport(uint32_t family, A2cPassReport& out);
bool A2c_GetFamilyReplay(uint32_t family, A2cReplayResult& out);
void A2c_OverlayLine(char* buf, size_t bufSize);

const char* A2c_FamilyName(uint32_t family);
