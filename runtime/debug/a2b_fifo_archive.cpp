// Approach A2b — bounded GX FIFO archive before ProcessFifo consumes bytes.
//
// Lifecycle (see docs/FRAME_RATE.md A2b):
//   GPFifo_Write* → Gather → (ring) → GpuThread copies → ProcessFifo erases s_fifo
//   Display lists: RecordingToRAM writes guest RAM; CallDisplayList executes later.
// Archive point: A2b_OnFifoSubmission on each ring/gather batch before ProcessFifo;
//                DL / indexed-XF refs snapshotted at decode time.
#include "a2b_fifo_archive.h"
#include "a1b_pose_capture.h"
#include "a1c_gx_capture.h"
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
#include <vector>

namespace {

bool EnvOn()
{
    static int c = -1;
    if (c < 0)
    {
        const char* v = std::getenv("SCR_A2B_FIFO_CAPTURE");
        c = (v && v[0] && v[0] != '0') ? 1 : 0;
        if (c)
            LOG_INFO("A2b: FIFO archive enabled (SCR_A2B_FIFO_CAPTURE); no replay / no extra XFB");
    }
    return c != 0;
}

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

uint32_t HashBytes(const uint8_t* p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h ? h : 1u;
}

uint32_t CanonGuestEa(uint32_t ea)
{
    // CallDisplayList passes phys (& 0x1FFFFFE0); CP arrays may already be 0x8/0x9.
    if (!ea) return 0;
    if ((ea & 0xFE000000u) == 0)
        return 0x80000000u | (ea & 0x1FFFFFFFu);
    return ea;
}

bool GuestPtrOk(uint32_t ea)
{
    ea = CanonGuestEa(ea);
    if (!ea) return false;
    const uint32_t hi = ea >> 28;
    return hi == 0x8 || hi == 0x9 || hi == 0xC || hi == 0xD;
}

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
    A2bSubmission subs[kA2bMaxSubsPerFrame]{};
    A2bResource resources[kA2bMaxResources]{};
    A2bGxStateSnap gx{};
    A2bContextSnap ctx{};
    std::vector<uint8_t> cmd;
    std::vector<uint8_t> resData;
};

std::mutex s_mu;
FrameBuild s_build{};
FrameBuild s_curr{};
FrameBuild s_prev{};
bool s_haveCurr = false, s_havePrev = false;
std::atomic<uint64_t> s_frames{ 0 };
std::atomic<uint64_t> s_simTag{ 0 };

void ResetBuild(FrameBuild& f)
{
    f = FrameBuild{};
    f.cmd.clear();
    f.resData.clear();
    f.cmd.reserve(256 * 1024);
}

void SnapGx(A2bGxStateSnap& g)
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
    // PNMTX0..9 = 120 words
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
    out.flags = f.flags;
    out.commandHash = f.cmd.empty() ? 0 : HashBytes(f.cmd.data(), f.cmd.size());
    out.gx = f.gx;
    out.ctx = f.ctx;
}

int FindResource(FrameBuild& f, uint32_t type, uint32_t ea, uint32_t size, uint32_t aux)
{
    for (uint32_t i = 0; i < f.resCount; i++)
        if (f.resources[i].type == type && f.resources[i].guestEa == ea &&
            f.resources[i].size == size && f.resources[i].aux == aux)
            return (int)i;
    return -1;
}

void AddResourceLocked(uint32_t type, uint32_t ea, uint32_t size, bool tryCopy,
                       uint32_t lifetime, uint32_t aux)
{
    const uint32_t eaCanon = CanonGuestEa(ea);
    // Allow size==0 inventory-only entries (CP arrays with unknown extent).
    int existing = FindResource(s_build, type, eaCanon, size, aux);
    if (existing >= 0) return;

    if (s_build.resCount >= kA2bMaxResources)
    {
        s_build.flags |= A2bFrame_OverflowResources | A2bFrame_Incomplete;
        return;
    }

    A2bResource& r = s_build.resources[s_build.resCount++];
    r = A2bResource{};
    r.type = type;
    r.guestEa = eaCanon;
    r.size = size;
    r.lifetime = lifetime;
    r.aux = aux;
    r.submitSeq = s_build.submitCount > 0 ? s_build.submitCount - 1 : 0;
    r.dataOffset = (uint32_t)s_build.resData.size();

    if (size == 0 || !GuestPtrOk(eaCanon) || !tryCopy)
    {
        r.flags = A2bRes_Unresolved;
        s_build.unresolved++;
        return;
    }

    uint32_t copyN = size;
    if (copyN > kA2bMaxResCopy)
    {
        copyN = kA2bMaxResCopy;
        r.flags |= A2bRes_Truncated;
        s_build.truncated++;
    }
    if (s_build.resBytes + copyN > kA2bMaxResBytes)
    {
        s_build.flags |= A2bFrame_OverflowResBytes | A2bFrame_Incomplete;
        r.flags = A2bRes_Unresolved;
        s_build.unresolved++;
        return;
    }

    // PhysPtr expects physical; CanonGuestEa yields cached VA — strip to phys for MEM1/2.
    const uint32_t phys = eaCanon & 0x1FFFFFFF;
    const uint8_t* src = PhysPtr(phys);
    s_build.resData.insert(s_build.resData.end(), src, src + copyN);
    r.copiedSize = copyN;
    r.contentHash = HashBytes(src, copyN);
    r.flags |= A2bRes_Copied;
    s_build.resBytes += copyN;
}

void AddResource(uint32_t type, uint32_t ea, uint32_t size, bool tryCopy,
                 uint32_t lifetime, uint32_t aux)
{
    if (!EnvOn()) return;
    std::lock_guard lk(s_mu);
    AddResourceLocked(type, ea, size, tryCopy, lifetime, aux);
}

// Inventory CP vertex/index array bases at XFB — address known, extent not.
void InventoryCpArraysLocked()
{
    for (int i = 0; i < 16; i++)
    {
        const uint32_t base = g_gx.cp[CP_ARRAY_BASE + i];
        const uint32_t stride = g_gx.cp[CP_ARRAY_STRIDE + i];
        if (!base) continue;
        AddResourceLocked(A2bRes_CpArrayBase, base, 0, false,
                          A2bResLife_FrameLocal, (uint32_t)i | (stride << 8));
    }
    // Texture / EFB: record that they are required but not snapshotted.
    AddResourceLocked(A2bRes_TextureTmem, 0, 0, false, A2bResLife_Unknown, 0);
    AddResourceLocked(A2bRes_EfbConfig, 0, 0, false, A2bResLife_FrameLocal, g_gx.bp[0x64]);
}

void MaybeDumpLocked(const FrameBuild& f)
{
    if (!DumpOn()) return;
    static uint64_t lastMs = 0;
    const uint64_t now = Platform_TickMs();
    if (now - lastMs < 2500) return;
    lastMs = now;

    LOG_INFO("A2b dump: xfb={} sim={} subs={} bytes={} cmdHash={:08x} "
             "res={} resBytes={} unres={} trunc={} skipSubs={} skipBytes={} "
             "flags={:02x} pair={} xfMem={:08x} a1b={} a1cLoads={} ply={}",
             f.xfbIndex, f.simFrameId, f.submitCount, f.totalBytes,
             f.cmd.empty() ? 0u : HashBytes(f.cmd.data(), f.cmd.size()),
             f.resCount, f.resBytes, f.unresolved, f.truncated,
             f.skippedSubs, f.skippedBytes, f.flags,
             s_havePrev ? "yes" : "no", f.gx.xfMemHash,
             f.ctx.a1bValid ? "yes" : "no", f.ctx.a1cLoads,
             f.ctx.playerValid ? "yes" : "no");

    if (s_havePrev)
    {
        const uint32_t prevH = s_prev.cmd.empty() ? 0u : HashBytes(s_prev.cmd.data(), s_prev.cmd.size());
        const uint32_t curH = f.cmd.empty() ? 0u : HashBytes(f.cmd.data(), f.cmd.size());
        LOG_INFO("A2b delta: prevBytes={} curBytes={} cmdHash {:08x}->{:08x} "
                 "subs {}->{} res {}->{} xfMem {:08x}->{:08x}",
                 s_prev.totalBytes, f.totalBytes, prevH, curH,
                 s_prev.submitCount, f.submitCount, s_prev.resCount, f.resCount,
                 s_prev.gx.xfMemHash, f.gx.xfMemHash);
    }

    // Emit a few submission lines for inspection.
    const uint32_t show = f.submitCount < 4 ? f.submitCount : 4;
    for (uint32_t i = 0; i < show; i++)
    {
        const A2bSubmission& s = f.subs[i];
        LOG_INFO("A2b sub[{}]: seq={} bytes={} hash={:08x} sim={}",
                 i, s.submitSeq, s.byteCount, s.contentHash, s.simFrameId);
    }
    for (uint32_t i = 0; i < f.resCount && i < 3; i++)
    {
        const A2bResource& r = f.resources[i];
        LOG_INFO("A2b res[{}]: type={} ea={:08x} size={} copied={} hash={:08x} flags={:02x}",
                 i, r.type, r.guestEa, r.size, r.copiedSize, r.contentHash, r.flags);
    }
}

}  // namespace

bool A2b_FifoCaptureEnabled() { return EnvOn(); }

void A2b_OnFifoSubmission(const uint8_t* data, size_t n)
{
    if (!EnvOn() || !data || n == 0) return;

    std::lock_guard lk(s_mu);
    if (s_build.totalBytes + n > kA2bMaxBytesPerFrame)
    {
        s_build.flags |= A2bFrame_OverflowBytes | A2bFrame_Incomplete;
        // Do not silently store a partial submission as complete.
        s_build.skippedSubs++;
        s_build.skippedBytes += (uint32_t)n;
        return;
    }

    // Gather publishes ~32 B; GpuThread often drains each chunk separately.
    // Coalesce consecutive drains into one submission (order-preserving) so
    // busy frames stay under kA2bMaxSubsPerFrame without dropping bytes.
    const uint64_t sim = s_simTag.load(std::memory_order_relaxed);
    if (s_build.submitCount > 0)
    {
        A2bSubmission& cur = s_build.subs[s_build.submitCount - 1];
        if (cur.byteCount + n <= kA2bMaxBytesPerSub)
        {
            s_build.cmd.insert(s_build.cmd.end(), data, data + n);
            cur.byteCount += (uint32_t)n;
            cur.contentHash = HashBytes(s_build.cmd.data() + cur.byteOffset, cur.byteCount);
            s_build.totalBytes += (uint32_t)n;
            if (s_build.simFrameId == 0)
                s_build.simFrameId = sim;
            return;
        }
    }

    if (s_build.submitCount >= kA2bMaxSubsPerFrame)
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
    s.contentHash = HashBytes(data, n);
    s.sourceHint = 0;
    s_build.cmd.insert(s_build.cmd.end(), data, data + n);
    s_build.totalBytes += (uint32_t)n;
    s_build.submitCount++;
    if (s_build.simFrameId == 0)
        s_build.simFrameId = s.simFrameId;
}

void A2b_OnDisplayList(uint32_t guestEa, uint32_t size)
{
    AddResource(A2bRes_DisplayList, guestEa, size, true,
                A2bResLife_FrameLocal, 0);
}

void A2b_OnIndexedXf(uint32_t guestEa, uint32_t byteSize)
{
    AddResource(A2bRes_IndexedXf, guestEa, byteSize, true,
                A2bResLife_FrameLocal, 0);
}

void A2b_OnSimFrameEnd()
{
    if (!EnvOn()) return;
    s_simTag.store(Perf_GuestFrameIndex(), std::memory_order_relaxed);
}

void A2b_OnXfbCopy()
{
    if (!EnvOn()) return;

    {
        std::lock_guard lk(s_mu);
        s_build.xfbIndex = Perf_XfbFrameIndex();
        if (s_build.simFrameId == 0)
            s_build.simFrameId = s_simTag.load(std::memory_order_relaxed);
        SnapGx(s_build.gx);
        SnapContext(s_build.ctx);
        InventoryCpArraysLocked();

        if (s_haveCurr)
        {
            s_prev = std::move(s_curr);
            s_havePrev = true;
        }
        s_curr = std::move(s_build);
        s_haveCurr = true;
        s_frames.fetch_add(1, std::memory_order_relaxed);

        MaybeDumpLocked(s_curr);
        ResetBuild(s_build);
    }

    // Optional file export (outside lock).
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
                  "A2b xfb %llu subs %u bytes %u res %u flags %02x%s",
                  (unsigned long long)s.xfbIndex, s.submissionCount, s.totalBytes,
                  s.resourceCount, s.flags, (s.flags & A2bFrame_Incomplete) ? " INC" : "");
}

bool A2b_ExportLastFrame(const char* dirPath)
{
    if (!EnvOn() || !dirPath || !dirPath[0]) return false;

    FrameBuild copy;
    {
        std::lock_guard lk(s_mu);
        if (!s_haveCurr) return false;
        copy = s_curr;  // deep-ish: vectors copy
    }

    // Best-effort mkdir via Platform / C.
    std::string base = dirPath;
    while (!base.empty() && (base.back() == '/' || base.back() == '\\'))
        base.pop_back();

    char path[512];
    std::snprintf(path, sizeof(path), "%s/a2b_xfb_%llu.bin", base.c_str(),
                  (unsigned long long)copy.xfbIndex);
    FILE* bin = std::fopen(path, "wb");
    if (!bin) return false;
    if (!copy.cmd.empty())
        std::fwrite(copy.cmd.data(), 1, copy.cmd.size(), bin);
    std::fclose(bin);

    std::snprintf(path, sizeof(path), "%s/a2b_xfb_%llu.txt", base.c_str(),
                  (unsigned long long)copy.xfbIndex);
    FILE* txt = std::fopen(path, "w");
    if (!txt) return false;
    A2bFrameSummary sum{};
    FillSummary(copy, sum);
    std::fprintf(txt,
                 "simFrameId=%llu\nxfbIndex=%llu\nsubmissions=%u\ntotalBytes=%u\n"
                 "commandHash=%08x\nresources=%u\nresourceBytes=%u\nunresolved=%u\n"
                 "truncated=%u\nskippedSubs=%u\nskippedBytes=%u\nflags=%08x\n"
                 "xfMemHash=%08x\nposMtx0Hash=%08x\ncpVcdLo=%08x\ncpVatA0=%08x\n"
                 "bpScissor0=%08x\nbpEfb=%08x\n"
                 "a1bValid=%u\na1cLoads=%u\nplayerValid=%u\nplayerBodyEa=%08x\n",
                 (unsigned long long)sum.simFrameId, (unsigned long long)sum.xfbIndex,
                 sum.submissionCount, sum.totalBytes, sum.commandHash, sum.resourceCount,
                 sum.resourceBytes, sum.unresolvedResources, sum.truncatedResources,
                 sum.skippedSubmissions, sum.skippedBytes, sum.flags,
                 sum.gx.xfMemHash, sum.gx.posMtx0Hash, sum.gx.cpVcdLo, sum.gx.cpVatA0,
                 sum.gx.bpScissor0, sum.gx.bpEfbAddr,
                 sum.ctx.a1bValid ? 1u : 0u, sum.ctx.a1cLoads,
                 sum.ctx.playerValid ? 1u : 0u, sum.ctx.playerBodyEa);
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
                     "life=%u aux=%08x submit=%u\n",
                     i, r.type, r.guestEa, r.size, r.copiedSize, r.contentHash, r.flags,
                     r.lifetime, r.aux, r.submitSeq);
    }
    std::fclose(txt);

    if (!copy.resData.empty())
    {
        std::snprintf(path, sizeof(path), "%s/a2b_xfb_%llu_res.bin", base.c_str(),
                      (unsigned long long)copy.xfbIndex);
        FILE* rb = std::fopen(path, "wb");
        if (rb)
        {
            std::fwrite(copy.resData.data(), 1, copy.resData.size(), rb);
            std::fclose(rb);
        }
    }
    LOG_INFO("A2b export: wrote dumps under {} for xfb {}", base, copy.xfbIndex);
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
