#pragma once
#include "../gx/gx_state.h"
#include <cstddef>
#include <cstdint>
#include <vector>

// Approach A2b — opt-in bounded GX FIFO archive (no replay).
// SCR_A2B_FIFO_CAPTURE=1 enables; SCR_A2B_FIFO_DUMP=1 logs/exports diagnostics.
// Bytes are copied before ProcessFifo consumes them. Guest/FIFO behaviour unchanged.
// A2c extends resource closure + readiness; see a2c_scratch_replay.h.

constexpr uint32_t kA2bMaxSubsPerFrame = 512;
constexpr uint32_t kA2bMaxBytesPerFrame = 4u << 20;   // 4 MiB command bytes
constexpr uint32_t kA2bMaxBytesPerSub = 256u << 10;   // coalesce ring drains up to 256 KiB/sub
constexpr uint32_t kA2bMaxResources = 256;            // unique resources (deduped)
constexpr uint32_t kA2bMaxResBytes = 4u << 20;        // 4 MiB resource snapshots
constexpr uint32_t kA2bMaxResCopy = 256u << 10;       // per-resource copy cap
constexpr uint32_t kA2bTmemSnapshotBytes = 64u << 10; // TLUT-relevant TMEM window

enum A2bFrameFlags : uint32_t
{
    A2bFrame_OverflowBytes = 1u << 0,
    A2bFrame_OverflowSubs = 1u << 1,
    A2bFrame_OverflowResources = 1u << 2,
    A2bFrame_OverflowResBytes = 1u << 3,
    A2bFrame_Incomplete = 1u << 4,   // any overflow / truncated resource
};

enum A2bResType : uint32_t
{
    A2bRes_DisplayList = 1,
    A2bRes_IndexedXf = 2,
    A2bRes_CpArrayBase = 3,   // inventory-only (unknown extent)
    A2bRes_VertexArray = 6,   // sized from draw index range
    A2bRes_Texture = 7,       // guest texture source bytes
    A2bRes_TlutRam = 8,       // TLUT source in guest RAM
    A2bRes_TmemWindow = 9,    // snapshot of TMEM used by CI
    A2bRes_EfbCopy = 10,      // A2f: non-toXFB EFB→tex producer (metadata; result via replay)
};

enum A2bEfbCopyFlags : uint32_t
{
    A2bEfbCopy_Half = 1u << 0,
    A2bEfbCopy_DepthSrc = 1u << 1,
    A2bEfbCopy_Intensity = 1u << 2,
    A2bEfbCopy_Sampled = 1u << 3,  // GetTexture later consumed this dest
    A2bEfbCopy_CrossFrame = 1u << 4, // sampled in a later guest/present frame than create
};

enum A2bResLifetime : uint32_t
{
    A2bResLife_Unknown = 0,
    A2bResLife_FrameLocal = 1,
    A2bResLife_LikelyStable = 2,
};

enum A2bResFlags : uint32_t
{
    A2bRes_Copied = 1u << 0,
    A2bRes_Truncated = 1u << 1,
    A2bRes_Unresolved = 1u << 2,
    A2bRes_Required = 1u << 3,   // referenced by a draw/tex bind this frame
};

struct A2bSubmission
{
    uint64_t simFrameId = 0;
    uint32_t submitSeq = 0;
    uint32_t byteCount = 0;
    uint32_t byteOffset = 0;
    uint32_t contentHash = 0;
    uint32_t sourceHint = 0;
};

struct A2bResource
{
    uint32_t type = 0;
    uint32_t guestEa = 0;
    uint32_t size = 0;
    uint32_t copiedSize = 0;
    uint32_t contentHash = 0;
    uint32_t flags = 0;
    uint32_t lifetime = 0;
    uint32_t dataOffset = 0;
    uint32_t submitSeq = 0;
    uint32_t aux = 0;            // stride / array idx / fmt|(w<<8)|(h<<20)
    uint32_t refCount = 0;       // how often referenced (dedupe)
    // A2g: EFB-copy generation identity (0 = not applicable)
    uint64_t genId = 0;
    uint64_t createXfb = 0;
    uint32_t createFrame = 0;
};

struct A2bGxStateSnap
{
    float xfProj[7]{};
    float xfViewport[6]{};
    uint32_t cpMatIndexA = 0;
    uint32_t cpMatIndexB = 0;
    uint32_t cpVcdLo = 0;
    uint32_t cpVcdHi = 0;
    uint32_t cpVatA0 = 0;
    uint32_t bpScissor0 = 0;
    uint32_t bpEfbAddr = 0;
    uint32_t posMtx0Hash = 0;
    uint32_t xfMemHash = 0;
    bool valid = false;
};

struct A2bContextSnap
{
    uint64_t a1bSeq = 0;
    uint64_t a1cXfb = 0;
    uint32_t a1cLoads = 0;
    uint32_t a1cBodies = 0;
    uint32_t playerBodyEa = 0;
    float playerPos[3]{};
    bool playerValid = false;
    bool a1bValid = false;
    bool a1cValid = false;
};

struct A2bFrameSummary
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint32_t submissionCount = 0;
    uint32_t totalBytes = 0;
    uint32_t resourceCount = 0;
    uint32_t resourceBytes = 0;
    uint32_t unresolvedResources = 0;
    uint32_t truncatedResources = 0;
    uint32_t skippedSubmissions = 0;
    uint32_t skippedBytes = 0;
    uint32_t drawCount = 0;
    uint32_t requiredResources = 0;
    uint32_t requiredUnresolved = 0;
    uint32_t flags = 0;
    uint32_t commandHash = 0;
    bool gxStartValid = false;
    bool efbIncomingValid = false;
    int efbIncomingSlot = -1;          // GPU snapshot handle (renderer-owned)
    uint32_t efbInColorHash = 0;
    uint32_t efbInDepthHash = 0;
    uint32_t efbInW = 0;
    uint32_t efbInH = 0;
    A2bGxStateSnap gx{};
    A2bContextSnap ctx{};
};

struct A2bCompareResult
{
    bool bothValid = false;
    bool commandEqual = false;
    bool gxEqual = false;
    bool ctxPlayerEqual = false;
    uint32_t subCountDelta = 0;
    uint32_t byteDelta = 0;
    uint32_t resCountDelta = 0;
};

// A2g: sample of an EFB generation created in an earlier present/sim frame.
struct A2bCrossEfbEdge
{
    uint32_t destAddr = 0;
    uint64_t genId = 0;
    uint32_t createFrame = 0;
    uint64_t createXfb = 0;
    uint64_t sampleXfb = 0;
    uint32_t sampleCount = 0;
};

// Full archive clone for A2c readiness/replay (deep copy of blobs).
struct A2bArchivePacket
{
    A2bFrameSummary summary{};
    GXState gxStart{};
    bool gxStartValid = false;
    int efbIncomingSlot = -1;
    bool efbIncomingValid = false;
    uint32_t efbInColorHash = 0;
    uint32_t efbInDepthHash = 0;
    std::vector<uint8_t> cmd;
    std::vector<uint8_t> resData;
    std::vector<uint8_t> tmemStart;   // optional TMEM window at frame start
    A2bSubmission subs[kA2bMaxSubsPerFrame]{};
    A2bResource resources[kA2bMaxResources]{};
    A2bCrossEfbEdge crossEdges[64]{};
    uint32_t submitCount = 0;
    uint32_t resCount = 0;
    uint32_t crossEdgeCount = 0;
};

bool A2b_FifoCaptureEnabled();
void A2b_OnFifoSubmission(const uint8_t* data, size_t n);
void A2b_OnDisplayList(uint32_t guestEa, uint32_t size);
void A2b_OnIndexedXf(uint32_t guestEa, uint32_t byteSize);
void A2b_OnVertexArray(uint32_t guestEa, uint32_t size, uint32_t stride, uint32_t arrayIndex);
void A2b_OnTexture(uint32_t guestEa, uint32_t size, uint32_t fmt, uint32_t w, uint32_t h);
void A2b_OnTlutRam(uint32_t guestEa, uint32_t size);
void A2b_OnTmemWindow(uint32_t tmemOff, uint32_t size, const uint8_t* data);
// A2f/A2g: non-toXFB EFB copy. aux packs sx|sy|sw|sh; size packs sh|fmt.
// genId/createFrame/createXfb identify the producer generation across frames.
void A2b_OnEfbCopy(uint32_t destAddr, uint32_t ctl, uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh,
                   uint32_t fmt, uint32_t copyFlags, uint64_t genId, uint32_t createFrame,
                   uint64_t createXfb);
void A2b_OnEfbCopySampled(uint32_t destAddr, uint64_t genId, uint32_t createFrame, uint64_t createXfb,
                          bool crossFrame);
void A2b_NoteDraw();
void A2b_OnSimFrameEnd();
void A2b_OnXfbCopy();
// Call after post-XFB EFB clear (pass-entry state for the next archived frame).
void A2b_OnPassEntryEfb();
uint32_t A2b_CountEfbCopiesInCmd(const uint8_t* cmd, uint32_t n);  // non-toXFB BP 0x52 count

uint64_t A2b_FrameCount();
bool A2b_HasPair();
bool A2b_GetCurrentSummary(A2bFrameSummary& out);
bool A2b_GetPreviousSummary(A2bFrameSummary& out);
bool A2b_CloneCurrentPacket(A2bArchivePacket& out);
bool A2b_ClonePreviousPacket(A2bArchivePacket& out);
uint32_t A2b_GetCrossEfbEdgeCount();
bool A2b_GetCrossEfbEdge(uint32_t index, A2bCrossEfbEdge& out);
void A2b_OverlayLine(char* buf, size_t bufSize);

bool A2b_ExportLastFrame(const char* dirPath);
bool A2b_CompareCurrPrev(A2bCompareResult& out);

// Find byte offset of first BP write to reg 0x52 (XFB/EFB copy trigger); n if none.
uint32_t A2b_FindFirstBp52(const uint8_t* cmd, uint32_t n);
