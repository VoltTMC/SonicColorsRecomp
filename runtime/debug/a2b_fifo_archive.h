#pragma once
#include <cstddef>
#include <cstdint>

// Approach A2b — opt-in bounded GX FIFO archive (no replay).
// SCR_A2B_FIFO_CAPTURE=1 enables; SCR_A2B_FIFO_DUMP=1 logs/exports diagnostics.
// Bytes are copied before ProcessFifo consumes them. Guest/FIFO behaviour unchanged.

constexpr uint32_t kA2bMaxSubsPerFrame = 512;
constexpr uint32_t kA2bMaxBytesPerFrame = 4u << 20;   // 4 MiB command bytes
constexpr uint32_t kA2bMaxBytesPerSub = 256u << 10;   // coalesce ring drains up to 256 KiB/sub
constexpr uint32_t kA2bMaxResources = 96;
constexpr uint32_t kA2bMaxResBytes = 1u << 20;        // 1 MiB resource snapshots
constexpr uint32_t kA2bMaxResCopy = 64u << 10;        // per-resource copy cap

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
    A2bRes_CpArrayBase = 3,   // CP_ARRAY_BASE+i inventory (size often unknown)
    A2bRes_TextureTmem = 4,   // placeholder: not snapshotted this phase
    A2bRes_EfbConfig = 5,     // placeholder inventory from BP
};

enum A2bResLifetime : uint32_t
{
    A2bResLife_Unknown = 0,
    A2bResLife_FrameLocal = 1,   // may change before next XFB
    A2bResLife_LikelyStable = 2, // address reused; contents not proven immutable
};

enum A2bResFlags : uint32_t
{
    A2bRes_Copied = 1u << 0,
    A2bRes_Truncated = 1u << 1,
    A2bRes_Unresolved = 1u << 2,   // address invalid / not copied
};

struct A2bSubmission
{
    uint64_t simFrameId = 0;
    uint32_t submitSeq = 0;      // order within the archived frame
    uint32_t byteCount = 0;
    uint32_t byteOffset = 0;     // into frame command blob
    uint32_t contentHash = 0;
    uint32_t sourceHint = 0;     // 0=GP ring/gather, 1=reserved
};

struct A2bResource
{
    uint32_t type = 0;
    uint32_t guestEa = 0;
    uint32_t size = 0;           // requested size (0 if unknown)
    uint32_t copiedSize = 0;     // bytes stored (0 if not copied)
    uint32_t contentHash = 0;
    uint32_t flags = 0;
    uint32_t lifetime = 0;       // A2bResLifetime*
    uint32_t dataOffset = 0;     // into frame resource blob
    uint32_t submitSeq = 0;      // submission when first seen (0-based)
    uint32_t aux = 0;            // stride / BP reg / array index
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
    uint32_t bpScissor0 = 0;     // BP 0x20-ish family when present
    uint32_t bpEfbAddr = 0;      // BP copy/EFB-related sample
    uint32_t posMtx0Hash = 0;
    uint32_t xfMemHash = 0;      // hash of XF pos-mtx words 0..119
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

// Lightweight public summary (no blob pointers — use dump/export for bytes).
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
    uint32_t skippedSubmissions = 0;  // overflow drops (explicit Incomplete)
    uint32_t skippedBytes = 0;
    uint32_t flags = 0;
    uint32_t commandHash = 0;    // hash of all archived command bytes
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

bool A2b_FifoCaptureEnabled();
void A2b_OnFifoSubmission(const uint8_t* data, size_t n);
void A2b_OnDisplayList(uint32_t guestEa, uint32_t size);
void A2b_OnIndexedXf(uint32_t guestEa, uint32_t byteSize);
void A2b_OnSimFrameEnd();
void A2b_OnXfbCopy();

uint64_t A2b_FrameCount();
bool A2b_HasPair();
bool A2b_GetCurrentSummary(A2bFrameSummary& out);
bool A2b_GetPreviousSummary(A2bFrameSummary& out);
void A2b_OverlayLine(char* buf, size_t bufSize);

// Optional: write last finalized frame bytes + manifest under dir (best-effort).
bool A2b_ExportLastFrame(const char* dirPath);

// Compare current vs previous finalized archives (in-memory).
bool A2b_CompareCurrPrev(A2bCompareResult& out);
