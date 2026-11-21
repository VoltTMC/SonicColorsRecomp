#pragma once
#include "../gx/gx_state.h"
#include "a1b_pose_capture.h"
#include <cstddef>
#include <cstdint>

// Approach A3a — draw-level GX transform attribution + scratch matrix override.
// Opt-in: SCR_A3A_DRAW_ATTR=1 (capture), SCR_A3A_MATRIX_OVERRIDE=1 (scratch only).
// Disabled by default. Never mutates live XF/guest memory outside isolation.

// PNMTXIDX is a row index into XF mem (0,3,6,…). One 3×4 pos matrix = 3 rows
// = 12 floats starting at xfmem[row*4].
// A3d: quiet/early TR1 ≈ 70–200 draws; busy TR1 can exceed 30k. Store cap bounds
// attribution detail (~800 KiB/snap at 1536); overflow still counted in drawCount /
// overflowCat[] (no silent drop of observations).
constexpr int kA3aMaxDrawsPerFrame = 1536;
constexpr int kA3aMaxMtxPerDraw = 8;
constexpr int kA3aOverflowCatCount = 8;

enum A3aCorrKind : uint8_t
{
    A3aCorr_Unknown = 0,
    A3aCorr_SameObject = 1,
    A3aCorr_SameStructure = 2,
    A3aCorr_Ambiguous = 3,
    A3aCorr_Appeared = 4,
    A3aCorr_Disappeared = 5,
};

struct A3aMtxSample
{
    uint8_t rowIdx = 0;
    uint8_t pad[3]{};
    uint32_t contentHash = 0;
    float m[12]{};
};

struct A3aDrawRec
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint32_t drawSeq = 0;
    uint8_t prim = 0;
    uint8_t vat = 0;
    uint8_t inDL = 0;
    uint8_t posMtxDirect = 0;
    uint16_t vtxCount = 0;
    uint16_t mtxCount = 0;
    uint32_t vcdLo = 0;
    uint32_t vcdHi = 0;
    uint32_t arrayBase0 = 0;
    uint32_t arrayStride0 = 0;
    uint32_t geomSig = 0;
    uint32_t identityKey = 0;
    uint8_t defPosMtx = 0;
    uint8_t bestRow = 0;
    uint8_t corrKind = 0;
    uint8_t flags = 0;  // bit0 = playerCandidate
    float bestDist = 1e9f;
    float playerPos[3]{};
    bool playerValid = false;
    A3aMtxSample mtx[kA3aMaxMtxPerDraw]{};
};

struct A3aFrameSnap
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint32_t drawCount = 0;
    uint32_t drawStored = 0;
    uint32_t drawsDropped = 0;
    uint32_t uniqueRows = 0;
    uint32_t playerCandidates = 0;
    uint32_t corrSame = 0;
    uint32_t corrStruct = 0;
    uint32_t corrAmbiguous = 0;
    uint32_t corrAppeared = 0;
    uint64_t rowMask = 0;
    // Lightweight category tallies for draws that did not fit in draws[] (A3d).
    // Indices match A3dDrawCat; classified without cross-frame corr (env→static bucket).
    uint32_t overflowCat[kA3aOverflowCatCount]{};
    uint32_t overflowTotal = 0;
    A3aDrawRec draws[kA3aMaxDrawsPerFrame]{};
};

struct A3aOverrideStats
{
    uint32_t applyCount = 0;
    uint32_t xfHookCount = 0;
    uint32_t rowIdx = 0;
    uint32_t mode = 0;  // 0=off 1=noop 2=tx
    float dx = 0, dy = 0, dz = 0;
    uint32_t baselineHash = 0;
    uint32_t overriddenHash = 0;
    bool exercised = false;
    bool noopMatch = false;
    bool modifiedDiff = false;
};

bool A3a_DrawAttrEnabled();
bool A3a_MatrixOverrideEnabled();

// hostVerts: HostVertex array (pos at +0, mtx packed at HostVertex::mtx).
void A3a_OnDraw(int prim, int vat, uint32_t count, const void* hostVerts, uint32_t hostVertStride,
                const VertexLayout& layout);

void A3a_OnXfbCopy();
void A3a_OnSimFrameEnd();

void A3a_OnXfLoadIsolation(uint32_t addr, uint32_t count);
bool A3a_ApplyMatrixOverride();

void A3a_ResetOverrideStats();
A3aOverrideStats A3a_GetOverrideStats();
void A3a_NoteReplayHashes(uint32_t baseline, uint32_t overridden, bool noopMode);

bool A3a_GetCurrent(A3aFrameSnap& out);
bool A3a_GetPrevious(A3aFrameSnap& out);
void A3a_OverlayLine(char* buf, size_t bufSize);

uint8_t A3a_OverrideRow();
uint32_t A3a_OverrideMode();

// Probe helper: suspend override applies without clearing env enable.
void A3a_SuspendOverride(bool suspend);
bool A3a_OverrideSuspended();
