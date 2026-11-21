#pragma once
#include "a1b_pose_capture.h"
#include <cstddef>
#include <cstdint>

// Approach A1c — scene-wide GX position-matrix + body world-mtx capture (opt-in).
// SCR_A1C_GX_CAPTURE=1 enables; disabled by default. Read-only; no guest/FIFO mutation.
// Does not emit extra XFBs or raise unique frame counts.

constexpr int kA1cPosMtxSlots = 10;      // GX_PNMTX0..9
constexpr int kA1cMaxLoadsPerFrame = 128;
constexpr int kA1cMaxBodies = 48;

struct A1cMtxLoad
{
    uint16_t seqInFrame = 0;     // load order within the XFB/GPU frame
    uint16_t xfAddr = 0;         // XF mem word address (slot*12 for full pos mtx)
    uint8_t slot = 0;            // 0..9 when xfAddr/12 < 10
    uint8_t flags = 0;           // bit0 = in display list; bit1 = indexed XF load path
    uint32_t contentHash = 0;
    A1bMtx3x4 mtx{};
};

struct A1cBodySample
{
    uint32_t bodyEa = 0;
    uint32_t dirtyFlags = 0;
    uint32_t contentHash = 0;
    uint32_t refreshCount = 0;   // BodyGetWorldMtx hits this frame (approx)
    A1bMtx3x4 world{};
    float pos168[3]{};           // body+0x168 if readable
    bool mtxValid = false;
    bool isPlayer = false;
};

struct A1cFrameSnap
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint32_t loadCount = 0;          // total pos-mtx loads observed (may exceed stored)
    uint32_t loadStored = 0;         // number of A1cMtxLoad entries kept
    uint32_t loadsDropped = 0;       // truncated by kA1cMaxLoadsPerFrame
    uint32_t uniqueSlotsTouched = 0; // bits in slot mask
    uint32_t slotChangeMask = 0;     // vs previous frame, bit i = slot i hash changed
    uint32_t bodyCount = 0;
    uint32_t playerBodyRefreshes = 0;
    A1cMtxLoad loads[kA1cMaxLoadsPerFrame]{};
    A1bMtx3x4 slotLast[kA1cPosMtxSlots]{};
    uint32_t slotHash[kA1cPosMtxSlots]{};
    bool slotValid[kA1cPosMtxSlots]{};
    A1cBodySample bodies[kA1cMaxBodies]{};
};

bool A1c_GxCaptureEnabled();
void A1c_OnXfLoad(uint32_t addr, uint32_t count, bool indexed);
void A1c_OnBodyWorldMtx(uint32_t bodyEa, const float* mtx12, uint32_t dirtyFlags);
void A1c_OnSimFrameEnd();
void A1c_OnXfbCopy();

uint64_t A1c_FrameCount();
bool A1c_HasPair();
bool A1c_GetCurrent(A1cFrameSnap& out);
bool A1c_GetPrevious(A1cFrameSnap& out);
void A1c_OverlayLine(char* buf, size_t bufSize);
