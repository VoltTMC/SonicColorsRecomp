// Approach A2b — bounded GX FIFO archive before ProcessFifo consumes bytes.
// A2c resource closure hooks (vertex/texture) feed the same archive.
#include "a2b_fifo_archive.h"
#include "a1b_pose_capture.h"
#include "a1c_gx_capture.h"
#include "a2c_scratch_replay.h"
#include "a2g_natural_trace.h"
#include "a3e_budgets.h"
#include "../gx/gx_state.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../perf.h"
#include "../platform/platform.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

bool EnvFlag(const char* name)
{
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

bool ProdMode()
{
    static int c = -1;
    if (c < 0)
    {
        c = (EnvFlag("SCR_A2B_PROD") || EnvFlag("SCR_DECOUPLED_PRESENT")) ? 1 : 0;
        if (c)
            LOG_INFO("A2b: production lean capture (SCR_A2B_PROD/DECOUPLED); {}",
                     A3e_BudgetSummary());
    }
    return c != 0;
}

bool ProfileOn()
{
    static int c = -1;
    if (c < 0)
        c = EnvFlag("SCR_A2B_PROFILE") ? 1 : 0;
    return c != 0;
}

bool EnvOn()
{
    static int c = -1;
    if (c < 0)
    {
        // A3p: DECOUPLED/PROD auto-enables capture so mid-replay has an archive.
        c = (EnvFlag("SCR_A2B_FIFO_CAPTURE") || ProdMode()) ? 1 : 0;
        if (c && EnvFlag("SCR_A2B_FIFO_CAPTURE"))
            LOG_INFO("A2b: FIFO archive enabled (SCR_A2B_FIFO_CAPTURE); {}", A3e_BudgetSummary());
    }
    return c != 0;
}

bool CaptureActive()
{
    return EnvOn() && A3e_CaptureArmed();
}

std::atomic<uint64_t> s_profFifoAppendNs{ 0 }, s_profFifoAppendBytes{ 0 }, s_profFifoAppendCalls{ 0 };
std::atomic<uint64_t> s_profResFindNs{ 0 }, s_profResCopyNs{ 0 }, s_profResHashNs{ 0 };
std::atomic<uint64_t> s_profResCacheHits{ 0 }, s_profResCacheMiss{ 0 }, s_profResCopiedBytes{ 0 };
std::atomic<uint64_t> s_profXfbFinalizeNs{ 0 }, s_profCmdHashNs{ 0 };
std::atomic<uint64_t> s_profMidRefreshNs{ 0 }, s_profMidRefreshBytes{ 0 }, s_profMidPinBlock{ 0 };
std::atomic<uint64_t> s_profEfbInNs{ 0 }, s_profMetaEvalNs{ 0 }, s_profIdxArrNs{ 0 };
std::atomic<uint64_t> s_profFrames{ 0 };
std::atomic<uint32_t> s_profPeakCmd{ 0 }, s_profPeakResBytes{ 0 }, s_profPeakResCount{ 0 };
// Soft-arm draw observation when EnvOn but not yet CaptureActive (no A3a required).
std::atomic<uint32_t> s_obsDraws{ 0 };
// Bit i set ⇒ CP array slot i already has a kMaxSpan covering copy this frame.
std::atomic<uint32_t> s_vertSatMask{ 0 };
constexpr uint32_t kVertSatSpan = 64u << 10;

uint64_t NowNs() { return Platform_PerfCounter(); }

struct ProfScope
{
    std::atomic<uint64_t>* acc = nullptr;
    uint64_t t0 = 0;
    explicit ProfScope(std::atomic<uint64_t>* a) : acc(a)
    {
        if (ProfileOn() && acc) t0 = NowNs();
    }
    ~ProfScope()
    {
        if (ProfileOn() && acc && t0)
            acc->fetch_add(NowNs() - t0, std::memory_order_relaxed);
    }
};

bool DumpOn()
{
    static int c = -1;
    if (c < 0)
    {
        const char* v = std::getenv("SCR_A2B_FIFO_DUMP");
        c = (v && v[0] && v[0] != '0') ? 1 : 0;
    }
    return c != 0;
}

uint32_t FnvUpdate(uint32_t h, const uint8_t* p, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

uint32_t HashBytes(const uint8_t* p, size_t n)
{
    const uint32_t h = FnvUpdate(2166136261u, p, n);
    return h ? h : 1u;
}

// A3p: cheap content identity for blob-cache reuse (head/mid/tail). Full HashBytes
// remains for non-prod MATCH diagnostics.
uint32_t SampleFingerprint(const uint8_t* p, uint32_t n)
{
    if (!p || n == 0) return 1u;
    if (n <= 1024u) return HashBytes(p, n);
    uint32_t h = FnvUpdate(2166136261u, p, 256);
    h = FnvUpdate(h, p + (n / 2) - 128, 256);
    h = FnvUpdate(h, p + n - 256, 256);
    h ^= n * 2654435761u;
    return h ? h : 1u;
}

uint32_t CanonGuestEa(uint32_t ea)
{
    if (!ea) return 0;
    // Texture/EFB dest addrs are often raw phys (MEM1 <0x01800000, MEM2 ≥0x10000000).
    const uint32_t hi = ea >> 28;
    if (hi == 0x8 || hi == 0x9 || hi == 0xC || hi == 0xD)
        return ea;
    return 0x80000000u | (ea & 0x1FFFFFFFu);
}

bool GuestPtrOk(uint32_t ea)
{
    ea = CanonGuestEa(ea);
    if (!ea) return false;
    const uint32_t hi = ea >> 28;
    return hi == 0x8 || hi == 0x9 || hi == 0xC || hi == 0xD;
}

// Bytes readable from eaCanon without crossing MEM1/MEM2 bounds (0 if invalid).
uint32_t GuestCopyableBytes(uint32_t eaCanon)
{
    eaCanon = CanonGuestEa(eaCanon);
    if (!GuestPtrOk(eaCanon)) return 0;
    const uint32_t phys = eaCanon & 0x3FFFFFFFu;
    if (phys < MEM1_SIZE) return MEM1_SIZE - phys;
    if (phys >= 0x10000000u && phys < 0x10000000u + MEM2_SIZE)
        return (0x10000000u + MEM2_SIZE) - phys;
    return 0;
}

constexpr uint32_t kA2bMaxCrossEdges = 64;

struct FrameBuild
{
    uint64_t simFrameId = 0;
    uint64_t xfbIndex = 0;
    uint32_t flags = 0;
    uint32_t submitCount = 0;
    uint32_t totalBytes = 0;
    uint32_t resCount = 0;
    uint32_t resBytes = 0;
    uint32_t unresolved = 0;
    uint32_t truncated = 0;
    uint32_t skippedSubs = 0;
    uint32_t skippedBytes = 0;
    uint32_t rejectedResources = 0;
    uint32_t drawCount = 0;
    uint32_t requiredResources = 0;
    uint32_t requiredUnresolved = 0;
    bool gxStartValid = false;
    GXState gxStart{};
    int efbIncomingSlot = -1;
    bool efbIncomingValid = false;
    uint32_t efbInColorHash = 0;
    uint32_t efbInDepthHash = 0;
    uint32_t efbInW = 0;
    uint32_t efbInH = 0;
    std::vector<uint8_t> tmemStart;
    std::vector<A2bSubmission> subs;
    std::vector<A2bResource> resources;
    A2bGxStateSnap gx{};
    A2bContextSnap ctx{};
    std::vector<uint8_t> cmd;
    std::vector<uint8_t> resData;
    A2bCrossEfbEdge crossEdges[kA2bMaxCrossEdges]{};
    uint32_t crossEdgeCount = 0;
};

void EnsureBuildCapsLocked(FrameBuild& f)
{
    const uint32_t maxSubs = A2b_CapMaxSubs();
    const uint32_t maxRes = A2b_CapMaxResources();
    if (f.subs.capacity() < maxSubs) f.subs.reserve(maxSubs);
    if (f.resources.capacity() < maxRes) f.resources.reserve(maxRes);
    if (f.subs.size() < maxSubs) f.subs.resize(maxSubs);
    if (f.resources.size() < maxRes) f.resources.resize(maxRes);
}

std::mutex s_mu;
FrameBuild s_build{};
FrameBuild s_curr{};
FrameBuild s_prev{};
bool s_haveCurr = false, s_havePrev = false;
std::atomic<uint64_t> s_frames{ 0 };
std::atomic<uint64_t> s_simTag{ 0 };

// A3p: resource index for O(1) dedupe within a frame.
struct ResKey
{
    uint32_t type = 0, ea = 0, size = 0, aux = 0;
    bool operator==(const ResKey& o) const
    {
        return type == o.type && ea == o.ea && size == o.size && aux == o.aux;
    }
};
struct ResKeyHash
{
    size_t operator()(const ResKey& k) const
    {
        size_t h = k.type;
        h = h * 1315423911u + k.ea;
        h = h * 1315423911u + k.size;
        h = h * 1315423911u + k.aux;
        return h;
    }
};
std::unordered_map<ResKey, uint32_t, ResKeyHash> s_resIndex;

// Cross-frame immutable blob cache (textures / TLUT / large copies).
// Keyed by guest identity; sample fingerprint detects mutation/reuse of the EA.
struct BlobKey
{
    uint32_t type = 0, ea = 0, size = 0, aux = 0;
    bool operator==(const BlobKey& o) const
    {
        return type == o.type && ea == o.ea && size == o.size && aux == o.aux;
    }
};
struct BlobKeyHash
{
    size_t operator()(const BlobKey& k) const
    {
        size_t h = k.type;
        h = h * 1315423911u + k.ea;
        h = h * 1315423911u + k.size;
        h = h * 1315423911u + k.aux;
        return h;
    }
};
struct BlobEnt
{
    std::vector<uint8_t> data;
    uint32_t sampleFp = 0;
    uint32_t lastXfb = 0;
};
std::unordered_map<BlobKey, BlobEnt, BlobKeyHash> s_blobCache;
constexpr size_t kBlobCacheMaxEntries = 1024;
constexpr uint32_t kBlobCacheMinBytes = 2048; // cache medium+ blobs

// Within-frame covering index (type+ea+aux) so growing VertexArray spans reuse.
struct CoverKey
{
    uint32_t type = 0, ea = 0, aux = 0;
    bool operator==(const CoverKey& o) const
    {
        return type == o.type && ea == o.ea && aux == o.aux;
    }
};
struct CoverKeyHash
{
    size_t operator()(const CoverKey& k) const
    {
        size_t h = k.type;
        h = h * 1315423911u + k.ea;
        h = h * 1315423911u + k.aux;
        return h;
    }
};
std::unordered_map<CoverKey, uint32_t, CoverKeyHash> s_resCover;

// Mid-replay pinned packet (A3p).
A2bArchivePacket s_midPkt{};
bool s_midValid = false;
bool s_midDirty = false;  // A3p: lazy clone on first Pin after XFB
std::atomic<int> s_midPin{ 0 };
uint32_t s_peakCmdReserve = 256u * 1024u;
uint32_t s_peakResReserve = 1u << 20;

void ReleaseEfbSlot(FrameBuild& f)
{
    if (f.efbIncomingSlot >= 0)
    {
        Render_EfbIncomingRelease(f.efbIncomingSlot);
        f.efbIncomingSlot = -1;
        f.efbIncomingValid = false;
    }
}

void ResetBuild(FrameBuild& f)
{
    ReleaseEfbSlot(f);
    f = FrameBuild{};
    f.cmd.clear();
    f.resData.clear();
    f.tmemStart.clear();
    f.cmd.reserve(s_peakCmdReserve);
    f.resData.reserve(s_peakResReserve);
    s_resIndex.clear();
    s_resIndex.reserve(1024);
    s_resCover.clear();
    s_resCover.reserve(1024);
    s_vertSatMask.store(0, std::memory_order_relaxed);
}

void SnapGxLite(A2bGxStateSnap& g)
{
    for (int i = 0; i < 7; i++)
        g.xfProj[i] = XFFloat(0x1000u + XF_PROJECTION + (uint32_t)i);
    for (int i = 0; i < 6; i++)
        g.xfViewport[i] = XFFloat(0x1000u + XF_VIEWPORT + (uint32_t)i);
    g.cpMatIndexA = g_gx.cp[CP_MATINDEX_A];
    g.cpMatIndexB = g_gx.cp[CP_MATINDEX_B];
    g.cpVcdLo = g_gx.cp[CP_VCD_LO];
    g.cpVcdHi = g_gx.cp[CP_VCD_HI];
    g.cpVatA0 = g_gx.cp[CP_VAT_A];
    g.bpScissor0 = g_gx.bp[0x20];
    g.bpEfbAddr = g_gx.bp[0x64];
    uint8_t tmp[48];
    for (int i = 0; i < 12; i++)
    {
        uint32_t u = g_gx.xfmem[i];
        tmp[i * 4 + 0] = (uint8_t)(u >> 24);
        tmp[i * 4 + 1] = (uint8_t)(u >> 16);
        tmp[i * 4 + 2] = (uint8_t)(u >> 8);
        tmp[i * 4 + 3] = (uint8_t)u;
    }
    g.posMtx0Hash = HashBytes(tmp, sizeof(tmp));
    uint8_t wide[120 * 4];
    for (int i = 0; i < 120; i++)
    {
        uint32_t u = g_gx.xfmem[i];
        wide[i * 4 + 0] = (uint8_t)(u >> 24);
        wide[i * 4 + 1] = (uint8_t)(u >> 16);
        wide[i * 4 + 2] = (uint8_t)(u >> 8);
        wide[i * 4 + 3] = (uint8_t)u;
    }
    g.xfMemHash = HashBytes(wide, sizeof(wide));
    g.valid = true;
}

void SnapContext(A2bContextSnap& c)
{
    A1bPoseSnap ply{};
    if (A1b_GetCurrent(ply))
    {
        c.a1bValid = true;
        c.a1bSeq = ply.seq;
        c.playerValid = ply.playerValid;
        c.playerBodyEa = ply.playerBodyEa;
        c.playerPos[0] = ply.playerPos[0];
        c.playerPos[1] = ply.playerPos[1];
        c.playerPos[2] = ply.playerPos[2];
    }
    A1cFrameSnap gx{};
    if (A1c_GetCurrent(gx))
    {
        c.a1cValid = true;
        c.a1cXfb = gx.xfbIndex;
        c.a1cLoads = gx.loadCount;
        c.a1cBodies = gx.bodyCount;
    }
}

void FillSummary(const FrameBuild& f, A2bFrameSummary& out)
{
    out = A2bFrameSummary{};
    out.simFrameId = f.simFrameId;
    out.xfbIndex = f.xfbIndex;
    out.submissionCount = f.submitCount;
    out.totalBytes = f.totalBytes;
    out.resourceCount = f.resCount;
    out.resourceBytes = f.resBytes;
    out.unresolvedResources = f.unresolved;
    out.truncatedResources = f.truncated;
    out.skippedSubmissions = f.skippedSubs;
    out.skippedBytes = f.skippedBytes;
    out.rejectedResources = f.rejectedResources;
    out.drawCount = f.drawCount;
    out.requiredResources = f.requiredResources;
    out.requiredUnresolved = f.requiredUnresolved;
    out.flags = f.flags;
    out.capResources = A2b_CapMaxResources();
    out.capResBytes = A2b_CapMaxResBytes();
    if (!ProdMode())
    {
        ProfScope ps(&s_profCmdHashNs);
        out.commandHash = f.cmd.empty() ? 0 : HashBytes(f.cmd.data(), f.cmd.size());
    }
    else
        out.commandHash = 0;
    out.gxStartValid = f.gxStartValid;
    out.efbIncomingValid = f.efbIncomingValid;
    out.efbIncomingSlot = f.efbIncomingSlot;
    out.efbInColorHash = f.efbInColorHash;
    out.efbInDepthHash = f.efbInDepthHash;
    out.efbInW = f.efbInW;
    out.efbInH = f.efbInH;
    out.gx = f.gx;
    out.ctx = f.ctx;
}

void RecountRequired(FrameBuild& f)
{
    f.requiredResources = 0;
    f.requiredUnresolved = 0;
    for (uint32_t i = 0; i < f.resCount; i++)
    {
        if (!(f.resources[i].flags & A2bRes_Required)) continue;
        f.requiredResources++;
        if (f.resources[i].flags & A2bRes_Unresolved)
            f.requiredUnresolved++;
        if (f.resources[i].flags & A2bRes_Truncated)
            f.requiredUnresolved++;
    }
}

int FindResource(FrameBuild& f, uint32_t type, uint32_t ea, uint32_t size, uint32_t aux)
{
    ProfScope ps(&s_profResFindNs);
    (void)f;
    const ResKey key{ type, ea, size, aux };
    auto it = s_resIndex.find(key);
    if (it == s_resIndex.end()) return -1;
    return (int)it->second;
}

void EnsureGxStartLocked()
{
    if (s_build.gxStartValid) return;
    s_build.gxStart = g_gx;
    s_build.gxStartValid = true;
}

bool CoverableType(uint32_t type)
{
    return type == A2bRes_VertexArray || type == A2bRes_IndexedXf || type == A2bRes_Texture ||
           type == A2bRes_TlutRam || type == A2bRes_DisplayList;
}

void AddResourceLocked(uint32_t type, uint32_t ea, uint32_t size, bool tryCopy,
                       uint32_t lifetime, uint32_t aux, bool required)
{
    const uint32_t eaCanon = CanonGuestEa(ea);

    // Exact key hit.
    int existing = FindResource(s_build, type, eaCanon, size, aux);
    if (existing >= 0)
    {
        A2bResource& er = s_build.resources[existing];
        er.refCount++;
        if (required) er.flags |= A2bRes_Required;
        return;
    }

    // Covering hit: prior larger-or-equal span for same guest identity.
    if (CoverableType(type) && size > 0)
    {
        auto cit = s_resCover.find(CoverKey{ type, eaCanon, aux });
        if (cit != s_resCover.end())
        {
            const uint32_t cidx = cit->second;
            if (cidx < s_build.resCount)
            {
                A2bResource& er = s_build.resources[cidx];
                if (er.type == type && er.guestEa == eaCanon && er.size >= size &&
                    (er.flags & A2bRes_Copied))
                {
                    er.refCount++;
                    if (required) er.flags |= A2bRes_Required;
                    if (type == A2bRes_VertexArray && er.copiedSize >= kVertSatSpan)
                    {
                        const uint32_t slot = aux & 0xFF;
                        if (slot < 12)
                            s_vertSatMask.fetch_or(1u << slot, std::memory_order_relaxed);
                    }
                    return;
                }
            }
        }
    }

    EnsureBuildCapsLocked(s_build);
    if (s_build.resCount >= A2b_CapMaxResources())
    {
        s_build.flags |= A2bFrame_OverflowResources | A2bFrame_Incomplete;
        s_build.rejectedResources++;
        return;
    }

    const uint32_t idx = s_build.resCount++;
    A2bResource& r = s_build.resources[idx];
    r = A2bResource{};
    r.type = type;
    r.guestEa = eaCanon;
    r.size = size;
    r.lifetime = lifetime;
    r.aux = aux;
    r.refCount = 1;
    r.submitSeq = s_build.submitCount > 0 ? s_build.submitCount - 1 : 0;
    r.dataOffset = (uint32_t)s_build.resData.size();
    if (required) r.flags |= A2bRes_Required;
    s_resIndex.insert_or_assign(ResKey{ type, eaCanon, size, aux }, idx);
    if (CoverableType(type))
        s_resCover.insert_or_assign(CoverKey{ type, eaCanon, aux }, idx);

    if (size == 0 || !GuestPtrOk(eaCanon) || !tryCopy)
    {
        r.flags |= A2bRes_Unresolved;
        s_build.unresolved++;
        return;
    }

    uint32_t copyN = size;
    if (copyN > A2b_CapMaxResCopy())
    {
        copyN = A2b_CapMaxResCopy();
        r.flags |= A2bRes_Truncated;
        s_build.truncated++;
    }
    {
        const uint32_t room = GuestCopyableBytes(eaCanon);
        if (room == 0)
        {
            r.flags |= A2bRes_Unresolved;
            s_build.unresolved++;
            return;
        }
        if (copyN > room)
        {
            copyN = room;
            r.flags |= A2bRes_Truncated;
            s_build.truncated++;
        }
    }
    if (s_build.resBytes + copyN > A2b_CapMaxResBytes())
    {
        s_build.flags |= A2bFrame_OverflowResBytes | A2bFrame_Incomplete;
        r.flags |= A2bRes_Unresolved;
        s_build.unresolved++;
        return;
    }

    const uint32_t phys = eaCanon & 0x3FFFFFFFu;
    const uint8_t* src = PhysPtr(phys);

    // A3p prod: sample fingerprint + address-keyed blob cache (no full O(n) hash).
    // Non-prod keeps full HashBytes for MATCH diagnostics.
    const bool useBlobCache = ProdMode() && copyN >= kBlobCacheMinBytes;
    uint32_t ch = 0;
    uint32_t sampleFp = 0;
    if (!ProdMode())
    {
        ProfScope ps(&s_profResHashNs);
        ch = HashBytes(src, copyN);
    }
    else if (useBlobCache)
    {
        ProfScope ps(&s_profResHashNs);
        sampleFp = SampleFingerprint(src, copyN);
        ch = sampleFp;
    }
    else
    {
        ch = copyN ^ (eaCanon * 2654435761u) ^ (type << 16);
        if (!ch) ch = 1u;
    }
    r.contentHash = ch;

    auto noteVertSat = [&]() {
        if (type == A2bRes_VertexArray && copyN >= kVertSatSpan)
        {
            const uint32_t slot = aux & 0xFF;
            if (slot < 12)
                s_vertSatMask.fetch_or(1u << slot, std::memory_order_relaxed);
        }
    };

    if (useBlobCache)
    {
        const BlobKey bk{ type, eaCanon, copyN, aux };
        auto it = s_blobCache.find(bk);
        if (it != s_blobCache.end() && it->second.data.size() == copyN &&
            it->second.sampleFp == sampleFp)
        {
            s_build.resData.insert(s_build.resData.end(), it->second.data.begin(),
                                   it->second.data.end());
            r.copiedSize = copyN;
            r.flags |= A2bRes_Copied;
            s_build.resBytes += copyN;
            it->second.lastXfb = (uint32_t)s_build.xfbIndex;
            s_profResCacheHits.fetch_add(1, std::memory_order_relaxed);
            s_profResCopiedBytes.fetch_add(copyN, std::memory_order_relaxed);
            noteVertSat();
            return;
        }
    }

    {
        ProfScope ps(&s_profResCopyNs);
        s_build.resData.insert(s_build.resData.end(), src, src + copyN);
    }
    r.copiedSize = copyN;
    r.flags |= A2bRes_Copied;
    s_build.resBytes += copyN;
    s_profResCacheMiss.fetch_add(1, std::memory_order_relaxed);
    s_profResCopiedBytes.fetch_add(copyN, std::memory_order_relaxed);
    noteVertSat();

    if (useBlobCache)
    {
        if (s_blobCache.size() >= kBlobCacheMaxEntries)
            s_blobCache.erase(s_blobCache.begin());
        BlobEnt ent{};
        ent.data.assign(src, src + copyN);
        ent.sampleFp = sampleFp;
        ent.lastXfb = (uint32_t)s_build.xfbIndex;
        s_blobCache.insert_or_assign(BlobKey{ type, eaCanon, copyN, aux }, std::move(ent));
    }
}

void AddResource(uint32_t type, uint32_t ea, uint32_t size, bool tryCopy,
                 uint32_t lifetime, uint32_t aux, bool required)
{
    if (!CaptureActive()) return;
    std::lock_guard lk(s_mu);
    EnsureGxStartLocked();
    AddResourceLocked(type, ea, size, tryCopy, lifetime, aux, required);
}

void MaybeDumpLocked(const FrameBuild& f)
{
    if (!DumpOn()) return;
    static uint64_t lastMs = 0;
    const uint64_t now = Platform_TickMs();
    if (now - lastMs < 2500) return;
    lastMs = now;

    LOG_INFO("A2b dump: xfb={} sim={} subs={} bytes={} cmdHash={:08x} "
             "res={} resBytes={} unres={} reqUnres={} draws={} trunc={} "
             "skipSubs={} flags={:02x} gxStart={} a1b={} ply={}",
             f.xfbIndex, f.simFrameId, f.submitCount, f.totalBytes,
             f.cmd.empty() ? 0u : HashBytes(f.cmd.data(), f.cmd.size()),
             f.resCount, f.resBytes, f.unresolved, f.requiredUnresolved, f.drawCount,
             f.truncated, f.skippedSubs, f.flags,
             f.gxStartValid ? "yes" : "no",
             f.ctx.a1bValid ? "yes" : "no",
             f.ctx.playerValid ? "yes" : "no");
}

}  // namespace

uint32_t A2b_FindFirstBp52(const uint8_t* cmd, uint32_t n)
{
    // Replay CP VCD/VAT into g_gx.cp temporarily so draw sizes can be skipped.
    uint32_t savedCp[0x100];
    memcpy(savedCp, g_gx.cp, sizeof(savedCp));

    uint32_t i = 0;
    while (i < n)
    {
        const uint8_t op = cmd[i];
        if (op == 0x00 || op == 0x44 || op == 0x48)
        {
            i += 1;
            continue;
        }
        if (op == 0x08)
        {
            if (i + 6 > n) break;
            uint8_t reg = cmd[i + 1];
            uint32_t val = ((uint32_t)cmd[i + 2] << 24) | ((uint32_t)cmd[i + 3] << 16) |
                           ((uint32_t)cmd[i + 4] << 8) | cmd[i + 5];
            g_gx.cp[reg] = val;
            i += 6;
            continue;
        }
        if (op == 0x10)
        {
            if (i + 5 > n) break;
            uint32_t w = ((uint32_t)cmd[i + 1] << 24) | ((uint32_t)cmd[i + 2] << 16) |
                         ((uint32_t)cmd[i + 3] << 8) | cmd[i + 4];
            uint32_t count = ((w >> 16) & 0xF) + 1;
            uint32_t need = 5 + 4 * count;
            if (i + need > n) break;
            i += need;
            continue;
        }
        if (op == 0x20 || op == 0x28 || op == 0x30 || op == 0x38)
        {
            if (i + 5 > n) break;
            i += 5;
            continue;
        }
        if (op == 0x40)
        {
            if (i + 9 > n) break;
            i += 9;
            continue;
        }
        if (op == 0x61)
        {
            if (i + 5 > n) break;
            // Only treat BP 0x52 as pass end when toXFB bit (val bit 14) is set.
            if (cmd[i + 1] == 0x52)
            {
                uint32_t val = ((uint32_t)cmd[i + 2] << 16) | ((uint32_t)cmd[i + 3] << 8) | cmd[i + 4];
                if ((val >> 14) & 1)
                {
                    memcpy(g_gx.cp, savedCp, sizeof(savedCp));
                    return i;
                }
            }
            i += 5;
            continue;
        }
        if (op & 0x80)
        {
            if (i + 3 > n) break;
            int vat = op & 7;
            uint32_t count = ((uint32_t)cmd[i + 1] << 8) | cmd[i + 2];
            VertexLayout l{};
            GX_ComputeLayout(vat, l);
            uint32_t need = 3 + count * l.size;
            if (i + need > n) break;
            i += need;
            continue;
        }
        i += 1;
    }
    memcpy(g_gx.cp, savedCp, sizeof(savedCp));
    return n;
}

bool A2b_FifoCaptureEnabled() { return EnvOn(); }
bool A2b_CaptureActive() { return CaptureActive(); }
bool A2b_ProdMode() { return ProdMode(); }
bool A2b_ProfileEnabled() { return ProfileOn(); }

void A2b_GetProfile(A2bProfileStats& out)
{
    out = A2bProfileStats{};
    out.frames = s_profFrames.load(std::memory_order_relaxed);
    out.fifoAppendNs = s_profFifoAppendNs.load(std::memory_order_relaxed);
    out.fifoAppendBytes = s_profFifoAppendBytes.load(std::memory_order_relaxed);
    out.fifoAppendCalls = s_profFifoAppendCalls.load(std::memory_order_relaxed);
    out.resFindNs = s_profResFindNs.load(std::memory_order_relaxed);
    out.resCopyNs = s_profResCopyNs.load(std::memory_order_relaxed);
    out.resHashNs = s_profResHashNs.load(std::memory_order_relaxed);
    out.resCacheHits = s_profResCacheHits.load(std::memory_order_relaxed);
    out.resCacheMiss = s_profResCacheMiss.load(std::memory_order_relaxed);
    out.resCopiedBytes = s_profResCopiedBytes.load(std::memory_order_relaxed);
    out.xfbFinalizeNs = s_profXfbFinalizeNs.load(std::memory_order_relaxed);
    out.cmdHashNs = s_profCmdHashNs.load(std::memory_order_relaxed);
    out.midRefreshNs = s_profMidRefreshNs.load(std::memory_order_relaxed);
    out.midRefreshBytes = s_profMidRefreshBytes.load(std::memory_order_relaxed);
    out.midPinBlock = s_profMidPinBlock.load(std::memory_order_relaxed);
    out.efbIncomingNs = s_profEfbInNs.load(std::memory_order_relaxed);
    out.metaEvalNs = s_profMetaEvalNs.load(std::memory_order_relaxed);
    out.indexedArrNs = s_profIdxArrNs.load(std::memory_order_relaxed);
    out.peakCmdBytes = s_profPeakCmd.load(std::memory_order_relaxed);
    out.peakResBytes = s_profPeakResBytes.load(std::memory_order_relaxed);
    out.peakResCount = s_profPeakResCount.load(std::memory_order_relaxed);
}

void A2b_OverlayProfile(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!ProfileOn()) return;
    A2bProfileStats st{};
    A2b_GetProfile(st);
    const double inv = st.frames ? (1.0 / (double)st.frames) : 0.0;
    std::snprintf(buf, bufSize,
                  "A2bProf f=%llu fifo=%.2fms resH=%.2fms resC=%.2fms idx=%.2fms efb=%.2fms "
                  "meta=%.2fms hit=%llu miss=%llu peakCmd=%u peakResB=%u",
                  (unsigned long long)st.frames, st.fifoAppendNs * inv / 1e6,
                  st.resHashNs * inv / 1e6, st.resCopyNs * inv / 1e6, st.indexedArrNs * inv / 1e6,
                  st.efbIncomingNs * inv / 1e6, st.metaEvalNs * inv / 1e6,
                  (unsigned long long)st.resCacheHits, (unsigned long long)st.resCacheMiss,
                  st.peakCmdBytes, st.peakResBytes);
}

void A2b_ProfAddIndexedArrNs(uint64_t ns)
{
    if (ns) s_profIdxArrNs.fetch_add(ns, std::memory_order_relaxed);
}

void A2b_OnFifoSubmission(const uint8_t* data, size_t n)
{
    if (!CaptureActive() || !data || n == 0) return;

    ProfScope ps(&s_profFifoAppendNs);
    std::lock_guard lk(s_mu);
    EnsureBuildCapsLocked(s_build);
    EnsureGxStartLocked();

    if (s_build.totalBytes + n > A2b_CapMaxBytes())
    {
        s_build.flags |= A2bFrame_OverflowBytes | A2bFrame_Incomplete;
        s_build.skippedSubs++;
        s_build.skippedBytes += (uint32_t)n;
        return;
    }

    const uint64_t sim = s_simTag.load(std::memory_order_relaxed);
    if (s_build.submitCount > 0)
    {
        A2bSubmission& cur = s_build.subs[s_build.submitCount - 1];
        if (cur.byteCount + n <= A2b_CapMaxBytesPerSub())
        {
            s_build.cmd.insert(s_build.cmd.end(), data, data + n);
            // A3p: incremental FNV — avoid O(n²) rehash of the whole submission.
            cur.contentHash = FnvUpdate(cur.contentHash, data, n);
            cur.byteCount += (uint32_t)n;
            s_build.totalBytes += (uint32_t)n;
            if (s_build.simFrameId == 0)
                s_build.simFrameId = sim;
            s_profFifoAppendCalls.fetch_add(1, std::memory_order_relaxed);
            s_profFifoAppendBytes.fetch_add(n, std::memory_order_relaxed);
            return;
        }
    }

    if (s_build.submitCount >= A2b_CapMaxSubs())
    {
        s_build.flags |= A2bFrame_OverflowSubs | A2bFrame_Incomplete;
        s_build.skippedSubs++;
        s_build.skippedBytes += (uint32_t)n;
        return;
    }

    A2bSubmission& s = s_build.subs[s_build.submitCount];
    s = A2bSubmission{};
    s.simFrameId = sim;
    s.submitSeq = s_build.submitCount;
    s.byteCount = (uint32_t)n;
    s.byteOffset = (uint32_t)s_build.cmd.size();
    // Running FNV state; appends continue from this value. Finalize 0→1 at summary.
    s.contentHash = FnvUpdate(2166136261u, data, n);
    s_build.cmd.insert(s_build.cmd.end(), data, data + n);
    s_build.totalBytes += (uint32_t)n;
    s_build.submitCount++;
    if (s_build.simFrameId == 0)
        s_build.simFrameId = s.simFrameId;
    s_profFifoAppendCalls.fetch_add(1, std::memory_order_relaxed);
    s_profFifoAppendBytes.fetch_add(n, std::memory_order_relaxed);
}

void A2b_OnDisplayList(uint32_t guestEa, uint32_t size)
{
    AddResource(A2bRes_DisplayList, guestEa, size, true, A2bResLife_FrameLocal, 0, true);
}

void A2b_OnIndexedXf(uint32_t guestEa, uint32_t byteSize)
{
    AddResource(A2bRes_IndexedXf, guestEa, byteSize, true, A2bResLife_FrameLocal, 0, true);
}

void A2b_OnVertexArray(uint32_t guestEa, uint32_t size, uint32_t stride, uint32_t arrayIndex)
{
    AddResource(A2bRes_VertexArray, guestEa, size, true, A2bResLife_FrameLocal,
                (arrayIndex & 0xFF) | ((stride & 0xFFFF) << 8), true);
}

bool A2b_HasCoveringCopy(uint32_t type, uint32_t guestEa, uint32_t aux, uint32_t minSize)
{
    if (!CaptureActive() || minSize == 0) return false;
    const uint32_t eaCanon = CanonGuestEa(guestEa);
    std::lock_guard lk(s_mu);
    auto it = s_resCover.find(CoverKey{ type, eaCanon, aux });
    if (it == s_resCover.end() || it->second >= s_build.resCount) return false;
    const A2bResource& r = s_build.resources[it->second];
    return r.type == type && r.guestEa == eaCanon && (r.flags & A2bRes_Copied) &&
           r.copiedSize >= minSize;
}

bool A2b_VertexSlotSaturated(uint32_t arrayIndex)
{
    if (arrayIndex >= 12) return false;
    return (s_vertSatMask.load(std::memory_order_relaxed) & (1u << arrayIndex)) != 0;
}

void A2b_OnTexture(uint32_t guestEa, uint32_t size, uint32_t fmt, uint32_t w, uint32_t h)
{
    const uint32_t aux = (fmt & 0xFF) | ((w & 0xFFF) << 8) | ((h & 0xFFF) << 20);
    AddResource(A2bRes_Texture, guestEa, size, true, A2bResLife_FrameLocal, aux, true);
}

void A2b_OnTlutRam(uint32_t guestEa, uint32_t size)
{
    AddResource(A2bRes_TlutRam, guestEa, size, true, A2bResLife_FrameLocal, 0, true);
}

void NoteCrossEdgeLocked(uint32_t ea, uint64_t genId, uint32_t createFrame, uint64_t createXfb)
{
    for (uint32_t i = 0; i < s_build.crossEdgeCount; i++)
    {
        if (s_build.crossEdges[i].genId == genId && s_build.crossEdges[i].destAddr == ea)
        {
            s_build.crossEdges[i].sampleCount++;
            return;
        }
    }
    if (s_build.crossEdgeCount >= kA2bMaxCrossEdges) return;
    A2bCrossEfbEdge& e = s_build.crossEdges[s_build.crossEdgeCount++];
    e = A2bCrossEfbEdge{};
    e.destAddr = ea;
    e.genId = genId;
    e.createFrame = createFrame;
    e.createXfb = createXfb;
    e.sampleXfb = s_build.xfbIndex ? s_build.xfbIndex : Perf_XfbFrameIndex();
    e.sampleCount = 1;
}

void A2b_OnEfbCopy(uint32_t destAddr, uint32_t ctl, uint32_t sx, uint32_t sy, uint32_t sw,
                   uint32_t sh, uint32_t fmt, uint32_t copyFlags, uint64_t genId,
                   uint32_t createFrame, uint64_t createXfb)
{
    if (!CaptureActive()) return;
    std::lock_guard lk(s_mu);
    EnsureBuildCapsLocked(s_build);
    EnsureGxStartLocked();
    const uint32_t ea = CanonGuestEa(destAddr);
    const uint32_t aux = (sx & 0x3FF) | ((sy & 0x3FF) << 10) | ((sw & 0x3FF) << 20);
    const uint32_t size = (sh & 0xFFFF) | ((fmt & 0xFF) << 16);
    // Prefer matching the exact generation when present; else same dest+region.
    int existing = -1;
    for (uint32_t i = 0; i < s_build.resCount; i++)
    {
        A2bResource& er = s_build.resources[i];
        if (er.type != A2bRes_EfbCopy || er.guestEa != ea) continue;
        if (genId && er.genId == genId) { existing = (int)i; break; }
        if (existing < 0 && er.size == size && er.aux == aux) existing = (int)i;
    }
    if (existing >= 0)
    {
        A2bResource& er = s_build.resources[existing];
        er.refCount++;
        er.flags |= A2bRes_Required | A2bRes_Copied;
        er.lifetime |= copyFlags;
        er.contentHash = ctl ? ctl : er.contentHash;
        if (genId)
        {
            er.genId = genId;
            er.createFrame = createFrame;
            er.createXfb = createXfb;
        }
        return;
    }
    if (s_build.resCount >= A2b_CapMaxResources())
    {
        s_build.flags |= A2bFrame_OverflowResources | A2bFrame_Incomplete;
        s_build.rejectedResources++;
        return;
    }
    A2bResource& r = s_build.resources[s_build.resCount++];
    r = A2bResource{};
    r.type = A2bRes_EfbCopy;
    r.guestEa = ea;
    r.size = size;
    r.aux = aux;
    r.contentHash = ctl;
    r.lifetime = copyFlags;
    r.refCount = 1;
    r.submitSeq = s_build.submitCount > 0 ? s_build.submitCount - 1 : 0;
    r.genId = genId;
    r.createFrame = createFrame;
    r.createXfb = createXfb;
    // Metadata-complete: the copy is reproduced from scratch EFB + BP cmds at replay.
    r.flags = A2bRes_Required | A2bRes_Copied;
    r.copiedSize = 0;
}

void A2b_OnEfbCopySampled(uint32_t destAddr, uint64_t genId, uint32_t createFrame,
                          uint64_t createXfb, bool crossFrame)
{
    if (!CaptureActive()) return;
    std::lock_guard lk(s_mu);
    EnsureBuildCapsLocked(s_build);
    const uint32_t ea = CanonGuestEa(destAddr);
    bool found = false;
    for (uint32_t i = 0; i < s_build.resCount; i++)
    {
        A2bResource& r = s_build.resources[i];
        if (r.type != A2bRes_EfbCopy || r.guestEa != ea) continue;
        if (genId && r.genId && r.genId != genId) continue;
        r.lifetime |= A2bEfbCopy_Sampled;
        if (crossFrame) r.lifetime |= A2bEfbCopy_CrossFrame;
        r.flags |= A2bRes_Required;
        r.refCount++;
        if (!r.genId && genId)
        {
            r.genId = genId;
            r.createFrame = createFrame;
            r.createXfb = createXfb;
        }
        found = true;
    }
    if (!found)
    {
        // Consumer-only reference: producer lived in an earlier archived frame.
        if (s_build.resCount >= A2b_CapMaxResources())
        {
            s_build.flags |= A2bFrame_OverflowResources | A2bFrame_Incomplete;
            s_build.rejectedResources++;
        }
        else
        {
            A2bResource& r = s_build.resources[s_build.resCount++];
            r = A2bResource{};
            r.type = A2bRes_EfbCopy;
            r.guestEa = ea;
            r.genId = genId;
            r.createFrame = createFrame;
            r.createXfb = createXfb;
            r.lifetime = A2bEfbCopy_Sampled | (crossFrame ? A2bEfbCopy_CrossFrame : 0);
            r.refCount = 1;
            r.submitSeq = s_build.submitCount > 0 ? s_build.submitCount - 1 : 0;
            // Copied=true for consumer stub: result comes from retained scratch gen, not bytes.
            r.flags = A2bRes_Required | A2bRes_Copied;
            if (!crossFrame)
                r.flags |= A2bRes_Unresolved; // same-frame sample without producer is incomplete
        }
    }
    if (crossFrame && genId)
        NoteCrossEdgeLocked(ea, genId, createFrame, createXfb);
}

uint32_t A2b_CountEfbCopiesInCmd(const uint8_t* cmd, uint32_t n)
{
    // Same walk as FindFirstBp52; count non-toXFB BP 0x52.
    if (!cmd || n == 0) return 0;
    uint32_t savedCp[0x100];
    memcpy(savedCp, g_gx.cp, sizeof(savedCp));
    uint32_t i = 0, count = 0;
    while (i < n)
    {
        const uint8_t op = cmd[i];
        if (op == 0x00 || op == 0x44 || op == 0x48) { i += 1; continue; }
        if (op == 0x08)
        {
            if (i + 6 > n) break;
            g_gx.cp[cmd[i + 1]] = ((uint32_t)cmd[i + 2] << 24) | ((uint32_t)cmd[i + 3] << 16) |
                                  ((uint32_t)cmd[i + 4] << 8) | cmd[i + 5];
            i += 6;
            continue;
        }
        if (op == 0x10)
        {
            if (i + 5 > n) break;
            uint32_t w = ((uint32_t)cmd[i + 1] << 24) | ((uint32_t)cmd[i + 2] << 16) |
                         ((uint32_t)cmd[i + 3] << 8) | cmd[i + 4];
            uint32_t need = 5 + 4 * (((w >> 16) & 0xF) + 1);
            if (i + need > n) break;
            i += need;
            continue;
        }
        if (op == 0x20 || op == 0x28 || op == 0x30 || op == 0x38)
        {
            if (i + 5 > n) break;
            i += 5;
            continue;
        }
        if (op == 0x40)
        {
            if (i + 9 > n) break;
            i += 9;
            continue;
        }
        if (op == 0x61)
        {
            if (i + 5 > n) break;
            if (cmd[i + 1] == 0x52)
            {
                uint32_t val = ((uint32_t)cmd[i + 2] << 16) | ((uint32_t)cmd[i + 3] << 8) | cmd[i + 4];
                if (((val >> 14) & 1) == 0)
                    count++;
            }
            i += 5;
            continue;
        }
        if (op & 0x80)
        {
            if (i + 3 > n) break;
            int vat = op & 7;
            uint32_t vtxCount = ((uint32_t)cmd[i + 1] << 8) | cmd[i + 2];
            VertexLayout l{};
            GX_ComputeLayout(vat, l);
            uint32_t need = 3 + vtxCount * l.size;
            if (i + need > n) break;
            i += need;
            continue;
        }
        i += 1;
    }
    memcpy(g_gx.cp, savedCp, sizeof(savedCp));
    return count;
}

void A2b_OnTmemWindow(uint32_t tmemOff, uint32_t size, const uint8_t* data)
{
    if (!CaptureActive() || !data || size == 0) return;
    std::lock_guard lk(s_mu);
    EnsureBuildCapsLocked(s_build);
    EnsureGxStartLocked();
    int existing = FindResource(s_build, A2bRes_TmemWindow, tmemOff, size, 0);
    if (existing >= 0)
    {
        s_build.resources[existing].refCount++;
        s_build.resources[existing].flags |= A2bRes_Required;
        return;
    }
    if (s_build.resCount >= A2b_CapMaxResources())
    {
        s_build.flags |= A2bFrame_OverflowResources | A2bFrame_Incomplete;
        s_build.rejectedResources++;
        return;
    }
    uint32_t copyN = size;
    if (copyN > A2b_CapMaxResCopy())
        copyN = A2b_CapMaxResCopy();
    if (s_build.resBytes + copyN > A2b_CapMaxResBytes())
    {
        s_build.flags |= A2bFrame_OverflowResBytes | A2bFrame_Incomplete;
        return;
    }
    A2bResource& r = s_build.resources[s_build.resCount++];
    r = A2bResource{};
    r.type = A2bRes_TmemWindow;
    r.guestEa = tmemOff;
    r.size = size;
    r.copiedSize = copyN;
    r.lifetime = A2bResLife_FrameLocal;
    r.flags = A2bRes_Copied | A2bRes_Required;
    if (copyN < size)
    {
        r.flags |= A2bRes_Truncated;
        s_build.truncated++;
    }
    r.dataOffset = (uint32_t)s_build.resData.size();
    r.contentHash = HashBytes(data, copyN);
    r.refCount = 1;
    s_build.resData.insert(s_build.resData.end(), data, data + copyN);
    s_build.resBytes += copyN;
}

void A2b_NoteDraw()
{
    if (!EnvOn()) return;
    if (!CaptureActive())
    {
        s_obsDraws.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    std::lock_guard lk(s_mu);
    s_build.drawCount++;
}

uint32_t A2b_CurrentCmdBytes()
{
    if (!EnvOn() || !CaptureActive()) return 0;
    std::lock_guard lk(s_mu);
    return (uint32_t)s_build.cmd.size();
}

void A2b_OnSimFrameEnd()
{
    A2gNat_OnGuestFrame();  // independent of FIFO capture
    if (!EnvOn()) return;
    s_simTag.store(Perf_GuestFrameIndex(), std::memory_order_relaxed);
}

void A2b_OnXfbCopy()
{
    if (!EnvOn()) return;
    // Soft-arm: observe draws without CaptureIndexedArrays / resource copies.
    if (!A3e_CaptureArmed())
    {
        const uint32_t obs = s_obsDraws.exchange(0, std::memory_order_relaxed);
        A3e_NoteObservedDraws(obs);
        std::lock_guard lk(s_mu);
        s_build = FrameBuild{};
        s_resIndex.clear();
        s_resCover.clear();
        s_vertSatMask.store(0, std::memory_order_relaxed);
        return;
    }
    s_obsDraws.store(0, std::memory_order_relaxed);

    {
        ProfScope ps(&s_profXfbFinalizeNs);
        std::lock_guard lk(s_mu);
        s_build.xfbIndex = Perf_XfbFrameIndex();
        if (s_build.simFrameId == 0)
            s_build.simFrameId = s_simTag.load(std::memory_order_relaxed);
        // Finalize submission hashes (0 → 1 sentinel matches HashBytes).
        for (uint32_t i = 0; i < s_build.submitCount; i++)
            if (!s_build.subs[i].contentHash) s_build.subs[i].contentHash = 1u;
        SnapGxLite(s_build.gx);
        SnapContext(s_build.ctx);
        RecountRequired(s_build);

        if (s_build.totalBytes > s_peakCmdReserve)
            s_peakCmdReserve = s_build.totalBytes + (s_build.totalBytes >> 2);
        if (s_build.resBytes > s_peakResReserve)
            s_peakResReserve = s_build.resBytes + (s_build.resBytes >> 2);
        s_profFrames.fetch_add(1, std::memory_order_relaxed);
        uint32_t peak = s_profPeakCmd.load(std::memory_order_relaxed);
        while (s_build.totalBytes > peak &&
               !s_profPeakCmd.compare_exchange_weak(peak, s_build.totalBytes))
        {
        }
        peak = s_profPeakResBytes.load(std::memory_order_relaxed);
        while (s_build.resBytes > peak &&
               !s_profPeakResBytes.compare_exchange_weak(peak, s_build.resBytes))
        {
        }
        peak = s_profPeakResCount.load(std::memory_order_relaxed);
        while (s_build.resCount > peak &&
               !s_profPeakResCount.compare_exchange_weak(peak, s_build.resCount))
        {
        }

        if (s_haveCurr)
        {
            ReleaseEfbSlot(s_prev);
            s_prev = std::move(s_curr);
            s_havePrev = true;
        }
        s_curr = std::move(s_build);
        // int move leaves source unchanged — detach so ResetBuild won't release s_curr's slot.
        s_build.efbIncomingSlot = -1;
        s_build.efbIncomingValid = false;
        s_haveCurr = true;
        s_frames.fetch_add(1, std::memory_order_relaxed);

        MaybeDumpLocked(s_curr);
        ResetBuild(s_build);
        // Next frame GX start = post-XFB registers. EFB incoming is captured later
        // via A2b_OnPassEntryEfb after the optional post-XFB ClearEFB.
        s_build.gxStart = g_gx;
        s_build.gxStartValid = true;
        A3e_NoteObservedDraws(s_curr.drawCount);
    }

    A2b_RefreshMidPacket();
    {
        ProfScope ps(&s_profMetaEvalNs);
        A2c_OnArchivedFrame();
    }

    if (ProfileOn())
    {
        static uint64_t lastProfMs = 0;
        const uint64_t now = Platform_TickMs();
        if (now - lastProfMs >= 2000)
        {
            lastProfMs = now;
            char line[256];
            A2b_OverlayProfile(line, sizeof(line));
            if (line[0]) LOG_INFO("{}", line);
        }
    }

    if (DumpOn())
    {
        const char* dir = std::getenv("SCR_A2B_FIFO_DUMP_DIR");
        if (dir && dir[0])
        {
            static uint64_t lastExportMs = 0;
            const uint64_t now = Platform_TickMs();
            if (now - lastExportMs >= 5000)
            {
                lastExportMs = now;
                A2b_ExportLastFrame(dir);
            }
        }
    }
}

void A2b_OnPassEntryEfb()
{
    // A3p: EFB incoming capture is a GPU readback — only while archive capture is armed.
    if (!CaptureActive()) return;
    ProfScope ps(&s_profEfbInNs);
    std::lock_guard lk(s_mu);
    // Replace any prior slot for the in-progress build.
    ReleaseEfbSlot(s_build);
    uint32_t ch = 0, dh = 0, ew = 0, eh = 0;
    // Prod: GPU copy only. Diagnostic FIFO capture keeps every-pixel hashes for MATCH.
    const int slot = Render_EfbIncomingCapture(&ch, &dh, &ew, &eh, !ProdMode());
    if (slot >= 0)
    {
        s_build.efbIncomingSlot = slot;
        s_build.efbIncomingValid = true;
        s_build.efbInColorHash = ch;
        s_build.efbInDepthHash = dh;
        s_build.efbInW = ew;
        s_build.efbInH = eh;
    }
}

uint64_t A2b_FrameCount() { return s_frames.load(std::memory_order_relaxed); }

bool A2b_HasPair()
{
    std::lock_guard lk(s_mu);
    return s_havePrev && s_haveCurr;
}

bool A2b_GetCurrentSummary(A2bFrameSummary& out)
{
    std::lock_guard lk(s_mu);
    if (!s_haveCurr) return false;
    FillSummary(s_curr, out);
    return true;
}

bool A2b_GetPreviousSummary(A2bFrameSummary& out)
{
    std::lock_guard lk(s_mu);
    if (!s_havePrev) return false;
    FillSummary(s_prev, out);
    return true;
}

static void ClonePacketFromLocked(const FrameBuild& src, A2bArchivePacket& out, bool addEfbRef,
                                  bool includeResData)
{
    out = A2bArchivePacket{};
    FillSummary(src, out.summary);
    out.gxStart = src.gxStart;
    out.gxStartValid = src.gxStartValid;
    out.efbIncomingSlot = src.efbIncomingSlot;
    out.efbIncomingValid = src.efbIncomingValid;
    out.efbInColorHash = src.efbInColorHash;
    out.efbInDepthHash = src.efbInDepthHash;
    if (addEfbRef && out.efbIncomingValid)
        Render_EfbIncomingAddRef(out.efbIncomingSlot);
    out.cmd = src.cmd;
    if (includeResData)
    {
        out.resData = src.resData;
        out.tmemStart = src.tmemStart;
    }
    out.submitCount = src.submitCount;
    out.resCount = src.resCount;
    out.crossEdgeCount = src.crossEdgeCount;
    out.subs.assign(src.subs.begin(), src.subs.begin() + src.submitCount);
    out.resources.assign(src.resources.begin(), src.resources.begin() + src.resCount);
    for (uint32_t i = 0; i < src.crossEdgeCount; i++)
        out.crossEdges[i] = src.crossEdges[i];
}

bool A2b_CloneCurrentPacket(A2bArchivePacket& out)
{
    std::lock_guard lk(s_mu);
    if (!s_haveCurr) return false;
    ClonePacketFromLocked(s_curr, out, true, true);
    return true;
}

bool A2b_ClonePreviousPacket(A2bArchivePacket& out)
{
    std::lock_guard lk(s_mu);
    if (!s_havePrev) return false;
    ClonePacketFromLocked(s_prev, out, true, true);
    return true;
}

bool A2b_CloneCurrentMeta(A2bArchivePacket& out)
{
    std::lock_guard lk(s_mu);
    if (!s_haveCurr) return false;
    out = A2bArchivePacket{};
    FillSummary(s_curr, out.summary);
    out.gxStart = s_curr.gxStart;
    out.gxStartValid = s_curr.gxStartValid;
    out.efbIncomingSlot = s_curr.efbIncomingSlot;
    out.efbIncomingValid = s_curr.efbIncomingValid;
    out.efbInColorHash = s_curr.efbInColorHash;
    out.efbInDepthHash = s_curr.efbInDepthHash;
    // No EFB addref — meta-only for EvaluatePass.
    out.cmd = s_curr.cmd;
    out.submitCount = s_curr.submitCount;
    out.resCount = s_curr.resCount;
    out.crossEdgeCount = s_curr.crossEdgeCount;
    out.subs.assign(s_curr.subs.begin(), s_curr.subs.begin() + s_curr.submitCount);
    out.resources.assign(s_curr.resources.begin(), s_curr.resources.begin() + s_curr.resCount);
    for (uint32_t i = 0; i < s_curr.crossEdgeCount; i++)
        out.crossEdges[i] = s_curr.crossEdges[i];
    // resData intentionally empty — EvaluatePass does not read blob bytes.
    return true;
}

void A2b_RefreshMidPacket()
{
    // A3p: mark dirty only — full clone deferred to first PinMidPacket.
    if (!ProdMode() && !EnvFlag("SCR_DECOUPLED_PRESENT")) return;
    std::lock_guard lk(s_mu);
    if (!s_haveCurr) return;
    s_midDirty = true;
}

bool A2b_PinMidPacket(const A2bArchivePacket** out)
{
    if (!out) return false;
    ProfScope ps(&s_profMidRefreshNs);
    std::lock_guard lk(s_mu);
    if (!s_haveCurr) return false;
    if (s_midDirty)
    {
        if (s_midPin.load(std::memory_order_relaxed) > 0)
        {
            s_profMidPinBlock.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        if (s_midValid && s_midPkt.efbIncomingValid)
        {
            Render_EfbIncomingRelease(s_midPkt.efbIncomingSlot);
            s_midPkt.efbIncomingValid = false;
            s_midPkt.efbIncomingSlot = -1;
        }
        // Prod mid reads live guest RAM — skip multi-MB resData clone.
        ClonePacketFromLocked(s_curr, s_midPkt, true, !ProdMode());
        s_midValid = true;
        s_midDirty = false;
        s_profMidRefreshBytes.fetch_add(s_midPkt.cmd.size() + s_midPkt.resData.size(),
                                        std::memory_order_relaxed);
    }
    if (!s_midValid) return false;
    s_midPin.fetch_add(1, std::memory_order_relaxed);
    *out = &s_midPkt;
    return true;
}

void A2b_UnpinMidPacket()
{
    s_midPin.fetch_sub(1, std::memory_order_relaxed);
}

bool A2b_MidPacketValid()
{
    std::lock_guard lk(s_mu);
    return s_midValid;
}

uint32_t A2b_GetCrossEfbEdgeCount()
{
    std::lock_guard lk(s_mu);
    return s_haveCurr ? s_curr.crossEdgeCount : 0;
}

bool A2b_GetCrossEfbEdge(uint32_t index, A2bCrossEfbEdge& out)
{
    std::lock_guard lk(s_mu);
    if (!s_haveCurr || index >= s_curr.crossEdgeCount) return false;
    out = s_curr.crossEdges[index];
    return true;
}

void A2b_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || bufSize == 0) return;
    buf[0] = 0;
    if (!EnvOn()) return;
    A2bFrameSummary s{};
    if (!A2b_GetCurrentSummary(s))
    {
        std::snprintf(buf, bufSize, "A2b: waiting for XFB");
        return;
    }
    std::snprintf(buf, bufSize,
                  "A2b xfb %llu subs %u bytes %u draws %u res %u reqU %u flags %02x%s",
                  (unsigned long long)s.xfbIndex, s.submissionCount, s.totalBytes, s.drawCount,
                  s.resourceCount, s.requiredUnresolved, s.flags,
                  (s.flags & A2bFrame_Incomplete) ? " INC" : "");
}

bool A2b_ExportLastFrame(const char* dirPath)
{
    if (!EnvOn() || !dirPath || !dirPath[0]) return false;

    A2bArchivePacket copy;
    if (!A2b_CloneCurrentPacket(copy)) return false;

    std::string base = dirPath;
    while (!base.empty() && (base.back() == '/' || base.back() == '\\'))
        base.pop_back();

    char path[512];
    std::snprintf(path, sizeof(path), "%s/a2b_xfb_%llu.bin", base.c_str(),
                  (unsigned long long)copy.summary.xfbIndex);
    FILE* bin = std::fopen(path, "wb");
    if (!bin) return false;
    if (!copy.cmd.empty())
        std::fwrite(copy.cmd.data(), 1, copy.cmd.size(), bin);
    std::fclose(bin);

    std::snprintf(path, sizeof(path), "%s/a2b_xfb_%llu.txt", base.c_str(),
                  (unsigned long long)copy.summary.xfbIndex);
    FILE* txt = std::fopen(path, "w");
    if (!txt) return false;
    const A2bFrameSummary& sum = copy.summary;
    std::fprintf(txt,
                 "simFrameId=%llu\nxfbIndex=%llu\nsubmissions=%u\ntotalBytes=%u\n"
                 "commandHash=%08x\nresources=%u\nresourceBytes=%u\nunresolved=%u\n"
                 "truncated=%u\nskippedSubs=%u\nskippedBytes=%u\ndraws=%u\n"
                 "required=%u\nrequiredUnresolved=%u\nflags=%08x\ngxStartValid=%u\n",
                 (unsigned long long)sum.simFrameId, (unsigned long long)sum.xfbIndex,
                 sum.submissionCount, sum.totalBytes, sum.commandHash, sum.resourceCount,
                 sum.resourceBytes, sum.unresolvedResources, sum.truncatedResources,
                 sum.skippedSubmissions, sum.skippedBytes, sum.drawCount,
                 sum.requiredResources, sum.requiredUnresolved, sum.flags,
                 sum.gxStartValid ? 1u : 0u);
    for (uint32_t i = 0; i < copy.submitCount; i++)
    {
        const A2bSubmission& s = copy.subs[i];
        std::fprintf(txt, "sub %u seq=%u off=%u bytes=%u hash=%08x sim=%llu\n",
                     i, s.submitSeq, s.byteOffset, s.byteCount, s.contentHash,
                     (unsigned long long)s.simFrameId);
    }
    for (uint32_t i = 0; i < copy.resCount; i++)
    {
        const A2bResource& r = copy.resources[i];
        std::fprintf(txt,
                     "res %u type=%u ea=%08x size=%u copied=%u hash=%08x flags=%02x "
                     "life=%u aux=%08x refs=%u\n",
                     i, r.type, r.guestEa, r.size, r.copiedSize, r.contentHash, r.flags,
                     r.lifetime, r.aux, r.refCount);
    }
    std::fclose(txt);

    if (!copy.resData.empty())
    {
        std::snprintf(path, sizeof(path), "%s/a2b_xfb_%llu_res.bin", base.c_str(),
                      (unsigned long long)copy.summary.xfbIndex);
        FILE* rb = std::fopen(path, "wb");
        if (rb)
        {
            std::fwrite(copy.resData.data(), 1, copy.resData.size(), rb);
            std::fclose(rb);
        }
    }
    LOG_INFO("A2b export: wrote dumps under {} for xfb {}", base, copy.summary.xfbIndex);
    return true;
}

bool A2b_CompareCurrPrev(A2bCompareResult& out)
{
    out = A2bCompareResult{};
    std::lock_guard lk(s_mu);
    if (!s_haveCurr || !s_havePrev) return false;
    out.bothValid = true;
    const uint32_t curH = s_curr.cmd.empty() ? 0u : HashBytes(s_curr.cmd.data(), s_curr.cmd.size());
    const uint32_t prevH = s_prev.cmd.empty() ? 0u : HashBytes(s_prev.cmd.data(), s_prev.cmd.size());
    out.commandEqual = (curH == prevH) && (s_curr.totalBytes == s_prev.totalBytes) &&
                       (s_curr.submitCount == s_prev.submitCount);
    out.gxEqual = s_curr.gx.valid && s_prev.gx.valid &&
                  s_curr.gx.xfMemHash == s_prev.gx.xfMemHash &&
                  s_curr.gx.posMtx0Hash == s_prev.gx.posMtx0Hash;
    out.ctxPlayerEqual = s_curr.ctx.playerValid && s_prev.ctx.playerValid &&
                         s_curr.ctx.playerBodyEa == s_prev.ctx.playerBodyEa &&
                         s_curr.ctx.playerPos[0] == s_prev.ctx.playerPos[0] &&
                         s_curr.ctx.playerPos[1] == s_prev.ctx.playerPos[1] &&
                         s_curr.ctx.playerPos[2] == s_prev.ctx.playerPos[2];
    out.subCountDelta = s_curr.submitCount > s_prev.submitCount
                            ? s_curr.submitCount - s_prev.submitCount
                            : s_prev.submitCount - s_curr.submitCount;
    out.byteDelta = s_curr.totalBytes > s_prev.totalBytes
                        ? s_curr.totalBytes - s_prev.totalBytes
                        : s_prev.totalBytes - s_curr.totalBytes;
    out.resCountDelta = s_curr.resCount > s_prev.resCount
                            ? s_curr.resCount - s_prev.resCount
                            : s_prev.resCount - s_curr.resCount;
    return true;
}
