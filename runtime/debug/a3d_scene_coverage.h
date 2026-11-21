#pragma once
#include "a3a_draw_attr.h"
#include <cstddef>
#include <cstdint>

// A3d — full-scene GX archive coverage audit (opt-in diagnostics).
// SCR_A3D_SCENE_COVERAGE=1 enables. Requires SCR_A3A_DRAW_ATTR=1 for draw
// categories; SCR_A2B_FIFO_CAPTURE=1 for archive/resource accounting.
// Disabled by default. No second XFB / no unique-FPS claim / no skeletal interp.
//
// Coverage terms (not interchangeable):
//   observed     — draws/resources seen during the guest frame
//   captured     — retained in A3a/A2b buffers (may still be incomplete)
//   complete     — captured with reqUnres=0 and no archive overflow
//   replayable   — A2c selected a ready pass for this XFB (family may vary)
//   validated    — live/scratch exact MATCH for a selected family
//   interpolatable — transform-attributed + stable enough for future interp

enum A3dDrawCat : uint8_t
{
    A3dCat_Unknown = 0,
    A3dCat_HudUi = 1,          // screen-space / title HUD quads
    A3dCat_EnvStatic = 2,      // single-matrix world (stable geom across frames)
    A3dCat_EnvMoving = 3,      // single-matrix world with changing T / weak corr
    A3dCat_SkinnedPlayer = 4,  // multi-row / playerCandidate
    A3dCat_SkinnedOther = 5,   // multi-row non-player
    A3dCat_ParticleLike = 6,   // tiny non-UI draws (effects candidates)
    A3dCat_Other = 7,
};

struct A3dCatStats
{
    uint32_t observed = 0;       // classified from A3a stored draws
    uint32_t stableCorr = 0;     // SameObject | SameStructure
    uint32_t weakCorr = 0;       // Ambiguous | Appeared
    uint32_t multiRow = 0;
    uint32_t inDL = 0;
    uint32_t depthHint = 0;      // vtxCount>=3 (writes/tests geometry; not PE proof)
};

struct A3dCoverageReport
{
    uint64_t xfbIndex = 0;
    uint64_t simFrameId = 0;

    // Draw accounting (A3a)
    uint32_t drawsObserved = 0;   // A3a drawCount (all NoteDraw-equivalent)
    uint32_t drawsCaptured = 0;   // A3a drawStored
    uint32_t drawsDropped = 0;    // A3a overflow past kA3aMaxDrawsPerFrame
    uint32_t uniqueRows = 0;
    uint64_t rowMask = 0;
    uint32_t playerCandidates = 0;

    // Archive accounting (A2b) — 0 if FIFO capture off
    uint32_t a2bDraws = 0;
    uint32_t a2bBytes = 0;
    uint32_t a2bSubs = 0;
    uint32_t a2bRes = 0;
    uint32_t a2bResBytes = 0;
    uint32_t a2bRequired = 0;
    uint32_t a2bReqUnresolved = 0;
    uint32_t a2bTruncated = 0;
    uint32_t a2bFlags = 0;
    bool a2bComplete = false;     // reqUnres=0 && !(Incomplete)
    bool a2bEfbIn = false;
    uint32_t a2bEfbCopies = 0;    // from packet edge/res inventory when available

    // Category histogram: stored draws + A3a overflowCat (observed includes overflow)
    A3dCatStats cat[8]{};
    uint32_t overflowTotal = 0;
    uint32_t overflowCat[8]{};

    // A2c notes (optional)
    uint32_t lastReplayFamily = 0;
    bool lastReplayReady = false;
    bool lastReplayMatch = false;
    uint32_t lastReplayPassBytes = 0;

    // Caps / truncation
    bool a3aCapped = false;
    bool a2bCapped = false;
};

bool A3d_SceneCoverageEnabled();
A3dDrawCat A3d_ClassifyDraw(const A3aDrawRec& d, const A3aFrameSnap* prev);

void A3d_OnXfbCopy();
void A3d_NoteReplay(uint32_t family, uint32_t passBytes, bool ready, bool imageMatch);

A3dCoverageReport A3d_GetReport();
void A3d_OverlayLine(char* buf, size_t bufSize);
const char* A3d_CatName(A3dDrawCat c);
