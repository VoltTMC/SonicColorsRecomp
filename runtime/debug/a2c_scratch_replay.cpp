// Approach A2c — resource closure + isolated scratch-EFB replay experiment.
#include "a2c_scratch_replay.h"
#include "a2b_fifo_archive.h"
#include "a3a_draw_attr.h"
#include "a3b_camera_interp.h"
#include "a3c_env_capture.h"
#include "a3d_scene_coverage.h"
#include "a3e_budgets.h"
#include "../gx/gx_state.h"
#include "../gx/texture_decode.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../platform/platform.h"

#include <cmath>
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
// Per-family last ready report + replay (A2d / A2e / A2f / A2g).
constexpr uint32_t kFamilySlots = 8;
A2cPassReport s_familyPass[kFamilySlots]{};
A2cReplayResult s_familyReplay[kFamilySlots]{};
bool s_haveFamilyPass[kFamilySlots]{};
bool s_haveFamilyReplay[kFamilySlots]{};
uint32_t s_replayBudgetFrames = 0;
uint64_t s_lastReplayMsByFamily[kFamilySlots]{};
uint32_t s_forcedCompareLeft = 4;  // first N replays force sync everyPixel compare
uint32_t s_comparePending = 0, s_compareDone = 0, s_compareSkipped = 0, s_compareFailed = 0;

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

uint32_t ClassifyFamily(uint32_t passBytes, uint32_t draws, uint32_t deps, uint32_t efbCopySampled,
                        uint32_t efbCopyCross)
{
    // Structural fingerprints only — no guest addresses / hard-coded EAs.
    // A2g: any pass that samples an EFB generation created in an earlier frame.
    if (efbCopyCross >= 1)
        return A2cFamily_A2gCrossEfb;
    // A2f: idle-scale pass with sampled EFB→tex feedback (mip-chain HUD).
    if (efbCopySampled >= 1 && passBytes >= 4000 && passBytes <= 7000 && draws >= 60 &&
        draws <= 75)
        return A2cFamily_A2fEfbFb;
    // A2d: same size band without sampled EFB feedback (rare once capture is on).
    if (passBytes >= 4000 && passBytes <= 7000 && draws >= 60 && draws <= 75 && deps >= 1 &&
        deps <= 8)
        return A2cFamily_A2dIdle;
    // A2e: richer static/UI — larger stream / multi-texture (EFB copies optional).
    if (passBytes >= 10000 && passBytes <= 16000 && draws >= 75 && draws <= 100 && deps >= 6)
        return A2cFamily_A2eMultiTex;
    // A3c: TR1-scale env pass — multi-draw world layer (opt-in selection only).
    // A3c: TR1-scale env — allow denser passes once resource budgets are raised.
    if ((A3c_EnvCaptureEnabled() || A3c_EnvInterpEnabled()) && passBytes >= 8000 &&
        passBytes <= (512u << 10) && draws >= 100 && draws <= 800 && deps >= 8)
        return A2cFamily_A3cEnv;
    // A3e: dense complete gameplay (beyond A2e/A3c bands). Explicit family so
    // selection budgets can rise without mis-labeling as Other.
    if (A3e_DenseEnabled() && passBytes >= 7000 && passBytes <= (1024u << 10) && draws >= 150 &&
        draws <= 16384 && deps >= 8)
        return A2cFamily_A3eDense;
    return A2cFamily_Other;
}

bool FindProducerForEdge(const A2bArchivePacket& prev, const A2bCrossEfbEdge& edge,
                         A2bResource* outRes)
{
    for (uint32_t i = 0; i < prev.resCount; i++)
    {
        const A2bResource& r = prev.resources[i];
        if (r.type != A2bRes_EfbCopy) continue;
        if (edge.genId && r.genId == edge.genId)
        {
            if (outRes) *outRes = r;
            return true;
        }
        if (!edge.genId && r.guestEa == edge.destAddr && r.createXfb == edge.createXfb)
        {
            if (outRes) *outRes = r;
            return true;
        }
    }
    return false;
}

void EvaluatePass(const A2bArchivePacket& pkt, A2cPassReport& out)
{
    out = A2cPassReport{};
    out.xfbIndex = pkt.summary.xfbIndex;
    out.simFrameId = pkt.summary.simFrameId;
    out.drawCount = pkt.summary.drawCount;

    // Pass boundary: commands [0, passEnd) — first toXFB BP 0x52 is the terminator
    // (not included). Live reference is sampled at that XFB event (EFB after pass draws).
    // Non-toXFB BP 0x52 (EFB→tex copies) remain inside the pass.
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
        if (r.type == A2bRes_DisplayList)
            out.depDl++;
        else if (r.type == A2bRes_Texture || r.type == A2bRes_TlutRam)
            out.depTex++;
        else if (r.type == A2bRes_TmemWindow)
            out.depTmem++;
        else if (r.type == A2bRes_VertexArray || r.type == A2bRes_IndexedXf)
            out.depVert++;
        else if (r.type == A2bRes_EfbCopy)
        {
            out.depEfbCopy++;
            if (r.lifetime & A2bEfbCopy_Sampled)
                out.depEfbCopySampled++;
            if (r.lifetime & A2bEfbCopy_CrossFrame)
                out.depEfbCopyCross++;
        }
        else
            out.depOther++;
        const bool incomplete = (r.flags & A2bRes_Unresolved) || (r.flags & A2bRes_Truncated) ||
                                ((r.flags & A2bRes_Copied) == 0);
        // Cross-frame consumer stubs are resolved at sequence level, not alone.
        const bool crossStub = (r.type == A2bRes_EfbCopy) && (r.lifetime & A2bEfbCopy_CrossFrame) &&
                               !(r.lifetime & ~((uint32_t)(A2bEfbCopy_Sampled | A2bEfbCopy_CrossFrame))) &&
                               r.copiedSize == 0 && r.size == 0;
        if (incomplete && !crossStub) reqUnres++;
        if (r.type == A2bRes_VertexArray && incomplete) vertOk = false;
        if ((r.type == A2bRes_Texture || r.type == A2bRes_TlutRam || r.type == A2bRes_TmemWindow) &&
            incomplete)
            texOk = false;
        if (r.type == A2bRes_DisplayList && incomplete) dlOk = false;
        if (r.type == A2bRes_IndexedXf && incomplete) vertOk = false;
        if (r.type == A2bRes_EfbCopy && incomplete && !crossStub) texOk = false;
    }
    out.depCount = deps;
    out.depBytes = depBytes;
    out.unresolvedRequired = reqUnres;
    out.crossEdgeCount = pkt.crossEdgeCount;
    out.consumerXfb = pkt.summary.xfbIndex;
    out.family =
        ClassifyFamily(out.passBytes, out.drawCount, deps, out.depEfbCopySampled, out.depEfbCopyCross);

    ok(A2cReady_VertexComplete, vertOk);
    ok(A2cReady_TextureComplete, texOk);
    ok(A2cReady_DlComplete, dlOk);

    // EFB feedback: if the pass produced copies, every required copy must be
    // metadata-complete; A2f also requires at least one sampled consumer.
    // Cross-frame-only consumers are handled by A2cReady_CrossFrame.
    const bool efbFbOk =
        (out.depEfbCopy == 0) || (out.depEfbCopyCross > 0 && out.depEfbCopySampled == out.depEfbCopyCross) ||
        (out.depEfbCopySampled >= 1 && out.depEfbCopy >= out.depEfbCopySampled);
    ok(A2cReady_EfbFeedback, efbFbOk);

    // A2g single-packet: cross edges present → need producer packet (checked later).
    // Without edges, CrossFrame readiness is N/A (vacuously true).
    ok(A2cReady_CrossFrame, out.depEfbCopyCross == 0 || pkt.crossEdgeCount > 0);

    // Selection: static/UI-scale pass (not full TR1 scenes). Named families preferred
    // but Other within size bounds remains selectable for diagnostics.
    // A2g may use slightly larger consumer passes when edges are present.
    // A3cEnv: raised draw/byte caps for coherent TR1 environment archives.
    const bool a3cSel = (A3c_EnvCaptureEnabled() || A3c_EnvInterpEnabled()) &&
                        out.family == A2cFamily_A3cEnv && out.passBytes > 256 &&
                        out.passBytes <= (512u << 10) && out.drawCount >= 100 &&
                        out.drawCount <= 800 && pkt.gxStartValid;
    const bool a3eSel = A3e_DenseEnabled() && out.family == A2cFamily_A3eDense &&
                        out.passBytes > 256 && out.passBytes <= (1024u << 10) &&
                        out.drawCount >= 150 && out.drawCount <= 16384 && pkt.gxStartValid &&
                        !(pkt.summary.flags & A2bFrame_Incomplete) &&
                        pkt.summary.rejectedResources == 0;
    const bool selected =
        a3cSel || a3eSel ||
        (out.passBytes > 256 && out.passBytes <= (128u << 10) && out.drawCount > 0 &&
         out.drawCount <= 128 && pkt.gxStartValid);
    ok(A2cReady_PassSelected, selected);

    out.readyFlags = ready;
    out.failFlags = fail;
    out.replayReady = (fail == 0) && selected && reqUnres == 0;
    A3c_NotePass(out.passBytes, deps, reqUnres, pkt.summary.flags, out.replayReady);

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
                      "ready fam=%s passBytes=%u draws=%u deps=%u "
                      "(dl=%u tex=%u vert=%u tmem=%u efbC=%u/%u x=%u) efbIn=%08x/%08x",
                      A2c_FamilyName(out.family), out.passBytes, out.drawCount, deps, out.depDl,
                      out.depTex, out.depVert, out.depTmem, out.depEfbCopySampled, out.depEfbCopy,
                      out.depEfbCopyCross, out.efbInColorHash, out.efbInDepthHash);
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
    rr.family = pass.family;
    rr.passHash = pass.passHash;
    rr.passBytes = pass.passBytes;
    rr.drawCount = pass.drawCount;
    rr.depCount = pass.depCount;
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
    uint32_t lastDepthHash = 0;
    auto replayOnce = [&](bool doCompare) -> uint32_t {
        g_gx = pkt.gxStart;
        Render_ResetHostDrawState();
        if (!Render_ScratchBeginFromIncoming(pkt.efbIncomingSlot, &sw, &sh))
            return 0;
        rr.scratchW = sw;
        rr.scratchH = sh;
        rr.isolationOk = true;
        rr.seededFromIncoming = true;

        if (doCompare)
        {
            // Incoming equivalence: scratch contents after seed == archived incoming.
            rr.seedColorHash = Render_HashScratchColor();
            rr.seedDepthHash = Render_HashScratchDepth();
            rr.seedColorMatch =
                (rr.seedColorHash != 0 && rr.seedColorHash == rr.efbInColorHash);
            rr.seedDepthMatch =
                (rr.seedDepthHash != 0 && rr.seedDepthHash == rr.efbInDepthHash);
            // Re-bind scratch targets after hash unbind.
            Render_SetReplayIsolation(true);
            // SetEFBTarget is internal — BeginFromIncoming rebinds cleanly without clear.
            if (!Render_ScratchBeginFromIncoming(pkt.efbIncomingSlot, &sw, &sh))
                return 0;
        }

        // A3a: seed override into gxStart XF before FIFO loads; XF hook re-applies after.
        // Suspended during primary live-vs-scratch MATCH; probe enables it explicitly.
        if (A3a_MatrixOverrideEnabled() && !A3a_OverrideSuspended())
            A3a_ApplyMatrixOverride();
        // A3b/A3c: camera delta left-multiply. A3c natural archives are at curr pose.
        if (A3b_CameraInterpEnabled() && !A3b_CameraSuspended())
        {
            const bool basePrev = !(A3c_EnvInterpEnabled() && pass.family == A2cFamily_A3cEnv);
            A3b_BeginScratchCamera(A3b_CameraAlpha(), basePrev);
        }

        GX_ReplayCommands(pkt.cmd.data(), pass.passBytes);

        if (A3b_ScratchCameraActive())
            A3b_EndScratchCamera();

        if (doCompare)
        {
            rr.efbFeedbackOk = (pass.depEfbCopySampled > 0);
            // Prefer async staging+query so Map/CPU walk leave the XFB path.
            // Force sync for the first few attempts / SCR_A2C_COMPARE_SYNC=1 /
            // after any mismatch (s_forcedCompareLeft).
            static int syncEnv = -1;
            if (syncEnv < 0)
            {
                const char* v = std::getenv("SCR_A2C_COMPARE_SYNC");
                syncEnv = (v && v[0] && v[0] != '0') ? 1 : 0;
            }
            // A3c probes need imageMatch before async settles — force sync for A3cEnv.
            const bool forceSync = syncEnv || s_forcedCompareLeft > 0 ||
                                   (pass.family == A2cFamily_A3cEnv && A3c_EnvInterpEnabled());
            if (forceSync)
            {
                RenderEfbCompare cmp{};
                if (Render_CompareLiveVsScratch(cmp, true))
                {
                    rr.compareStatus = A2cCompare_Completed;
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
                    rr.imageMatch = cmp.colorExact && cmp.depthCompared && cmp.depthExact &&
                                    rr.seedColorMatch && rr.seedDepthMatch;
                    if (rr.imageMatch && s_forcedCompareLeft)
                        s_forcedCompareLeft--;
                    if (!rr.imageMatch)
                        s_forcedCompareLeft = 4;  // mismatch → re-enable sync investigation
                    s_compareDone++;
                }
                else
                {
                    rr.compareStatus = A2cCompare_Failed;
                    s_compareFailed++;
                }
            }
            else
            {
                const int ticket = Render_CompareBeginAsync(true);
                if (ticket >= 0)
                {
                    rr.compareStatus = A2cCompare_Pending;
                    s_comparePending++;
                    // Stash ticket in comparedPixels temporarily for poll (unique id).
                    rr.comparedPixels = (uint32_t)ticket | 0x80000000u;
                }
                else
                {
                    // Queue full — skip rather than stall; never count as MATCH.
                    rr.compareStatus = A2cCompare_Skipped;
                    s_compareSkipped++;
                }
            }
        }

        lastDepthHash = Render_HashScratchDepth();
        return Render_ScratchEndHash();
    };

    // Keep A3a/A3b overrides off for primary live-vs-scratch acceptance.
    A3a_SuspendOverride(true);
    A3b_SuspendCamera(true);
    const uint32_t h1 = replayOnce(true);
    // Determinism probe: skip second full replay once family has MATCH'd (costly).
    bool skipDet = false;
    if (pass.family < kFamilySlots && s_haveFamilyReplay[pass.family] &&
        s_familyReplay[pass.family].imageMatch)
        skipDet = true;
    const A2cReplayResult saved = rr;
    uint32_t h2 = h1;
    if (!skipDet)
        h2 = replayOnce(false);
    rr = saved;
    rr.attempted = true;
    if (!rr.imageCompared && rr.compareStatus != A2cCompare_Pending)
        rr.scratchEfbHash = h1;
    const bool deterministic = (h1 != 0 && h1 == h2);
    rr.completed = deterministic && rr.seededFromIncoming && rr.isolationOk;

    // A3a probe before cleanup (needs incoming EFB slot + isolation).
    if (A3a_MatrixOverrideEnabled() && rr.completed && h1 != 0)
    {
        const uint32_t mode = A3a_OverrideMode();
        // Baseline: still suspended.
        const uint32_t hBase = replayOnce(false);
        A3a_SuspendOverride(false);
        A3a_ResetOverrideStats();
        const uint32_t hOvr = replayOnce(false);
        A3a_SuspendOverride(true);
        A3a_NoteReplayHashes(hBase, hOvr, mode == 1);
        if (mode == 1 && hBase == hOvr && A3a_GetOverrideStats().exercised && rr.imageMatch)
            LOG_INFO("A3a TestA: noop substitution MATCH+exercised row={}", A3a_OverrideRow());
        if (mode == 2 && hBase != 0 && hOvr != 0 && hBase != hOvr &&
            A3a_GetOverrideStats().exercised)
            LOG_INFO("A3a TestB: controlled tx substitution DIFF as expected row={}",
                     A3a_OverrideRow());
    }

    // A3b camera probes: α=0 / α=1 endpoints + configured α midpoint.
    // Skip while the live pair is still discontinuous (boot proj changes).
    // When A3c natural probes run, skip A3b to avoid duplicate GPU cost (~20 Hz).
    A3bCameraPair a3bPair{};
    const bool a3bPairOk =
        A3b_CameraInterpEnabled() && A3b_GetPair(a3bPair) && a3bPair.interpolable;
    // A3c owns endpoint/mid probes on A3cEnv; skip A3b set to avoid ~4× GPU cost.
    const bool skipA3bForA3c = A3c_EnvInterpEnabled() && pass.family == A2cFamily_A3cEnv;
    if (!skipA3bForA3c && A3b_CameraInterpEnabled() && rr.completed && h1 != 0 && rr.imageMatch &&
        a3bPairOk)
    {
        A3b_SuspendCamera(true);
        const uint32_t hBase = replayOnce(false);
        // Test A — α=0 (previous pose / identity delta)
        {
            A3b_SetProbeAlpha(0.f);
            A3b_SuspendCamera(false);
            A3b_ResetCameraStats();
            const uint32_t h0 = replayOnce(false);
            A3b_SuspendCamera(true);
            A3b_NoteCameraHashes(hBase, h0);
            if (hBase == h0 && A3b_GetCameraStats().exercised)
                LOG_INFO("A3b TestA: α=0 endpoint MATCH+exercised");
            else if (hBase == h0 && !A3b_GetCameraStats().exercised)
                LOG_INFO("A3b TestA: α=0 hash match but not exercised (fail={:03x})",
                         A3b_GetCameraStats().failBits);
            A3b_SetProbeAlpha(NAN);
        }
        // Test B — α=1 (current pose). Distinct from baseline only when view moved.
        {
            A3b_SetProbeAlpha(1.f);
            A3b_SuspendCamera(false);
            A3b_ResetCameraStats();
            const uint32_t h1a = replayOnce(false);
            A3b_SuspendCamera(true);
            A3b_NoteCameraHashes(hBase, h1a);
            const A3bCameraStats st = A3b_GetCameraStats();
            if (st.exercised && hBase == h1a)
                LOG_INFO("A3b TestB: α=1 exercised; hash==baseline (view unchanged or archive=curr)");
            else if (st.exercised && hBase != h1a)
                LOG_INFO("A3b TestB: α=1 exercised; hash≠baseline (curr pose on prev archive)");
            else
                LOG_INFO("A3b TestB: not exercised (fail={:03x})", st.failBits);
            A3b_SetProbeAlpha(NAN);
        }
        // Test C — configured α (default 0.5)
        {
            A3b_SuspendCamera(false);
            A3b_ResetCameraStats();
            const uint32_t hMid = replayOnce(false);
            A3b_SuspendCamera(true);
            A3b_NoteCameraHashes(hBase, hMid);
            const A3bCameraStats st = A3b_GetCameraStats();
            if (st.exercised && st.midpointDistinct)
                LOG_INFO("A3b TestC: α={:.2f} midpoint distinct from baseline (hash)", st.alpha);
            else if (st.exercised && !st.midpointDistinct)
                LOG_INFO("A3b TestC: α={:.2f} exercised but hash==baseline (no cam delta / UI pass)",
                         st.alpha);
            else
                LOG_INFO("A3b TestC: skipped or not exercised (fail={:03x})", st.failBits);
        }
    }

    // A3c.1 natural env probes (no FORCE_VIEW_DX). Archive embeds V_curr → baseIsPrev=false.
    // Endpoint asymmetry root cause: α=0 was compared to the *same* archive's live image
    // (frame N+1). That DIFF is expected camera-only change — not a failed cross-frame
    // previous endpoint. Labels: archive_frame / reference_frame / camera_frame / alpha.
    if (A3c_EnvInterpEnabled() && pass.family == A2cFamily_A3cEnv && rr.completed && h1 != 0 &&
        rr.imageMatch)
    {
        if (!A3c_NaturalInterpEligible())
            LOG_INFO("A3c probes skipped (eligible=0 fail={:03x})", A3c_GetStats().failBits);
        else if (!A3c_TakeProbeSlot())
            LOG_INFO("A3c probes skipped (probe budget exhausted used={})", A3c_ProbeSlotsUsed());
        else
        {
            A3bCameraPair frozen{};
            A3b_GetPair(frozen);
            A3b_FreezePair(frozen);
            const uint64_t archiveFrame = pass.xfbIndex;
            const uint64_t camPrevFrame = frozen.prev.xfbIndex;
            const uint64_t camCurrFrame = frozen.curr.xfbIndex;
            const bool geomOk = A3c_EnvGeomUnchanged();
            A3c_LogCoverage(archiveFrame);
            LOG_INFO("A3c probe-context: archive_frame={} reference_frame={} "
                     "cam_prev_frame={} cam_curr_frame={} baseIsPrev=0 (Vbase=V_curr) "
                     "v0={:08x} v1={:08x} geomUnchanged={} probeSlot={}",
                     archiveFrame, archiveFrame, camPrevFrame, camCurrFrame, frozen.prev.viewHash,
                     frozen.curr.viewHash, geomOk ? 1 : 0, A3c_ProbeSlotsUsed());

            A3b_SuspendCamera(true);
            A3b_ClearRowMask();
            // Test A — same-archive identity (no camera modification).
            const uint32_t hIdent = replayOnce(false);
            const uint32_t dIdent = lastDepthHash;
            // Primary replay already exact-MATCH'd live colour+depth; identity re-hash must equal h1.
            const bool identMatch = (hIdent == h1 && rr.imageMatch && dIdent != 0);
            LOG_INFO("A3c TestA identity: archive_frame={} reference_frame={} camera_frame=none "
                     "alpha=n/a color={:08x}/{:08x} depth={:08x} MATCH={}",
                     archiveFrame, archiveFrame, hIdent, h1, dIdent, identMatch ? 1 : 0);
            if (identMatch)
                LOG_INFO("A3c TestA: same-archive identity exact MATCH (colour+depth vs live)");

            auto runAlpha = [&](float a, const char* tag) -> uint32_t {
                A3b_SetProbeAlpha(a);
                A3b_SuspendCamera(false);
                A3b_ResetCameraStats();
                A3b_ClearRowMask();  // PNMTX0..9 + XF-load re-hook (once per load)
                const uint32_t h = replayOnce(false);
                A3b_SuspendCamera(true);
                A3b_NoteCameraHashes(hIdent, h);
                A3b_SetProbeAlpha(NAN);
                const A3bCameraStats st = A3b_GetCameraStats();
                LOG_INFO("A3c {}: archive_frame={} reference_frame={} camera_frame={} alpha={:.2f} "
                         "color={:08x}/{:08x} depth={:08x}/{:08x} applies={} xfHooks={} "
                         "rows={} delta={:08x} endMatch={}",
                         tag, archiveFrame, archiveFrame,
                         (a >= 0.999f)   ? camCurrFrame
                         : (a <= 0.001f) ? camPrevFrame
                                         : archiveFrame,
                         a, h, hIdent, lastDepthHash, dIdent, st.applyCount, st.xfHookCount,
                         st.rowsTouched, st.deltaHash, st.endpointMatch ? 1 : 0);
                return h;
            };

            // Test B — α=1 current endpoint on this archive (identity delta).
            const uint32_t hCurr = runAlpha(1.f, "TestB");
            const uint32_t dCurr = lastDepthHash;
            const A3bCameraStats stB = A3b_GetCameraStats();
            if (hIdent == hCurr && dIdent == dCurr && stB.exercised && stB.endpointMatch)
                LOG_INFO("A3c TestB: α=1 current endpoint exact MATCH+exercised "
                         "(applies={} xfHooks={} — delta applied once at begin, then per XF load)",
                         stB.applyCount, stB.xfHookCount);
            else
                LOG_INFO("A3c TestB: α=1 incomplete MATCH={} exercised={} endMatch={}",
                         (hIdent == hCurr && dIdent == dCurr) ? 1 : 0, stB.exercised ? 1 : 0,
                         stB.endpointMatch ? 1 : 0);

            // Test C — α=0 previous camera on *current* archive.
            // Not a cross-frame live-N match: same N+1 geometry/resources, different camera.
            const uint32_t hPrevCam = runAlpha(0.f, "TestC");
            const uint32_t dPrevCam = lastDepthHash;
            const A3bCameraStats stC = A3b_GetCameraStats();
            A3bMtx3x4 cam0{}, cam1{}, viewA{};
            const bool invOk =
                A3b_MtxInvertRigid(frozen.prev.view, cam0) && A3b_MtxInvertRigid(frozen.curr.view, cam1);
            float camDist = 0.f;
            if (invOk)
            {
                const float dx = cam0.m[3] - cam1.m[3];
                const float dy = cam0.m[7] - cam1.m[7];
                const float dz = cam0.m[11] - cam1.m[11];
                camDist = std::sqrt(dx * dx + dy * dy + dz * dz);
            }
            // Project a representative world point (camera-mid between poses) for geometry check.
            A3bVec3 midWorld{ 0.5f * (cam0.m[3] + cam1.m[3]), 0.5f * (cam0.m[7] + cam1.m[7]),
                              0.5f * (cam0.m[11] + cam1.m[11]) };
            // Offset forward along -Z of prev camera for a visible scene point.
            A3bVec3 probePt{ midWorld.x, midWorld.y, midWorld.z - 8.f };
            A3bVec3 inPrev = A3b_MtxTransformPoint(frozen.prev.view, probePt);
            A3bVec3 inCurr = A3b_MtxTransformPoint(frozen.curr.view, probePt);
            const float screenDx = inCurr.x - inPrev.x;
            const float screenDy = inCurr.y - inPrev.y;
            const bool camOnlyDiff =
                stC.exercised && (hPrevCam != hIdent || dPrevCam != dIdent) && camDist > 1e-3f;
            LOG_INFO("A3c TestC: α=0 previous-camera on archive (camera_only_change) "
                     "geomUnchanged={} colorDiff={} depthDiff={} camDist={:.4f} "
                     "viewSpaceΔ=({:.3f},{:.3f}) — NOT compared to live frame {}",
                     geomOk ? 1 : 0, (hPrevCam != hIdent) ? 1 : 0, (dPrevCam != dIdent) ? 1 : 0,
                     camDist, screenDx, screenDy, camPrevFrame);
            if (camOnlyDiff && geomOk)
                LOG_INFO("A3c TestC: expected DIFF from archive baseline (valid camera-only "
                         "experiment; cross-frame live match requires Test D)");
            else if (camOnlyDiff && !geomOk)
                LOG_INFO("A3c TestC: DIFF observed but env geom/resources not fully unchanged "
                         "(fail={:03x}) — do not treat as pure camera proof",
                         A3c_GetStats().failBits);

            // Test D — prior A3cEnv archive identity already proven when that frame MATCH'd.
            static uint64_t s_prevA3cArchive = 0;
            static uint32_t s_prevA3cColor = 0, s_prevA3cDepth = 0;
            static bool s_havePrevA3cIdent = false;
            if (s_havePrevA3cIdent && s_prevA3cColor != 0)
            {
                LOG_INFO("A3c TestD: prior archive_frame={} reference_frame={} camera_frame={} "
                         "alpha=identity color={:08x} depth={:08x} — exact MATCH established when "
                         "that archive was accepted (cross-frame previous endpoint)",
                         s_prevA3cArchive, s_prevA3cArchive, s_prevA3cArchive, s_prevA3cColor,
                         s_prevA3cDepth);
            }
            else
                LOG_INFO("A3c TestD: no prior A3cEnv identity yet (first probe set this run)");
            if (identMatch)
            {
                s_prevA3cArchive = archiveFrame;
                s_prevA3cColor = hIdent;
                s_prevA3cDepth = dIdent;
                s_havePrevA3cIdent = true;
            }

            // Midpoint — geometric validation; depth ≠ endpoints is expected under motion.
            const uint32_t hMid = runAlpha(0.5f, "TestMid");
            const uint32_t dMid = lastDepthHash;
            A3b_SetProbeAlpha(0.5f);
            A3b_SuspendCamera(false);
            A3b_ResetCameraStats();
            A3b_ClearRowMask();
            const uint32_t hMid2 = replayOnce(false);
            const uint32_t dMid2 = lastDepthHash;
            A3b_SuspendCamera(true);
            A3b_NoteCameraHashes(hIdent, hMid2);
            A3b_SetProbeAlpha(NAN);
            const A3bCameraStats midSt = A3b_GetCameraStats();
            uint32_t ifail = 0;
            A3bMtx3x4 camA{};
            const bool viewAOk =
                A3b_InterpolateView(frozen.prev.view, frozen.curr.view, 0.5f, viewA, &ifail) &&
                invOk && A3b_MtxInvertRigid(viewA, camA);
            const float lx = 0.5f * (cam0.m[3] + cam1.m[3]);
            const float ly = 0.5f * (cam0.m[7] + cam1.m[7]);
            const float lz = 0.5f * (cam0.m[11] + cam1.m[11]);
            const bool midBetween = viewAOk && camDist > 1e-3f &&
                                    std::fabs(camA.m[3] - lx) < 1e-3f &&
                                    std::fabs(camA.m[7] - ly) < 1e-3f &&
                                    std::fabs(camA.m[11] - lz) < 1e-3f;
            A3bVec3 inMid = A3b_MtxTransformPoint(viewA, probePt);
            const float midFracX =
                (std::fabs(screenDx) > 1e-4f) ? (inMid.x - inPrev.x) / screenDx : 0.5f;
            const bool mathDistinct = midSt.viewAHash != 0 && midSt.viewAHash != midSt.view0Hash &&
                                      midSt.viewAHash != midSt.view1Hash && midSt.deltaHash != 0 &&
                                      !midSt.endpointMatch;
            const bool colorMidDiff = (hMid != 0 && hMid == hMid2 && hMid != hIdent && hMid != hPrevCam);
            const bool depthMidDiff =
                (dMid != 0 && dMid == dMid2 && dMid != dIdent && (dPrevCam == 0 || dMid != dPrevCam));
            // Depth classification (mid must not equal endpoints under motion).
            const char* depthClass = "unchanged";
            if (depthMidDiff && colorMidDiff)
                depthClass = "expected_camera_motion";
            else if (depthMidDiff && !colorMidDiff)
                depthClass = "depth_only_camera_motion";
            else if (!depthMidDiff && colorMidDiff)
                depthClass = "color_moved_depth_stable";
            else if (!geomOk)
                depthClass = "geom_changed_inconclusive";
            LOG_INFO("A3c TestMid: α=0.5 archive_frame={} camera_frame=interp "
                     "color={:08x}/{:08x} depth={:08x}/{:08x} det={} camLerp={} camDist={:.4f} "
                     "viewMidFracX={:.3f} depthClass={} vA={:08x} d={:08x} applies={} xfHooks={}",
                     archiveFrame, hMid, hIdent, dMid, dIdent, (hMid == hMid2 && dMid == dMid2) ? 1 : 0,
                     midBetween ? 1 : 0, camDist, midFracX, depthClass, midSt.viewAHash,
                     midSt.deltaHash, midSt.applyCount, midSt.xfHookCount);
            if (colorMidDiff && midSt.exercised && mathDistinct && midBetween)
                LOG_INFO("A3c TestMid: natural midpoint COLOR+DEPTH DIFF+deterministic OK");
            else if (depthMidDiff && midSt.exercised && mathDistinct && midBetween)
                LOG_INFO("A3c TestMid: natural midpoint DEPTH-DIFF+deterministic OK "
                         "(depthClass={}) — not an endpoint failure",
                         depthClass);
            (void)hCurr;
            (void)dCurr;
            A3b_ClearFrozenPair();
        }
    }

    A3a_SuspendOverride(false);
    A3b_SuspendCamera(false);
    A3b_SetProbeAlpha(NAN);
    A3b_ClearRowMask();
    A3b_ClearFrozenPair();

    cleanup();

    const char* verdict = "DIFF";
    if (rr.compareStatus == A2cCompare_Pending)
        verdict = "PENDING";
    else if (rr.compareStatus == A2cCompare_Skipped)
        verdict = "SKIPPED";
    else if (rr.imageMatch)
        verdict = "MATCH";
    else if (rr.colorExact && !rr.depthCompared)
        verdict = "COLOR_MATCH";
    else if (rr.colorExact && rr.depthCompared && !rr.depthExact)
        verdict = "DEPTH_DIFF";
    else if (!rr.seedColorMatch || !rr.seedDepthMatch)
        verdict = "SEED_DIFF";
    std::snprintf(rr.detail, sizeof(rr.detail),
                  "fam=%s xfb=%llu pass=%u deps=%u efbC=%u/%u seed=%d/%d det=%d "
                  "colorMis=%u depthMis=%u maxΔ=%u dCmp=%d cmp=%u %s",
                  A2c_FamilyName(rr.family), (unsigned long long)pass.xfbIndex, pass.passBytes,
                  pass.depCount, pass.depEfbCopySampled, pass.depEfbCopy,
                  rr.seedColorMatch ? 1 : 0, rr.seedDepthMatch ? 1 : 0, deterministic ? 1 : 0,
                  rr.colorMismatch, rr.depthMismatch, rr.maxChannelDelta,
                  rr.depthCompared ? 1 : 0, rr.compareStatus, verdict);
    LOG_INFO("A2f replay: {} liveC={:08x} scrC={:08x} liveD={:08x} scrD={:08x} inC={:08x} inD={:08x} "
             "q={}/{}",
             rr.detail, rr.liveEfbHash, rr.scratchEfbHash, rr.liveDepthHash,
             rr.scratchDepthHash, rr.efbInColorHash, rr.efbInDepthHash,
             Render_CompareQueueDepth(), s_compareSkipped);
    A3d_NoteReplay(rr.family, pass.passBytes, pass.replayReady, rr.imageMatch);
}

struct A2gSeqReady
{
    bool ready = false;
    uint32_t resolved = 0;
    uint32_t edges = 0;
    uint64_t producerXfb = 0;
    char reason[160]{};
};

A2gSeqReady EvaluateCrossSequence(const A2bArchivePacket& producer, const A2bArchivePacket& consumer,
                                  const A2cPassReport& prodPass, const A2cPassReport& consPass)
{
    A2gSeqReady s{};
    s.edges = consumer.crossEdgeCount;
    s.producerXfb = producer.summary.xfbIndex;
    if (!prodPass.replayReady)
    {
        std::snprintf(s.reason, sizeof(s.reason), "producer not ready: %s", prodPass.reason);
        return s;
    }
    if (!consPass.replayReady)
    {
        std::snprintf(s.reason, sizeof(s.reason), "consumer not ready: %s", consPass.reason);
        return s;
    }
    if (consumer.crossEdgeCount == 0)
    {
        std::snprintf(s.reason, sizeof(s.reason), "no cross-frame EFB edges");
        return s;
    }
    uint32_t resolved = 0;
    for (uint32_t i = 0; i < consumer.crossEdgeCount; i++)
    {
        A2bResource pr{};
        if (FindProducerForEdge(producer, consumer.crossEdges[i], &pr) &&
            (pr.flags & A2bRes_Copied) && !(pr.flags & A2bRes_Unresolved))
            resolved++;
    }
    s.resolved = resolved;
    if (resolved != consumer.crossEdgeCount)
    {
        std::snprintf(s.reason, sizeof(s.reason), "unresolved cross edges %u/%u (prodXfb=%llu)",
                      resolved, consumer.crossEdgeCount,
                      (unsigned long long)producer.summary.xfbIndex);
        return s;
    }
    s.ready = true;
    std::snprintf(s.reason, sizeof(s.reason),
                  "seq ready prodXfb=%llu consXfb=%llu edges=%u",
                  (unsigned long long)producer.summary.xfbIndex,
                  (unsigned long long)consumer.summary.xfbIndex, resolved);
    return s;
}

void SynthesizeSelftestEdges(const A2bArchivePacket& producer, A2bArchivePacket& consumer)
{
    // Controlled A2g: no live cross-frame samples observed on title/menu. Use producer's
    // EFB copy dests as the retained set the consumer must sample under skip-copy mode.
    consumer.crossEdgeCount = 0;
    for (uint32_t i = 0; i < producer.resCount && consumer.crossEdgeCount < 64; i++)
    {
        const A2bResource& r = producer.resources[i];
        if (r.type != A2bRes_EfbCopy || !r.genId) continue;
        A2bCrossEfbEdge& e = consumer.crossEdges[consumer.crossEdgeCount++];
        e = A2bCrossEfbEdge{};
        e.destAddr = r.guestEa;
        e.genId = r.genId;
        e.createFrame = r.createFrame;
        e.createXfb = r.createXfb ? r.createXfb : producer.summary.xfbIndex;
        e.sampleXfb = consumer.summary.xfbIndex;
        e.sampleCount = 1;
    }
}

bool SelftestPairOk(const A2cPassReport& prod, const A2cPassReport& cons)
{
    // Two consecutive A2f-scale passes with EFB copies — static enough for retention proof.
    auto a2fish = [](const A2cPassReport& p) {
        return p.replayReady && p.depEfbCopySampled >= 1 && p.passBytes >= 4000 &&
               p.passBytes <= 7000 && p.drawCount >= 60 && p.drawCount <= 75;
    };
    return a2fish(prod) && a2fish(cons);
}

void RunCrossFrameReplay(A2bArchivePacket& producer, A2bArchivePacket& consumer,
                         const A2cPassReport& prodPass, const A2cPassReport& consPass,
                         const A2gSeqReady& seq, A2cReplayResult& rr, bool selftest)
{
    rr = A2cReplayResult{};
    rr.attempted = true;
    rr.family = A2cFamily_A2gCrossEfb;
    rr.passHash = consPass.passHash;
    rr.passBytes = consPass.passBytes;
    rr.drawCount = consPass.drawCount;
    rr.depCount = consPass.depCount;
    rr.efbInColorHash = consPass.efbInColorHash;
    rr.efbInDepthHash = consPass.efbInDepthHash;
    rr.producerXfb = seq.producerXfb;
    rr.consumerXfb = consPass.consumerXfb ? consPass.consumerXfb : consumer.summary.xfbIndex;
    rr.crossCopyTotal = seq.resolved;
    std::snprintf(rr.detail, sizeof(rr.detail), "starting");

    if (!seq.ready)
    {
        std::snprintf(rr.detail, sizeof(rr.detail), "seq not ready: %s", seq.reason);
        return;
    }

    rr.liveEfbHash = Render_HashLiveEfb();
    rr.liveDepthHash = Render_HashLiveDepth();

    GXState gxSaved = g_gx;
    std::vector<uint8_t> tmemSaved(Render_TmemPtr(), Render_TmemPtr() + Render_TmemSize());
    int32_t tevSaved[4][4];
    memcpy(tevSaved, g_tevKonst, sizeof(tevSaved));

    std::vector<RamSave> ramSavesProd, ramSavesCons;
    ApplyResourcesToGuest(producer, ramSavesProd);

    auto cleanup = [&]() {
        GX_SetPeSuppress(false);
        Render_SetReplayIsolation(false);
        Render_ScratchDropEfbCopies();
        g_gx = gxSaved;
        memcpy(Render_TmemPtr(), tmemSaved.data(), tmemSaved.size());
        memcpy(g_tevKonst, tevSaved, sizeof(tevSaved));
        RestoreGuest(ramSavesCons);
        RestoreGuest(ramSavesProd);
        if (producer.efbIncomingValid)
        {
            Render_EfbIncomingRelease(producer.efbIncomingSlot);
            producer.efbIncomingValid = false;
            producer.efbIncomingSlot = -1;
        }
        if (consumer.efbIncomingValid)
        {
            Render_EfbIncomingRelease(consumer.efbIncomingSlot);
            consumer.efbIncomingValid = false;
            consumer.efbIncomingSlot = -1;
        }
        rr.stateRestored = true;
    };

    GX_SetPeSuppress(true);
    Render_ScratchDropEfbCopies();  // explicit clear — no stale gens

    uint32_t sw = 0, sh = 0;
    g_gx = producer.gxStart;
    Render_ResetHostDrawState();
    if (!Render_ScratchBeginFromIncomingEx(producer.efbIncomingSlot, false, &sw, &sh))
    {
        std::snprintf(rr.detail, sizeof(rr.detail), "producer scratch begin failed");
        cleanup();
        return;
    }
    rr.isolationOk = true;
    rr.seededFromIncoming = true;
    rr.seedColorHash = Render_HashScratchColor();
    rr.seedDepthHash = Render_HashScratchDepth();
    // Producer seed vs its own incoming (not consumer).
    const bool prodSeedC =
        (rr.seedColorHash != 0 && rr.seedColorHash == producer.efbInColorHash);
    const bool prodSeedD =
        (rr.seedDepthHash != 0 && rr.seedDepthHash == producer.efbInDepthHash);
    if (!Render_ScratchBeginFromIncomingEx(producer.efbIncomingSlot, false, &sw, &sh))
    {
        std::snprintf(rr.detail, sizeof(rr.detail), "producer re-seed failed");
        cleanup();
        return;
    }
    GX_ReplayCommands(producer.cmd.data(), prodPass.passBytes);

    // Prove scratch produced the retained dests independently of live.
    uint32_t copyMatch = 0;
    for (uint32_t i = 0; i < consumer.crossEdgeCount; i++)
    {
        const uint32_t dest = consumer.crossEdges[i].destAddr;
        if (!Render_ScratchHasEfbCopy(dest))
            continue;
        const uint32_t liveH = Render_HashLiveEfbCopy(dest);
        const uint32_t scrH = Render_HashScratchEfbCopy(dest);
        if (liveH && scrH && liveH == scrH)
            copyMatch++;
    }
    rr.crossCopyMatch = copyMatch;
    rr.noLiveFallback = true;  // isolation map is separate from live

    // Consumer: retain scratch EFB copies from producer.
    RestoreGuest(ramSavesProd);  // restore producer RAM patches before consumer apply
    ramSavesProd.clear();
    ApplyResourcesToGuest(consumer, ramSavesCons);
    g_gx = consumer.gxStart;
    Render_ResetHostDrawState();
    if (!Render_ScratchBeginFromIncomingEx(consumer.efbIncomingSlot, true, &sw, &sh))
    {
        std::snprintf(rr.detail, sizeof(rr.detail), "consumer scratch begin failed");
        cleanup();
        return;
    }
    rr.scratchW = sw;
    rr.scratchH = sh;
    rr.seedColorHash = Render_HashScratchColor();
    rr.seedDepthHash = Render_HashScratchDepth();
    rr.seedColorMatch =
        (rr.seedColorHash != 0 && rr.seedColorHash == rr.efbInColorHash) && prodSeedC;
    rr.seedDepthMatch =
        (rr.seedDepthHash != 0 && rr.seedDepthHash == rr.efbInDepthHash) && prodSeedD;
    if (!Render_ScratchBeginFromIncomingEx(consumer.efbIncomingSlot, true, &sw, &sh))
    {
        std::snprintf(rr.detail, sizeof(rr.detail), "consumer re-seed failed");
        cleanup();
        return;
    }

    // Confirm retained scratch gens still present before consumer cmds.
    uint32_t retained = 0;
    for (uint32_t i = 0; i < consumer.crossEdgeCount; i++)
        if (Render_ScratchHasEfbCopy(consumer.crossEdges[i].destAddr))
            retained++;
    rr.crossFrameOk = (retained == consumer.crossEdgeCount && retained > 0);

    if (selftest)
        Render_SetScratchSkipEfbTexCopy(true);
    GX_ReplayCommands(consumer.cmd.data(), consPass.passBytes);
    Render_SetScratchSkipEfbTexCopy(false);

    // After consumer cmds, retained gens must still be the ones sampled (not replaced).
    uint32_t stillRetained = 0;
    for (uint32_t i = 0; i < consumer.crossEdgeCount; i++)
        if (Render_ScratchHasEfbCopy(consumer.crossEdges[i].destAddr))
            stillRetained++;
    rr.crossFrameOk = rr.crossFrameOk && (stillRetained == consumer.crossEdgeCount);

    // Sync compare for A2g acceptance (exact everyPixel).
    RenderEfbCompare cmp{};
    if (Render_CompareLiveVsScratch(cmp, true))
    {
        rr.compareStatus = A2cCompare_Completed;
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
        rr.imageMatch = cmp.colorExact && cmp.depthCompared && cmp.depthExact &&
                        rr.seedColorMatch && rr.seedDepthMatch && rr.crossFrameOk &&
                        rr.noLiveFallback;
        s_compareDone++;
        if (!rr.imageMatch)
            s_forcedCompareLeft = 4;
    }
    else
    {
        rr.compareStatus = A2cCompare_Failed;
        s_compareFailed++;
    }

    Render_ScratchEndHash();
    rr.completed = rr.crossFrameOk && rr.isolationOk && rr.seededFromIncoming;
    cleanup();

    const char* verdict = rr.imageMatch ? "MATCH" : "DIFF";
    if (!rr.crossFrameOk)
        verdict = "NO_RETAIN";
    else if (!rr.seedColorMatch || !rr.seedDepthMatch)
        verdict = "SEED_DIFF";
    std::snprintf(rr.detail, sizeof(rr.detail),
                  "fam=A2g-cross xfb=%llu←%llu edges=%u/%u copyH=%u/%u retain=%d self=%d "
                  "seed=%d/%d colorMis=%u depthMis=%u dCmp=%d %s",
                  (unsigned long long)rr.consumerXfb, (unsigned long long)rr.producerXfb,
                  stillRetained, consumer.crossEdgeCount, rr.crossCopyMatch, rr.crossCopyTotal,
                  rr.crossFrameOk ? 1 : 0, selftest ? 1 : 0, rr.seedColorMatch ? 1 : 0,
                  rr.seedDepthMatch ? 1 : 0, rr.colorMismatch, rr.depthMismatch,
                  rr.depthCompared ? 1 : 0, verdict);
    LOG_INFO("A2g replay: {} liveC={:08x} scrC={:08x} liveD={:08x} scrD={:08x}", rr.detail,
             rr.liveEfbHash, rr.scratchEfbHash, rr.liveDepthHash, rr.scratchDepthHash);
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

static void PollPendingComparesLocked()
{
    // Complete any async compare tickets stashed on family replay results.
    for (uint32_t f = 0; f < kFamilySlots; f++)
    {
        if (!s_haveFamilyReplay[f]) continue;
        A2cReplayResult& rr = s_familyReplay[f];
        if (rr.compareStatus != A2cCompare_Pending) continue;
        if ((rr.comparedPixels & 0x80000000u) == 0) continue;
        const int ticket = (int)(rr.comparedPixels & 0x7FFFFFFFu);
        RenderEfbCompare cmp{};
        bool pending = false;
        if (!Render_ComparePoll(ticket, cmp, &pending))
        {
            if (pending) continue;
            rr.compareStatus = A2cCompare_Failed;
            s_compareFailed++;
            if (s_comparePending) s_comparePending--;
            continue;
        }
        if (s_comparePending) s_comparePending--;
        s_compareDone++;
        rr.compareStatus = A2cCompare_Completed;
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
        rr.imageMatch = cmp.colorExact && cmp.depthCompared && cmp.depthExact &&
                        rr.seedColorMatch && rr.seedDepthMatch;
        if (!rr.imageMatch)
            s_forcedCompareLeft = 4;
        LOG_INFO("A2f async: fam={} {} colorMis={} depthMis={} liveC={:08x} scrC={:08x}",
                 A2c_FamilyName(rr.family), rr.imageMatch ? "MATCH" : "DIFF", rr.colorMismatch,
                 rr.depthMismatch, rr.liveEfbHash, rr.scratchEfbHash);
        if (rr.family == s_lastReplay.family)
            s_lastReplay = rr;
    }
}

void A2c_OnArchivedFrame()
{
    A2bArchivePacket pkt;
    if (!A2b_CloneCurrentPacket(pkt)) return;

    A2cPassReport report{};
    EvaluatePass(pkt, report);

    // A2g: natural cross edges, or controlled selftest on consecutive A2f passes.
    A2bArchivePacket prevPkt{};
    A2cPassReport prevReport{};
    A2gSeqReady seq{};
    bool havePrev = false;
    bool a2gSelftest = false;
    if (A2b_ClonePreviousPacket(prevPkt))
    {
        havePrev = true;
        EvaluatePass(prevPkt, prevReport);
        // Synthetic retain selftest is opt-in (SCR_A2G_SELFTEST=1). Default on when
        // scratch replay is enabled, unless natural-trace-only discovery sets it off.
        static int selfEnv = -1;
        if (selfEnv < 0)
        {
            const char* v = std::getenv("SCR_A2G_SELFTEST");
            if (v && v[0] == '0')
                selfEnv = 0;
            else
                selfEnv = 1;  // default: keep A2g synthetic regression
        }
        if (selfEnv && pkt.crossEdgeCount == 0 && SelftestPairOk(prevReport, report))
        {
            SynthesizeSelftestEdges(prevPkt, pkt);
            a2gSelftest = pkt.crossEdgeCount > 0;
        }
        if (pkt.crossEdgeCount > 0)
        {
            seq = EvaluateCrossSequence(prevPkt, pkt, prevReport, report);
            report.crossResolved = seq.resolved;
            report.crossEdgeCount = pkt.crossEdgeCount;
            report.depEfbCopyCross = pkt.crossEdgeCount;
            report.producerXfb = seq.producerXfb;
            if (seq.ready)
            {
                report.family = A2cFamily_A2gCrossEfb;
                report.readyFlags |= A2cReady_CrossFrame;
                report.failFlags &= ~A2cReady_CrossFrame;
                std::snprintf(report.reason, sizeof(report.reason), "%s%s", seq.reason,
                              a2gSelftest ? " (selftest retain)" : "");
                report.replayReady = true;
            }
            else
            {
                report.readyFlags &= ~A2cReady_CrossFrame;
                report.failFlags |= A2cReady_CrossFrame;
                if (report.family == A2cFamily_A2gCrossEfb)
                    report.replayReady = false;
                std::snprintf(report.reason, sizeof(report.reason), "%s", seq.reason);
            }
        }
        if (!seq.ready && prevPkt.efbIncomingValid)
        {
            Render_EfbIncomingRelease(prevPkt.efbIncomingSlot);
            prevPkt.efbIncomingValid = false;
            havePrev = false;
        }
    }

    {
        std::lock_guard lk(s_mu);
        PollPendingComparesLocked();
        s_pass = report;
        s_havePass = true;
        if (report.family < kFamilySlots)
        {
            s_familyPass[report.family] = report;
            s_haveFamilyPass[report.family] = true;
        }
    }

    static uint64_t lastLogMs = 0;
    const uint64_t now = Platform_TickMs();
    const bool namedFamily = report.family == A2cFamily_A2dIdle ||
                             report.family == A2cFamily_A2eMultiTex ||
                             report.family == A2cFamily_A2fEfbFb ||
                             report.family == A2cFamily_A2gCrossEfb ||
                             report.family == A2cFamily_A3cEnv ||
                             report.family == A2cFamily_A3eDense;
    if (pkt.crossEdgeCount > 0 && seq.ready && (now - lastLogMs >= 2000))
    {
        LOG_INFO("A2g edges: xfb={} count={} resolved={}/{} prodXfb={} — {}", pkt.summary.xfbIndex,
                 pkt.crossEdgeCount, report.crossResolved, pkt.crossEdgeCount, report.producerXfb,
                 seq.reason);
    }
    if (now - lastLogMs >= 2500 || (report.replayReady && namedFamily && now - lastLogMs >= 800))
    {
        lastLogMs = now;
        LOG_INFO("A2f ready: fam={} xfb={} ready={} flags={:03x}/{:03x} passBytes={} draws={} "
                 "deps={} (dl={} tex={} vert={} tmem={} efbC={}/{} x={}) reqUnres={} efbIn={} — {}",
                 A2c_FamilyName(report.family), report.xfbIndex,
                 report.replayReady ? "yes" : "no", report.readyFlags, report.failFlags,
                 report.passBytes, report.drawCount, report.depCount, report.depDl, report.depTex,
                 report.depVert, report.depTmem, report.depEfbCopySampled, report.depEfbCopy,
                 report.depEfbCopyCross, report.unresolvedRequired,
                 report.efbIncomingValid ? "yes" : "no", report.reason);
    }

    if (!ReplayEnv() || !report.replayReady) return;

    // Prefer A2g sequential replay when the sequence is ready.
    if (report.family == A2cFamily_A2gCrossEfb && seq.ready && havePrev)
    {
        static uint64_t lastA2gMs = 0;
        if (now - lastA2gMs < 2000)
        {
            if (prevPkt.efbIncomingValid)
                Render_EfbIncomingRelease(prevPkt.efbIncomingSlot);
            return;
        }
        {
            std::lock_guard lk(s_mu);
            if (s_haveFamilyReplay[A2cFamily_A2gCrossEfb] &&
                s_familyReplay[A2cFamily_A2gCrossEfb].imageMatch && now - lastA2gMs < 12000)
            {
                if (prevPkt.efbIncomingValid)
                    Render_EfbIncomingRelease(prevPkt.efbIncomingSlot);
                return;
            }
        }
        lastA2gMs = now;
        s_lastReplayMsByFamily[A2cFamily_A2gCrossEfb] = now;
        s_replayBudgetFrames++;
        A2cReplayResult rr{};
        RunCrossFrameReplay(prevPkt, pkt, prevReport, report, seq, rr, a2gSelftest);
        std::lock_guard lk(s_mu);
        s_lastReplay = rr;
        s_familyReplay[A2cFamily_A2gCrossEfb] = rr;
        s_haveFamilyReplay[A2cFamily_A2gCrossEfb] = true;
        return;
    }

    // Skip uncategorised Other once a named family has been observed.
    if (!namedFamily)
    {
        std::lock_guard lk(s_mu);
        if (s_haveFamilyPass[A2cFamily_A2dIdle] || s_haveFamilyPass[A2cFamily_A2eMultiTex] ||
            s_haveFamilyPass[A2cFamily_A2fEfbFb] || s_haveFamilyPass[A2cFamily_A2gCrossEfb] ||
            s_haveFamilyPass[A2cFamily_A3cEnv])
            return;
    }

    // Prefer unmatched named families; back off once A2d+A2e+A2f all MATCH.
    static uint64_t lastReplayMs = 0;
    if (now - lastReplayMs < 1000) return;
    if (report.family < kFamilySlots)
    {
        std::lock_guard lk(s_mu);
        const bool selfMatch =
            s_haveFamilyReplay[report.family] && s_familyReplay[report.family].imageMatch;
        bool allNamedMatch =
            (s_haveFamilyReplay[A2cFamily_A2fEfbFb] &&
             s_familyReplay[A2cFamily_A2fEfbFb].imageMatch) &&
            ((s_haveFamilyReplay[A2cFamily_A2eMultiTex] &&
              s_familyReplay[A2cFamily_A2eMultiTex].imageMatch) ||
             !s_haveFamilyPass[A2cFamily_A2eMultiTex]);
        // Keep re-probing A3cEnv until a natural-camera midpoint has been attempted.
        const bool a3cNeedProbe =
            report.family == A2cFamily_A3cEnv && A3c_EnvInterpEnabled() &&
            A3c_NaturalInterpEligible();
        if (selfMatch && allNamedMatch && !a3cNeedProbe &&
            now - s_lastReplayMsByFamily[report.family] < 10000)
            return;
        if (selfMatch && !allNamedMatch && namedFamily && !a3cNeedProbe &&
            now - s_lastReplayMsByFamily[report.family] < 6000)
            return;
        if (now - s_lastReplayMsByFamily[report.family] < 2000 &&
            s_haveFamilyReplay[report.family] && !a3cNeedProbe)
            return;
    }

    lastReplayMs = now;
    if (report.family < kFamilySlots) s_lastReplayMsByFamily[report.family] = now;
    s_replayBudgetFrames++;

    A2cReplayResult rr{};
    RunScratchReplay(pkt, report, rr);  // releases pkt's EFB slot ref
    std::lock_guard lk(s_mu);
    s_lastReplay = rr;
    if (rr.family < kFamilySlots)
    {
        s_familyReplay[rr.family] = rr;
        s_haveFamilyReplay[rr.family] = true;
    }
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

bool A2c_GetFamilyPassReport(uint32_t family, A2cPassReport& out)
{
    std::lock_guard lk(s_mu);
    if (family >= kFamilySlots || !s_haveFamilyPass[family]) return false;
    out = s_familyPass[family];
    return true;
}

bool A2c_GetFamilyReplay(uint32_t family, A2cReplayResult& out)
{
    std::lock_guard lk(s_mu);
    if (family >= kFamilySlots || !s_haveFamilyReplay[family]) return false;
    out = s_familyReplay[family];
    return true;
}

const char* A2c_FamilyName(uint32_t family)
{
    switch (family)
    {
    case A2cFamily_A2dIdle: return "A2d-idle";
    case A2cFamily_A2eMultiTex: return "A2e-multitex";
    case A2cFamily_A2fEfbFb: return "A2f-efbfb";
    case A2cFamily_A2gCrossEfb: return "A2g-cross";
    case A2cFamily_A3cEnv: return "A3c-env";
    case A2cFamily_Other: return "other";
    case A2cFamily_A3eDense: return "A3e-dense";
    default: return "none";
    }
}

void A2c_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || bufSize == 0) return;
    buf[0] = 0;
    A2cReplayResult a{}, b{}, f{}, g{};
    const bool haveA = A2c_GetFamilyReplay(A2cFamily_A2dIdle, a);
    const bool haveB = A2c_GetFamilyReplay(A2cFamily_A2eMultiTex, b);
    const bool haveF = A2c_GetFamilyReplay(A2cFamily_A2fEfbFb, f);
    const bool haveG = A2c_GetFamilyReplay(A2cFamily_A2gCrossEfb, g);
    if (haveA || haveB || haveF || haveG)
    {
        auto tag = [](bool have, const A2cReplayResult& r) -> const char* {
            if (!have) return "-";
            if (r.compareStatus == A2cCompare_Pending) return "pend";
            if (r.imageMatch) return "MATCH";
            if (r.completed) return "DIFF";
            return "fail";
        };
        std::snprintf(buf, bufSize, "A2d:%s A2e:%s A2f:%s A2g:%s", tag(haveA, a), tag(haveB, b),
                      tag(haveF, f), tag(haveG, g));
        return;
    }
    A2cPassReport p{};
    if (!A2c_GetPassReport(p))
    {
        if (A2c_ScratchReplayEnabled())
            std::snprintf(buf, bufSize, "A2g: waiting");
        return;
    }
    std::snprintf(buf, bufSize, "A2g %s %s pass=%u d=%u efbC=%u x=%u", A2c_FamilyName(p.family),
                  p.replayReady ? "READY" : "no", p.passBytes, p.depCount, p.depEfbCopySampled,
                  p.depEfbCopyCross);
}

bool A2c_DecoupledMidPresent(float alpha, uint32_t* outColorHash, uint32_t* outDraws,
                             char* detail, size_t detailN)
{
    if (detail && detailN) detail[0] = 0;
    if (!A2b_FifoCaptureEnabled())
    {
        if (detail && detailN)
            std::snprintf(detail, detailN, "no A2b archive (set SCR_A2B_FIFO_CAPTURE=1)");
        return false;
    }

    A2cPassReport pass{};
    bool have = A2c_GetFamilyPassReport(A2cFamily_A3eDense, pass) && pass.replayReady;
    if (!have)
        have = A2c_GetFamilyPassReport(A2cFamily_A3cEnv, pass) && pass.replayReady;
    if (!have)
        have = A2c_GetPassReport(pass) && pass.replayReady &&
               (pass.family == A2cFamily_A3eDense || pass.family == A2cFamily_A3cEnv ||
                pass.family == A2cFamily_Other);
    if (!have)
    {
        if (detail && detailN)
            std::snprintf(detail, detailN, "no ready dense/env pass");
        return false;
    }

    A2bArchivePacket pkt{};
    if (!A2b_CloneCurrentPacket(pkt))
    {
        if (detail && detailN)
            std::snprintf(detail, detailN, "clone packet failed");
        return false;
    }

    GXState gxSaved = g_gx;
    std::vector<uint8_t> tmemSaved(Render_TmemPtr(), Render_TmemPtr() + Render_TmemSize());
    int32_t tevSaved[4][4];
    memcpy(tevSaved, g_tevKonst, sizeof(tevSaved));
    std::vector<RamSave> ramSaves;
    ApplyResourcesToGuest(pkt, ramSaves);

    bool ok = false;
    uint32_t colorHash = 0;
    GX_SetPeSuppress(true);
    A3b_SetProbeAlpha(alpha);
    A3a_SuspendOverride(true);  // PoseInterp owns player ViewPos; avoid A3a clash

    auto cleanup = [&]() {
        A3b_SetProbeAlpha(NAN);
        A3a_SuspendOverride(false);
        if (A3b_ScratchCameraActive()) A3b_EndScratchCamera();
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
    };

    uint32_t sw = 0, sh = 0;
    g_gx = pkt.gxStart;
    Render_ResetHostDrawState();
    if (!Render_ScratchBeginFromIncoming(pkt.efbIncomingSlot, &sw, &sh))
    {
        if (detail && detailN)
            std::snprintf(detail, detailN, "scratch begin failed");
        cleanup();
        return false;
    }

    // Curr-pose archive → baseIsPrev=false. Camera delta toward α when A3b on.
    if (A3b_CameraInterpEnabled() && !A3b_CameraSuspended())
        A3b_BeginScratchCamera(alpha, false);

    GX_ReplayCommands(pkt.cmd.data(), pass.passBytes);

    if (A3b_ScratchCameraActive()) A3b_EndScratchCamera();

    ok = Render_PresentScratchUniqueMid(&colorHash);
    if (outColorHash) *outColorHash = colorHash;
    if (outDraws) *outDraws = pass.drawCount;
    if (detail && detailN)
        std::snprintf(detail, detailN, "%s %s draws=%u hash=%08x α=%.3f",
                      ok ? "mid-OK" : "mid-FAIL", A2c_FamilyName(pass.family), pass.drawCount,
                      colorHash, alpha);

    cleanup();
    return ok;
}
