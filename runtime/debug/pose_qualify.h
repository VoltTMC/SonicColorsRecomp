#pragma once
#include "pose_capture.h"
#include <cstddef>
#include <cstdint>

// Approach A3m — opt-in snapshot qualification (no interpolation).
// SCR_POSE_QUALIFY=1  pair + matrix-class counters on top of SCR_POSE_CAPTURE
// Off by default. No dumps, no heap, no presentation changes.

struct PoseQualifyStats
{
    uint64_t pairsExamined = 0;
    uint64_t pairsOk = 0;
    uint64_t pairsOkSameGen = 0;
    uint64_t reject[16]{};  // indexed by PosePairReject
    uint64_t classWorld[8]{};
    uint64_t classView[8]{};
    uint64_t classRoot = 0;
    uint64_t classRootRigid = 0;
    uint64_t xfbGapOk = 0;      // pair OK but xfbIndex skipped
    uint64_t unchangedPose = 0; // pair OK, worldTDelta ≈ 0
    float worldTDeltaMax = 0;
    float worldTDeltaSum = 0;
    float vHatTDeltaMax = 0;
    float vHatTDeltaSum = 0;
    float viewTDeltaMax = 0;
    float viewTDeltaSum = 0;
    // Camera-separation probe: frames where V̂ moved more than World (camera-dominated).
    uint64_t camDominated = 0;
    uint64_t poseDominated = 0;
};

bool PoseQualify_Enabled();
void PoseQualify_OnXfb();  // after PoseCapture_OnXfb
void PoseQualify_GetStats(PoseQualifyStats& out);
void PoseQualify_OverlayLine(char* buf, size_t bufSize);
