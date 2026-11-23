#pragma once
#include "a3a_draw_attr.h"
#include "a1b_pose_capture.h"
#include "a3b_camera_math.h"
#include <cstddef>
#include <cstdint>

// Approach A3g — player multi-draw pose aggregation + hierarchy reconstruction (opt-in).
// SCR_A3G_POSE_AGGREGATE=1   collect all player-near skeletal draws into one pose
// SCR_A3G_HIERARCHY_TRACE=1  view/body/root analysis + unresolved parent links
// SCR_A3G_POSE_DUMP=1        denser bounded logs
// Requires SCR_A3A_DRAW_ATTR; auto-enables A1b identity (via A3f NeedsPlayerIdentity pattern).
// No interpolation. Disabled → zero cost.

// Busy TR1 observes ~600–700 eye-near skeletal pieces. A3g kept nearest-128
// (≈518 overflow). A3h raises caps so the full contributor set fits with margin;
// any remaining overflow is still counted explicitly.
constexpr int kA3gMaxDraws = 768;
constexpr int kA3gMaxObs = 2048;
constexpr int kA3gRingSize = 8;  // large snaps; keep ring modest
// View-space distance from V·playerPos. Player mesh pieces cluster ~0.3–5;
// 12 keeps the character without sweeping the whole near field (80 pulled 600+ draws).
constexpr float kA3gEyeNear = 12.f;

enum A3gStatus : uint8_t
{
    A3gStatus_Fail = 0,
    A3gStatus_Partial = 1,
    A3gStatus_Pass = 2,
    A3gStatus_Unverified = 3,
    A3gStatus_NA = 4,
};

enum A3gObsFlags : uint8_t
{
    A3gObs_None = 0,
    A3gObs_DuplicateRow = 1u << 0,   // same XF row seen in another draw this frame
    A3gObs_DuplicateHash = 1u << 1,  // same contentHash as another obs (not proof of identity)
    A3gObs_RootCandidate = 1u << 2,  // ≈ View * BodyWorld (when both valid)
    A3gObs_Conflict = 1u << 3,       // same (structKey,row) with different hash same frame
};

enum A3gInterpClass : uint8_t
{
    A3gInterp_Unknown = 0,
    A3gInterp_Ready = 1,       // finite rigid-ish + temporally matched
    A3gInterp_Unsupported = 2, // shear/reflect/singular/nonfinite
    A3gInterp_Unmatched = 3,   // ok locally but no consecutive match
};

struct A3gDrawRef
{
    uint32_t drawSeq = 0;
    uint32_t identityKey = 0;  // A3a (unstable under anim)
    uint32_t structKey = 0;    // format/array/rows (no geomSig)
    uint32_t geomSig = 0;
    uint32_t arrayBase0 = 0;
    uint16_t vtxCount = 0;
    uint16_t mtxCount = 0;
    uint8_t prim = 0;
    uint8_t vat = 0;
    uint8_t inDL = 0;
    uint8_t posMtxDirect = 0;
    float bestDist = 1e9f;
    uint16_t obsBegin = 0;
    uint16_t obsCount = 0;
};

struct A3gMtxObs
{
    uint32_t structKey = 0;
    uint32_t obsKey = 0;        // structKey × row — temporal correspondence key
    uint32_t contentHash = 0;
    uint32_t drawSeq = 0;
    uint8_t rowIdx = 0;
    uint8_t flags = 0;          // A3gObsFlags
    uint8_t interp = A3gInterp_Unknown;
    uint8_t pad = 0;
    uint32_t failBits = 0;      // A3bInterpFail bits from decompose
    float m[12]{};              // draw-time model×view
    float modelApprox[12]{};    // inv(V)*M when view valid; else zero
    float tEye[3]{};
    float distToPlayerEye = 1e9f;
    uint8_t modelApproxValid = 0;
    uint8_t matchedPrev = 0;
    uint8_t parentResolved = 0; // 1 if parent link known (root→view only today)
    uint8_t pad2 = 0;
};

struct A3gPoseSnap
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint64_t seq = 0;
    uint32_t playerInstanceKey = 0;  // hash(shell,body) — primary identity
    uint32_t playerShellEa = 0;
    uint32_t playerBodyEa = 0;
    uint32_t bodyRefreshCount = 0;

    uint16_t drawCount = 0;
    uint16_t drawOverflow = 0;
    uint16_t obsCount = 0;
    uint16_t obsOverflow = 0;
    uint16_t uniqueObsKeys = 0;
    uint16_t duplicateRowCount = 0;
    uint16_t duplicateHashCount = 0;
    uint16_t conflictCount = 0;
    uint16_t unresolvedParents = 0;
    uint16_t rootCandidates = 0;
    uint16_t interpReady = 0;
    uint16_t interpUnsupported = 0;
    uint16_t interpUnmatched = 0;

    uint8_t playerValid = 0;
    uint8_t viewValid = 0;
    uint8_t bodyWorldValid = 0;
    uint8_t hierarchyPartial = 0;  // view+body known; bone parents unresolved
    uint8_t poseComplete = 0;      // all contributing draws stored, no overflow/conflict
    uint8_t sameInstancePrev = 0;
    uint8_t sameInstanceNext = 0;
    uint8_t completeness = A3gStatus_Unverified;

    float playerPos[3]{};
    float playerEye[3]{};
    A3bMtx3x4 view{};
    A3bMtx3x4 bodyWorld{};

    A3gDrawRef draws[kA3gMaxDraws]{};
    A3gMtxObs obs[kA3gMaxObs]{};
};

bool A3g_PoseAggregateEnabled();
bool A3g_HierarchyTraceEnabled();
bool A3g_NeedsPlayerIdentity();

void A3g_OnFrame(const A3aFrameSnap& cur, const A3aFrameSnap* prev);

bool A3g_GetLatest(A3gPoseSnap& out);
// Lightweight identity for A3h (avoids copying the full multi-hundred-KiB snap).
bool A3g_GetLatestIdentity(uint32_t* outInst, uint32_t* outShell, uint32_t* outBody,
                           uint16_t* outObsCount);
// Same-thread peek at the snap A3g_OnFrame just built (valid until next A3g_OnFrame).
const A3gPoseSnap* A3g_PeekLastBuild();
uint32_t A3g_RingCount();
bool A3g_GetAt(uint32_t idxFromNewest, A3gPoseSnap& out);

void A3g_OverlayLine(char* buf, size_t bufSize);
