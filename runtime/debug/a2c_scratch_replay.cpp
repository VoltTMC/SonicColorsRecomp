// Approach A2c — resource closure + isolated scratch-EFB replay experiment.
#include "a2c_scratch_replay.h"
#include "a2b_fifo_archive.h"
#include "../gx/gx_state.h"
#include "../gx/texture_decode.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../platform/platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

extern int32_t g_tevKonst[4][4];

namespace {

bool ReplayEnv()
{
    static int c = -1;
    if (c < 0)
    {
        const char* v = std::getenv("SCR_A2C_SCRATCH_REPLAY");
        c = (v && v[0] && v[0] != '0') ? 1 : 0;
        if (c)
            LOG_INFO("A2c: scratch-EFB replay enabled (SCR_A2C_SCRATCH_REPLAY); PE IRQ suppress; no live XFB write");
    }
    return c != 0;
}

std::mutex s_mu;
A2cPassReport s_pass{};
A2cReplayResult s_lastReplay{};
bool s_havePass = false;
uint32_t s_replayBudgetFrames = 0;  // rate-limit live replay attempts

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

// Walk FIFO vertex stream like VertexLoader to derive indexed array ranges.
void CaptureIndexedArrays(int vat, uint32_t count, const uint8_t* stream, uint32_t streamBytes)
{
    VertexLayout layout{};
    GX_ComputeLayout(vat, layout);
    if (!count || layout.size == 0) return;

    static const uint8_t kCompSize[8] = { 1, 1, 2, 2, 4, 0, 0, 0 };
    static const uint8_t kColorSize[8] = { 2, 3, 4, 2, 3, 4, 0, 0 };

    struct Range { bool used = false; uint32_t minI = ~0u, maxI = 0; };
    Range ranges[12]{};

    auto noteIdx = [&](int arr, uint32_t idx) {
        if (arr < 0 || arr >= 12) return;
        ranges[arr].used = true;
        if (idx < ranges[arr].minI) ranges[arr].minI = idx;
        if (idx > ranges[arr].maxI) ranges[arr].maxI = idx;
    };

    const uint8_t* s = stream;
    const uint8_t* end = stream + streamBytes;
    for (uint32_t v = 0; v < count && s < end; v++)
    {
        if (layout.posMtxIdx)
        {
            if (s >= end) break;
            s++;
        }
        for (int i = 0; i < 8; i++)
            if (layout.texMtxIdx[i])
            {
                if (s >= end) break;
                s++;
            }

        auto fetch = [&](uint8_t type, int arr, uint32_t directSize) {
            if (type == 1)
            {
                s += directSize;
                return;
            }
            if (type == 2)
            {
                if (s >= end) return;
                noteIdx(arr, *s++);
                return;
            }
            if (type == 3)
            {
                if (s + 2 > end) return;
                uint32_t idx = ((uint32_t)s[0] << 8) | s[1];
                s += 2;
                noteIdx(arr, idx);
            }
        };

        if (layout.pos)
        {
            uint32_t posN = layout.posCnt ? 3u : 2u;
            fetch(layout.pos, 0, kCompSize[layout.posFmt] * posN);
        }
        if (layout.nrm)
        {
            if (layout.nrm == 1)
                s += kCompSize[layout.nrmFmt] * (layout.nrmCnt ? 9u : 3u);
            else
            {
                int n = (layout.nrmCnt && layout.nrmIndex3) ? 3 : 1;
                for (int i = 0; i < n; i++)
                    fetch(layout.nrm, 1, 0);
            }
        }
        for (int i = 0; i < 2; i++)
            if (layout.col[i])
                fetch(layout.col[i], 2 + i, kColorSize[layout.colFmt[i]]);
        for (int i = 0; i < 8; i++)
            if (layout.tex[i])
            {
                uint32_t tn = layout.texCnt[i] ? 2u : 1u;
                fetch(layout.tex[i], 4 + i, kCompSize[layout.texFmt[i]] * tn);
            }
    }

    for (int a = 0; a < 12; a++)
    {
        if (!ranges[a].used) continue;
        uint32_t base = g_gx.cp[CP_ARRAY_BASE + a];
        uint32_t stride = g_gx.cp[CP_ARRAY_STRIDE + a];
        if (!base || stride == 0) continue;
        // Inclusive max index → copy through (max+1)*stride; reject absurd spans.
        uint64_t elems = (uint64_t)ranges[a].maxI + 1;
        uint64_t bytes = elems * stride;
        if (bytes == 0 || bytes > (256u << 10))
        {
            // Still record as required unresolved if too large / invalid.
            A2b_OnVertexArray(base, 0, stride, (uint32_t)a);
            continue;
        }
        // Prefer copying from index 0 through max (GPU arrays often densely packed).
        A2b_OnVertexArray(base, (uint32_t)bytes, stride, (uint32_t)a);
    }
}

void EvaluatePass(const A2bArchivePacket& pkt, A2cPassReport& out)
{
    out = A2cPassReport{};
    out.xfbIndex = pkt.summary.xfbIndex;
    out.simFrameId = pkt.summary.simFrameId;
    out.drawCount = pkt.summary.drawCount;

    // Pass boundary: commands [0, passEnd) — first toXFB BP 0x52 is the terminator
    // (not included). Live reference is sampled at that XFB event (EFB after pass draws).
    uint32_t passEnd = A2b_FindFirstBp52(pkt.cmd.data(), (uint32_t)pkt.cmd.size());
    if (passEnd == 0 || (passEnd < pkt.cmd.size() && pkt.cmd[passEnd] != 0x61))
        passEnd = (uint32_t)pkt.cmd.size();
    out.passBytes = passEnd;
    out.passHash = passEnd ? HashBytes(pkt.cmd.data(), passEnd) : 0;
    out.efbIncomingValid = pkt.efbIncomingValid;
    out.efbInColorHash = pkt.efbInColorHash;
    out.efbInDepthHash = pkt.efbInDepthHash;

    uint32_t ready = 0;
    uint32_t fail = 0;
    auto ok = [&](uint32_t bit, bool cond) {
        if (cond) ready |= bit;
        else fail |= bit;
    };

    ok(A2cReady_CmdComplete,
       pkt.summary.skippedBytes == 0 && !(pkt.summary.flags & A2bFrame_OverflowBytes) &&
           !pkt.cmd.empty());
    ok(A2cReady_SubsComplete, pkt.summary.skippedSubmissions == 0 &&
                                  !(pkt.summary.flags & A2bFrame_OverflowSubs));
    ok(A2cReady_GxStartPresent, pkt.gxStartValid);
    ok(A2cReady_EfbIncoming, pkt.efbIncomingValid && Render_EfbIncomingValid(pkt.efbIncomingSlot));
    ok(A2cReady_NoOverflow, !(pkt.summary.flags & A2bFrame_Incomplete) ||
                                pkt.summary.requiredUnresolved == 0);

    bool vertOk = true, texOk = true, dlOk = true;
    uint32_t reqUnres = 0, deps = 0, depBytes = 0;
    for (uint32_t i = 0; i < pkt.resCount; i++)
    {
        const A2bResource& r = pkt.resources[i];
        if (!(r.flags & A2bRes_Required)) continue;
        deps++;
        depBytes += r.copiedSize;
        const bool incomplete = (r.flags & A2bRes_Unresolved) || (r.flags & A2bRes_Truncated) ||
                                ((r.flags & A2bRes_Copied) == 0);
        if (incomplete) reqUnres++;
        if (r.type == A2bRes_VertexArray && incomplete) vertOk = false;
        if ((r.type == A2bRes_Texture || r.type == A2bRes_TlutRam || r.type == A2bRes_TmemWindow) &&
            incomplete)
            texOk = false;
        if (r.type == A2bRes_DisplayList && incomplete) dlOk = false;
        if (r.type == A2bRes_IndexedXf && incomplete) vertOk = false;
    }
    out.depCount = deps;
    out.depBytes = depBytes;
    out.unresolvedRequired = reqUnres;

    ok(A2cReady_VertexComplete, vertOk);
    ok(A2cReady_TextureComplete, texOk);
    ok(A2cReady_DlComplete, dlOk);

    // Selection: title/boot-scale pass (not full TR1 scenes).
    const bool selected = out.passBytes > 256 && out.passBytes <= (128u << 10) &&
                          out.drawCount > 0 && out.drawCount <= 128 && pkt.gxStartValid;
    ok(A2cReady_PassSelected, selected);

    out.readyFlags = ready;
    out.failFlags = fail;
    out.replayReady = (fail == 0) && selected && reqUnres == 0;

    if (!out.replayReady)
    {
        if (!selected)
            std::snprintf(out.reason, sizeof(out.reason),
                          "not selected (bytes=%u draws=%u gxStart=%u efbIn=%u)", out.passBytes,
                          out.drawCount, pkt.gxStartValid ? 1u : 0u,
                          pkt.efbIncomingValid ? 1u : 0u);
        else if (reqUnres)
            std::snprintf(out.reason, sizeof(out.reason),
                          "required unresolved=%u failFlags=%03x", reqUnres, fail);
        else
            std::snprintf(out.reason, sizeof(out.reason), "failFlags=%03x", fail);
    }
    else
    {
        std::snprintf(out.reason, sizeof(out.reason),
                      "ready passBytes=%u draws=%u deps=%u efbIn=%08x/%08x",
                      out.passBytes, out.drawCount, deps, out.efbInColorHash,
                      out.efbInDepthHash);
    }
}

struct RamSave
{
    uint32_t phys = 0;
    std::vector<uint8_t> bytes;
};

bool ApplyResourcesToGuest(const A2bArchivePacket& pkt, std::vector<RamSave>& saves)
{
    saves.clear();
    for (uint32_t i = 0; i < pkt.resCount; i++)
    {
        const A2bResource& r = pkt.resources[i];
        if (!(r.flags & A2bRes_Copied) || r.copiedSize == 0) continue;
        if (r.type == A2bRes_TmemWindow)
        {
            uint8_t* tmem = Render_TmemPtr();
            if (r.guestEa + r.copiedSize <= Render_TmemSize())
                memcpy(tmem + r.guestEa, pkt.resData.data() + r.dataOffset, r.copiedSize);
            continue;
        }
        uint32_t phys = r.guestEa & 0x1FFFFFFF;
        RamSave sv;
        sv.phys = phys;
        sv.bytes.assign(PhysPtr(phys), PhysPtr(phys) + r.copiedSize);
        saves.push_back(std::move(sv));
        memcpy(PhysPtr(phys), pkt.resData.data() + r.dataOffset, r.copiedSize);
    }
    return true;
}

void RestoreGuest(const std::vector<RamSave>& saves)
{
    for (const RamSave& sv : saves)
        memcpy(PhysPtr(sv.phys), sv.bytes.data(), sv.bytes.size());
}

void RunScratchReplay(A2bArchivePacket& pkt, const A2cPassReport& pass, A2cReplayResult& rr)
{
    rr = A2cReplayResult{};
    rr.attempted = true;
    rr.passHash = pass.passHash;
    rr.efbInColorHash = pass.efbInColorHash;
    rr.efbInDepthHash = pass.efbInDepthHash;
    std::snprintf(rr.detail, sizeof(rr.detail), "starting");

    if (!pass.replayReady)
    {
        std::snprintf(rr.detail, sizeof(rr.detail), "not replay-ready: %s", pass.reason);
        return;
    }

    // Live reference = EFB after pass draws, sampled at toXFB (this call site).
    rr.liveEfbHash = Render_HashLiveEfb();
    rr.liveDepthHash = Render_HashLiveDepth();

    GXState gxSaved = g_gx;
    std::vector<uint8_t> tmemSaved(Render_TmemPtr(), Render_TmemPtr() + Render_TmemSize());
    int32_t tevSaved[4][4];
    memcpy(tevSaved, g_tevKonst, sizeof(tevSaved));

    std::vector<RamSave> ramSaves;
    ApplyResourcesToGuest(pkt, ramSaves);

    auto cleanup = [&]() {
        GX_SetPeSuppress(false);
        Render_SetReplayIsolation(false);
        g_gx = gxSaved;
        memcpy(Render_TmemPtr(), tmemSaved.data(), tmemSaved.size());
        memcpy(g_tevKonst, tevSaved, sizeof(tevSaved));
        RestoreGuest(ramSaves);
        if (pkt.efbIncomingValid)
        {
            Render_EfbIncomingRelease(pkt.efbIncomingSlot);
            pkt.efbIncomingValid = false;
            pkt.efbIncomingSlot = -1;
        }
        rr.stateRestored = true;
    };

    GX_SetPeSuppress(true);

    uint32_t sw = 0, sh = 0;
    auto replayOnce = [&](bool doCompare) -> uint32_t {
        g_gx = pkt.gxStart;
        Render_ResetHostDrawState();
        if (!Render_ScratchBeginFromIncoming(pkt.efbIncomingSlot, &sw, &sh))
            return 0;
        rr.scratchW = sw;
        rr.scratchH = sh;
        rr.isolationOk = true;
        rr.seededFromIncoming = true;

        GX_ReplayCommands(pkt.cmd.data(), pass.passBytes);

        if (doCompare)
        {
            // Live EFB unchanged (draws went to scratch); same pass-exit boundary.
            // Sample depth hash before EndHash tears down isolation.
            const uint32_t scrD = Render_ScratchEndDepthHash();
            RenderEfbCompare cmp{};
            if (Render_CompareLiveVsScratch(cmp, true))
            {
                rr.imageCompared = true;
                rr.colorExact = cmp.colorExact;
                rr.depthExact = cmp.depthExact;
                rr.depthCompared = cmp.depthCompared;
                rr.colorMismatch = cmp.colorMismatch;
                rr.depthMismatch = cmp.depthMismatch;
                rr.maxChannelDelta = cmp.maxChannelDelta;
                rr.comparedPixels = cmp.comparedPixels;
                rr.liveEfbHash = cmp.liveColorHash;
                rr.scratchEfbHash = cmp.scratchColorHash;
                rr.liveDepthHash = cmp.liveDepthHash;
                rr.scratchDepthHash = cmp.scratchDepthHash;
                (void)scrD;  // cmp hashes are authoritative (same everyPixel mode)
                // Full MATCH requires color exact and a successful depth compare that matches.
                // Color-only equality with depth N/A is reported as COLOR_MATCH, not MATCH.
                rr.imageMatch = cmp.colorExact && cmp.depthCompared && cmp.depthExact;
            }
            else
                rr.scratchDepthHash = scrD;
        }

        return Render_ScratchEndHash();
    };

    const uint32_t h1 = replayOnce(true);
    // Determinism probe — do not clobber comparison hashes from the first run.
    const uint32_t liveCSaved = rr.liveEfbHash, scrCSaved = rr.scratchEfbHash;
    const uint32_t liveDSaved = rr.liveDepthHash, scrDSaved = rr.scratchDepthHash;
    const bool matchSaved = rr.imageMatch, colorSaved = rr.colorExact, depthSaved = rr.depthExact;
    const bool dCmpSaved = rr.depthCompared, imgCmpSaved = rr.imageCompared;
    const uint32_t cMisSaved = rr.colorMismatch, dMisSaved = rr.depthMismatch;
    const uint32_t maxDSaved = rr.maxChannelDelta, pixSaved = rr.comparedPixels;
    const uint32_t h2 = replayOnce(false);
    rr.liveEfbHash = liveCSaved;
    rr.scratchEfbHash = scrCSaved;
    rr.liveDepthHash = liveDSaved;
    rr.scratchDepthHash = scrDSaved;
    rr.imageMatch = matchSaved;
    rr.colorExact = colorSaved;
    rr.depthExact = depthSaved;
    rr.depthCompared = dCmpSaved;
    rr.imageCompared = imgCmpSaved;
    rr.colorMismatch = cMisSaved;
    rr.depthMismatch = dMisSaved;
    rr.maxChannelDelta = maxDSaved;
    rr.comparedPixels = pixSaved;
    if (!rr.imageCompared)
        rr.scratchEfbHash = h1;
    const bool deterministic = (h1 != 0 && h1 == h2);
    rr.completed = deterministic && rr.seededFromIncoming && rr.isolationOk;

    cleanup();

    const char* verdict = "DIFF";
    if (rr.imageMatch)
        verdict = "MATCH";
    else if (rr.colorExact && !rr.depthCompared)
        verdict = "COLOR_MATCH";
    else if (rr.colorExact && rr.depthCompared && !rr.depthExact)
        verdict = "DEPTH_DIFF";
    std::snprintf(rr.detail, sizeof(rr.detail),
                  "xfb=%llu pass=%u seed=1 det=%d colorMis=%u depthMis=%u maxΔ=%u dCmp=%d %s",
                  (unsigned long long)pass.xfbIndex, pass.passBytes, deterministic ? 1 : 0,
                  rr.colorMismatch, rr.depthMismatch, rr.maxChannelDelta,
                  rr.depthCompared ? 1 : 0, verdict);
    LOG_INFO("A2d replay: {} liveC={:08x} scrC={:08x} liveD={:08x} scrD={:08x} inC={:08x} inD={:08x}",
             rr.detail, rr.liveEfbHash, rr.scratchEfbHash, rr.liveDepthHash,
             rr.scratchDepthHash, rr.efbInColorHash, rr.efbInDepthHash);
}

}  // namespace

bool A2c_ScratchReplayEnabled() { return ReplayEnv(); }

bool A2c_ClosureCaptureEnabled()
{
    return A2b_FifoCaptureEnabled();
}

void A2c_OnDraw(int vat, uint32_t count, const uint8_t* vtxStream, uint32_t streamBytes)
{
    if (!A2c_ClosureCaptureEnabled() || Render_ReplayIsolationActive()) return;
    CaptureIndexedArrays(vat, count, vtxStream, streamBytes);
}

void A2c_OnTexture(uint32_t physAddr, uint32_t byteSize, uint32_t fmt, uint32_t w, uint32_t h,
                   uint32_t tlutTmemOff, uint32_t tlutBytes)
{
    if (!A2c_ClosureCaptureEnabled() || Render_ReplayIsolationActive()) return;
    A2b_OnTexture(physAddr, byteSize, fmt, w, h);
    if (tlutBytes && tlutTmemOff + tlutBytes <= Render_TmemSize())
        A2b_OnTmemWindow(tlutTmemOff, tlutBytes, Render_TmemPtr() + tlutTmemOff);
}

void A2c_OnTlutRam(uint32_t physAddr, uint32_t byteSize)
{
    if (!A2c_ClosureCaptureEnabled() || Render_ReplayIsolationActive()) return;
    A2b_OnTlutRam(physAddr, byteSize);
}

void A2c_OnArchivedFrame()
{
    A2bArchivePacket pkt;
    if (!A2b_CloneCurrentPacket(pkt)) return;

    A2cPassReport report{};
    EvaluatePass(pkt, report);

    {
        std::lock_guard lk(s_mu);
        s_pass = report;
        s_havePass = true;
    }

    static uint64_t lastLogMs = 0;
    const uint64_t now = Platform_TickMs();
    if (now - lastLogMs >= 2500)
    {
        lastLogMs = now;
        LOG_INFO("A2d ready: xfb={} ready={} flags={:03x}/{:03x} passBytes={} draws={} "
                 "deps={} reqUnres={} efbIn={} — {}",
                 report.xfbIndex, report.replayReady ? "yes" : "no", report.readyFlags,
                 report.failFlags, report.passBytes, report.drawCount, report.depCount,
                 report.unresolvedRequired, report.efbIncomingValid ? "yes" : "no",
                 report.reason);
    }

    if (!ReplayEnv() || !report.replayReady) return;

    // Rate-limit: at most one replay every ~3s while ready frames stream.
    if (now - lastLogMs < 50 && s_replayBudgetFrames > 0)
    {
        // allow first; then space out via s_replayBudgetFrames
    }
    static uint64_t lastReplayMs = 0;
    if (now - lastReplayMs < 3000) return;
    lastReplayMs = now;
    s_replayBudgetFrames++;

    A2cReplayResult rr{};
    RunScratchReplay(pkt, report, rr);  // releases pkt's EFB slot ref
    std::lock_guard lk(s_mu);
    s_lastReplay = rr;
}

bool A2c_GetPassReport(A2cPassReport& out)
{
    std::lock_guard lk(s_mu);
    if (!s_havePass) return false;
    out = s_pass;
    return true;
}

bool A2c_GetLastReplay(A2cReplayResult& out)
{
    std::lock_guard lk(s_mu);
    if (!s_lastReplay.attempted) return false;
    out = s_lastReplay;
    return true;
}

void A2c_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || bufSize == 0) return;
    buf[0] = 0;
    A2cPassReport p{};
    if (!A2c_GetPassReport(p))
    {
        if (A2c_ScratchReplayEnabled())
            std::snprintf(buf, bufSize, "A2c: waiting");
        return;
    }
    A2cReplayResult r{};
    const bool haveR = A2c_GetLastReplay(r);
    if (haveR && r.attempted)
        std::snprintf(buf, bufSize, "A2d %s mis=%u/%u %s",
                      p.replayReady ? "READY" : "no", r.colorMismatch, r.depthMismatch,
                      r.imageMatch ? "MATCH" : (r.completed ? "DIFF" : "fail"));
    else
        std::snprintf(buf, bufSize, "A2d %s pass=%u efbIn=%u u=%u",
                      p.replayReady ? "READY" : "no", p.passBytes,
                      p.efbIncomingValid ? 1u : 0u, p.unresolvedRequired);
}
