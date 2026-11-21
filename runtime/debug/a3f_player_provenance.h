#pragma once
#include "a3a_draw_attr.h"
#include "a1b_pose_capture.h"
#include <cstddef>
#include <cstdint>

// Approach A3f — player draw ownership + skeletal-palette provenance (opt-in).
// SCR_A3F_PLAYER_TRACE=1     stage-by-stage ownership / rejection report
// SCR_A3F_PALETTE_CAPTURE=1  consecutive draw-time palette ring (no interp)
// SCR_A3F_DUMP=1             denser bounded logs
// Requires SCR_A3A_DRAW_ATTR. Auto-enables A1b shell/body identity sources.
// Disabled → no hooks cost beyond existing A1b/A3a when those are off.

enum A3fStage : uint8_t
{
    A3fStage_CandidateFound = 0,
    A3fStage_GuestPlayerIdentified,
    A3fStage_RenderComponentIdentified,  // body EA + BodyGetWorldMtx refreshes
    A3fStage_DrawSubmissionAssociated,
    A3fStage_DrawOwnershipVerified,
    A3fStage_MatrixRowsIdentified,
    A3fStage_MatrixSpaceVerified,
    A3fStage_PaletteCaptured,
    A3fStage_PaletteComplete,
    A3fStage_ConsecutiveCorrespondence,
    A3fStage_Count
};

enum A3fStatus : uint8_t
{
    A3fStatus_Fail = 0,
    A3fStatus_Partial = 1,
    A3fStatus_Pass = 2,
    A3fStatus_Unverified = 3,
    A3fStatus_NA = 4,
};

enum A3fMtxKind : uint8_t
{
    A3fMtxKind_Unknown = 0,
    A3fMtxKind_ModelViewBone = 1,   // draw-time XF PNMTX = model×view bone
    A3fMtxKind_ViewOnly = 2,
    A3fMtxKind_WorldOnly = 3,
    A3fMtxKind_ObjectRoot = 4,
};

enum A3fCandVerdict : uint8_t
{
    A3fCand_Unknown = 0,
    A3fCand_ProvenPlayer = 1,
    A3fCand_Rejected = 2,
    A3fCand_Partial = 3,
};

constexpr int kA3fMaxMtx = kA3aMaxMtxPerDraw;
constexpr int kA3fRingSize = 64;
constexpr uint32_t kA3fTargetIdentity = 0x66308a11u;  // A3e candidate under test

struct A3fMtxProv
{
    uint8_t rowIdx = 0;           // PNMTXIDX / XF row start
    uint8_t kind = A3fMtxKind_Unknown;
    uint8_t usedByVtx = 0;        // 1 if ≥1 sampled vertex selects it
    uint8_t pad = 0;
    uint32_t contentHash = 0;
    uint32_t vtxRefs = 0;         // how many verts in full scan selected this row
    float m[12]{};                // draw-time XF 3×4 (model-view)
    float tEye[3]{};              // translation of m
    float distToPlayerEye = 1e9f;
};

struct A3fPaletteRec
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint64_t seq = 0;
    uint32_t playerShellEa = 0;
    uint32_t playerBodyEa = 0;
    uint32_t identityKey = 0;
    uint32_t geomSig = 0;
    uint32_t arrayBase0 = 0;
    uint32_t arrayStride0 = 0;
    uint32_t drawSeq = 0;
    uint32_t bodyRefreshCount = 0;  // BodyGetWorldMtx hits for player body this XFB
    uint16_t vtxCount = 0;
    uint16_t mtxCount = 0;
    uint16_t mtxUsedTotal = 0;      // unique PNMTXIDX across ALL verts (not just store)
    uint8_t prim = 0;
    uint8_t vat = 0;
    uint8_t inDL = 0;
    uint8_t posMtxDirect = 0;
    uint8_t paletteComplete = 0;    // store covers all used rows
    uint8_t sameInstancePrev = 0;
    uint8_t sameInstanceNext = 0;   // filled when next rec arrives
    uint8_t spaceVerified = 0;      // model-view vs player eye
    uint8_t ownership = A3fStatus_Unverified;
    uint8_t candVerdict = A3fCand_Unknown;
    float playerPos[3]{};
    float playerEye[3]{};
    float bestDist = 1e9f;
    float clusterRadius = 0.f;      // max pairwise T distance among mtx
    A3fMtxProv mtx[kA3fMaxMtx]{};
};

struct A3fFrameReport
{
    uint64_t xfbIndex = 0;
    uint64_t simFrameId = 0;
    uint8_t stage[A3fStage_Count]{};  // A3fStatus per stage
    uint32_t candidateKey = 0;
    uint32_t playerShellEa = 0;
    uint32_t playerBodyEa = 0;
    uint32_t bodyRefreshCount = 0;
    uint32_t sharedGeomPeers = 0;     // other draws same geomSig
    uint32_t sharedArrayPeers = 0;    // other draws same arrayBase0
    uint32_t skinnedCandCount = 0;
    uint32_t rejectNearMiss = 0;
    A3fCandVerdict verdict = A3fCand_Unknown;
    const char* missing = "";         // short reason when PARTIAL/FAIL
};

bool A3f_PlayerTraceEnabled();
bool A3f_PaletteCaptureEnabled();
bool A3f_NeedsPlayerIdentity();  // A1b shell hooks should run

void A3f_NotePlayerBody(uint32_t bodyEa);  // from A1b shell resolve (guest thread)
void A3f_OnBodyWorldMtx(uint32_t bodyEa, const float* mtx12, uint32_t dirtyFlags);
void A3f_OnFrame(const A3aFrameSnap& cur, const A3aFrameSnap* prev);

bool A3f_GetLatestReport(A3fFrameReport& out);
bool A3f_GetLatestPalette(A3fPaletteRec& out);
uint32_t A3f_PaletteRingCount();
bool A3f_GetPaletteAt(uint32_t idxFromNewest, A3fPaletteRec& out);

void A3f_OverlayLine(char* buf, size_t bufSize);
