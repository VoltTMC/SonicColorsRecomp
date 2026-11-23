#pragma once
#include <cstddef>
#include <cstdint>

// Approach A3l — production-bounded player pose capture (opt-in).
// SCR_POSE_CAPTURE=1  capture coherent World+ViewPos snapshots at ~60 Hz
// Off by default. No diagnostic dumps, no heap allocs in hot path, no interpolation.

constexpr uint32_t kPoseFmtVersion = 1;

constexpr uint32_t kPoseScn_RootMtx = 0x3C;
constexpr uint32_t kPoseScn_ResMdl = 0xE8;
constexpr uint32_t kPoseScn_World = 0xEC;
constexpr uint32_t kPoseScn_ViewPos = 0xF4;
constexpr uint32_t kPoseScn_Byte100 = 0x100;
constexpr uint32_t kPoseScn_Byte101 = 0x101;
constexpr uint32_t kPoseScn_NumView = 0x102;

constexpr uint32_t kPoseResNode_MtxID = 0x10;
constexpr uint32_t kPoseResNode_ToParent = 0x5C;
constexpr uint32_t kPoseResNode_Size = 0xD0;
constexpr uint32_t kPoseResMdl_ToNodeDic = 0x14;

// Capacity: Sonic TR1 nView=193; A3h max ResNodes=256. Fail closed above this.
constexpr int kPoseMaxNodes = 256;
constexpr int kPoseRingSlots = 3;  // prev | latest | writing

// GC MTX 3×4 row-major (same as A1b / nw4r).
struct PoseMtx3x4
{
    float m[12];
};

enum PoseFlags : uint32_t
{
    PoseFlag_None = 0,
    PoseFlag_Complete = 1u << 0,
    PoseFlag_OwnOk = 1u << 1,
    PoseFlag_HierOk = 1u << 2,
    PoseFlag_WorldOk = 1u << 3,
    PoseFlag_ViewOk = 1u << 4,
    PoseFlag_VHatOk = 1u << 5,
    PoseFlag_RelOk = 1u << 6,       // ViewPos ≈ V̂·World for all nodes
    PoseFlag_Rejected = 1u << 7,
    PoseFlag_ModelChanged = 1u << 8,
    PoseFlag_Overflow = 1u << 9,
    PoseFlag_GenMismatch = 1u << 10,
    PoseFlag_Warmup = 1u << 11,     // first complete; no previous
};

enum PoseReject : uint16_t
{
    PoseReject_None = 0,
    PoseReject_NoOwner = 1,
    PoseReject_NoScn = 2,
    PoseReject_BadHier = 3,
    PoseReject_Overflow = 4,
    PoseReject_WorldIncomplete = 5,
    PoseReject_ViewIncomplete = 6,
    PoseReject_GenMismatch = 7,
    PoseReject_SingularW0 = 8,
    PoseReject_RelFail = 9,
    PoseReject_ModelChange = 10,
};

struct PoseNode
{
    uint16_t nodeIdx = 0;     // ResDic index (stable within modelGen)
    int16_t parentIdx = -1;   // ResDic parent index; -1 = root/none
    uint16_t mtxID = 0xFFFF;  // palette / World index when < nView
    uint8_t worldOk = 0;
    uint8_t viewOk = 0;
    PoseMtx3x4 world{};
    PoseMtx3x4 viewPos{};
};

struct PoseSnap
{
    uint32_t formatVersion = kPoseFmtVersion;
    uint32_t flags = PoseFlag_None;
    uint16_t reject = PoseReject_None;
    uint16_t nodeCount = 0;       // ResDic node count
    uint16_t inPaletteCount = 0;  // nodes with mtxID < nView (have World/ViewPos)
    uint16_t nodeCapacity = kPoseMaxNodes;
    uint16_t parentResolved = 0;

    uint64_t seq = 0;           // monotonic published sequence
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint32_t worldGen = 0;      // CalcWorld observations for this ScnMdl
    uint32_t viewSlot = 0;      // byte101 active ViewPos slot (0/1)
    uint32_t slotBytes = 0;

    uint32_t shellEa = 0;
    uint32_t bodyEa = 0;
    uint32_t instanceKey = 0;
    uint32_t scnMdlEa = 0;
    uint32_t resMdlEa = 0;
    uint32_t modelGen = 0;      // increments on ScnMdl/ResMdl change

    PoseMtx3x4 root{};          // ScnMdl+0x3C visual root (not body+0x0C)
    uint8_t rootOk = 0;
    uint8_t pad0[3]{};

    PoseMtx3x4 vHat{};          // ViewPos[0] · inv(World[0])
    uint8_t vHatOk = 0;
    uint8_t relOk = 0;
    uint16_t relPassCount = 0;
    float relResMax = 0;
    float relResSum = 0;

    PoseNode nodes[kPoseMaxNodes]{};
};

struct PoseStats
{
    uint64_t attempts = 0;
    uint64_t complete = 0;
    uint64_t incomplete = 0;
    uint64_t ownFail = 0;
    uint64_t hierFail = 0;
    uint64_t genMismatch = 0;
    uint64_t invalidMtx = 0;
    uint64_t relFail = 0;
    uint64_t overflow = 0;
    uint64_t ringOverwrite = 0;
    uint64_t modelChange = 0;
    uint64_t bytesCopied = 0;
    uint32_t peakNodes = 0;
};

// --- Host-testable math / ring (no guest) ------------------------------------

bool Pose_MtxInvert(const PoseMtx3x4& m, PoseMtx3x4& out);
void Pose_MtxConcat(const PoseMtx3x4& a, const PoseMtx3x4& b, PoseMtx3x4& c);
float Pose_MtxMaxAbsDiff(const PoseMtx3x4& a, const PoseMtx3x4& b);
bool Pose_MtxFinite(const PoseMtx3x4& m);
bool Pose_DeriveVHat(const PoseMtx3x4& view0, const PoseMtx3x4& world0, PoseMtx3x4& vHat);
bool Pose_ValidateNodeRel(const PoseMtx3x4& vHat, const PoseMtx3x4& world, const PoseMtx3x4& view,
                          float tol, float* outRes);

// Transform class for future TRS suitability (no interpolation performed).
enum PoseMtxClass : uint8_t
{
    PoseMtx_Invalid = 0,     // non-finite
    PoseMtx_Singular = 1,    // |det| too small
    PoseMtx_Shear = 2,       // non-orthogonal after scale removal
    PoseMtx_NonUniform = 3,  // orthogonal axes, unequal scale
    PoseMtx_UniformScale = 4,
    PoseMtx_Rigid = 5,       // near-orthonormal, |scale|≈1
    PoseMtx_Reflection = 6,  // det < 0 (may combine with scale flags via bits)
};

struct PoseMtxClassInfo
{
    PoseMtxClass cls = PoseMtx_Invalid;
    float det = 0;
    float scale[3]{};
    float orthoErr = 0;  // max |dot| of normalized columns
    uint8_t reflected = 0;
};

PoseMtxClassInfo Pose_ClassifyMtx(const PoseMtx3x4& m);

// Pair suitability (host + runtime qualify). Does not generate poses.
enum PosePairReject : uint16_t
{
    PosePair_Ok = 0,
    PosePair_MissingPrev = 1,
    PosePair_Incomplete = 2,
    PosePair_OwnMismatch = 3,
    PosePair_ScnMismatch = 4,
    PosePair_ModelGen = 5,
    PosePair_NodeCount = 6,
    PosePair_ParentTable = 7,
    PosePair_SeqGap = 8,
    PosePair_XfbGap = 9,
    PosePair_Teleport = 10,
    PosePair_CamCut = 11,
    PosePair_BadClass = 12,
    PosePair_Warmup = 13,
};

struct PosePairVerdict
{
    PosePairReject reject = PosePair_MissingPrev;
    uint16_t parentMismatches = 0;
    float worldTDelta = 0;  // |Δ translation| of World[ref]
    float vHatTDelta = 0;
    float viewTDelta = 0;
    uint8_t seqGap = 0;
    uint8_t xfbGap = 0;
};

// Teleport / camera-cut thresholds (guest world units / V̂ translation).
constexpr float kPoseTeleportWorldT = 25.f;
constexpr float kPoseCamCutVHatT = 40.f;

PosePairVerdict Pose_EvaluatePair(const PoseSnap& prev, const PoseSnap& curr);

// Synthetic ring helpers for host tests (independent of guest capture).
struct PoseRingTest
{
    PoseSnap slots[kPoseRingSlots]{};
    uint32_t write = 0;
    uint32_t count = 0;
    uint64_t published = 0;
    uint64_t overwrites = 0;
};

void PoseRing_Init(PoseRingTest& r);
bool PoseRing_Publish(PoseRingTest& r, const PoseSnap& snap);  // only if Complete
bool PoseRing_Latest(const PoseRingTest& r, PoseSnap& out);
bool PoseRing_Previous(const PoseRingTest& r, PoseSnap& out);

// --- Runtime capture ---------------------------------------------------------

bool PoseCapture_Enabled();
void PoseCapture_OnCalcWorld(uint32_t scnMdlEa);  // guest thread, after world write
void PoseCapture_OnXfb();                         // publish coherent snapshot
bool PoseCapture_GetLatest(PoseSnap& out);
bool PoseCapture_GetPrevious(PoseSnap& out);
void PoseCapture_GetStats(PoseStats& out);
void PoseCapture_OverlayLine(char* buf, size_t bufSize);

constexpr float kPoseRelTol = 0.05f;  // A3k residual ≈0.002; keep same acceptance band
