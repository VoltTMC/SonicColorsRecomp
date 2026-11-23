// Approach A3n — live TR1 Sonic pose interpolation (SCR_POSE_INTERP).
#include "pose_interp.h"
#include "pose_qualify.h"
#include "../kernel/memory.h"  // CanonAddr, Read32, Read16
#include "../log.h"
#include "../perf.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_on{ -1 };
std::atomic<int> s_trace{ -1 };
std::atomic<int> s_validate{ -1 };
std::mutex s_mu;

PoseInterpStats s_st{};
float s_fixedAlpha = 0.5f;

// Published output (GPU may read under lock briefly).
struct InterpOut
{
    uint8_t active = 0;
    uint8_t pad[3]{};
    uint32_t viewBaseEa = 0;    // ScnMdl ViewPos pointer
    uint32_t viewActiveEa = 0;  // slot selected at publish
    uint32_t slotBytes = 0;
    uint32_t scnMdlEa = 0;
    uint16_t nView = 0;
    uint16_t nodesInterp = 0;
    uint64_t seqPrev = 0;
    uint64_t seqCurr = 0;
    float alpha = 0.5f;
    PoseMtx3x4 vHat{};
    // Palette-indexed ViewPos' (by mtxID). valid[i]=1 if overridden.
    PoseMtx3x4 viewPos[kPoseMaxNodes]{};
    uint8_t valid[kPoseMaxNodes]{};
};

InterpOut s_out{};
InterpOut s_published{};  // double-buffer publish
uint32_t s_logN = 0;

bool EnvFlag(const char* name)
{
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

float EnvAlpha()
{
    const char* e = std::getenv("SCR_POSE_INTERP_ALPHA");
    if (!e || !e[0]) return 0.5f;
    char* end = nullptr;
    const float v = std::strtof(e, &end);
    if (end == e) return 0.5f;
    if (v < 0.f) return 0.f;
    if (v > 1.f) return 1.f;
    return v;
}

bool On()
{
    int v = s_on.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_POSE_INTERP") ? 1 : 0;
        s_on.store(v, std::memory_order_relaxed);
        s_fixedAlpha = EnvAlpha();
        if (v)
            LOG_INFO("PoseInterp: enabled (SCR_POSE_INTERP); alpha={:.3f} (fixed mid-bracket "
                     "latency; no extra XFBs)",
                     s_fixedAlpha);
    }
    return v != 0;
}

bool TraceOn()
{
    int v = s_trace.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_POSE_INTERP_TRACE") ? 1 : 0;
        s_trace.store(v, std::memory_order_relaxed);
    }
    return v != 0;
}

bool ValidateOn()
{
    int v = s_validate.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_POSE_INTERP_VALIDATE") ? 1 : 0;
        s_validate.store(v, std::memory_order_relaxed);
    }
    return v != 0;
}

void NoteFb(PoseInterpFallback why)
{
    s_st.fallback++;
    if (why < 8) s_st.fbReason[why]++;
}

void PublishInactive()
{
    std::lock_guard<std::mutex> lk(s_mu);
    s_published = {};
}

}  // namespace

bool PoseInterp_Enabled() { return On(); }

bool PoseInterp_Active()
{
    if (!On()) return false;
    std::lock_guard<std::mutex> lk(s_mu);
    return s_published.active != 0;
}

void PoseInterp_OnXfb()
{
    if (!On()) return;
    if (!PoseCapture_Enabled()) return;

    s_st.attempts++;
    PoseSnap prev{}, curr{};
    if (!PoseCapture_GetLatest(curr))
    {
        NoteFb(PoseInterpFb_NoPair);
        PublishInactive();
        return;
    }
    if (!PoseCapture_GetPrevious(prev))
    {
        NoteFb(PoseInterpFb_NoPair);
        PublishInactive();
        return;
    }

    const PosePairVerdict verd = Pose_EvaluatePair(prev, curr);
    if (verd.reject != PosePair_Ok)
    {
        if (verd.reject < 16) s_st.pairReject[verd.reject]++;
        NoteFb(PoseInterpFb_PairReject);
        PublishInactive();
        if (TraceOn() && (s_logN < 8 || (s_logN % 180) == 0))
            LOG_INFO("PoseInterp fallback reject={} seq={}/{}", (unsigned)verd.reject, prev.seq,
                     curr.seq);
        s_logN++;
        return;
    }

    const float alpha = s_fixedAlpha;
    s_st.alphaLast = alpha;

    // Resolve active ViewPos base from current ScnMdl (frozen at this XFB).
    const uint32_t scn = CanonAddr(curr.scnMdlEa);
    if (!scn)
    {
        NoteFb(PoseInterpFb_NoPair);
        PublishInactive();
        return;
    }
    const uint32_t viewBase = CanonAddr(Read32(scn + kPoseScn_ViewPos));
    const uint16_t nView = Read16(scn + kPoseScn_NumView);
    if (!viewBase || nView == 0 || nView > (uint16_t)kPoseMaxNodes)
    {
        NoteFb(PoseInterpFb_NoPair);
        PublishInactive();
        return;
    }
    const uint32_t viewActive = viewBase + curr.viewSlot * curr.slotBytes;

    InterpOut out{};
    out.viewBaseEa = viewBase;
    out.viewActiveEa = viewActive;
    out.slotBytes = curr.slotBytes;
    out.scnMdlEa = scn;
    out.nView = nView;
    out.seqPrev = prev.seq;
    out.seqCurr = curr.seq;
    out.alpha = alpha;

    if (!Pose_InterpRigid(prev.vHat, curr.vHat, alpha, out.vHat))
    {
        NoteFb(PoseInterpFb_Decompose);
        PublishInactive();
        return;
    }

    uint16_t nInterp = 0;
    float relMax = 0;
    for (uint16_t i = 0; i < curr.nodeCount && i < (uint16_t)kPoseMaxNodes; i++)
    {
        if (!curr.nodes[i].worldOk || !prev.nodes[i].worldOk) continue;
        const uint16_t mid = curr.nodes[i].mtxID;
        if (mid >= nView || mid >= kPoseMaxNodes) continue;

        PoseMtx3x4 w{};
        if (!Pose_InterpRigid(prev.nodes[i].world, curr.nodes[i].world, alpha, w))
        {
            NoteFb(PoseInterpFb_Decompose);
            PublishInactive();
            return;
        }
        PoseMtx3x4 vp{};
        Pose_MtxConcat(out.vHat, w, vp);
        if (!Pose_MtxFinite(vp))
        {
            NoteFb(PoseInterpFb_Validate);
            PublishInactive();
            return;
        }
        out.viewPos[mid] = vp;
        out.valid[mid] = 1;
        nInterp++;

        if (ValidateOn())
        {
            // Relationship residual vs V̂'·W' (should be ~0 by construction).
            float r = 0;
            Pose_ValidateNodeRel(out.vHat, w, vp, kPoseRelTol, &r);
            if (r > relMax) relMax = r;
        }
    }

    if (nInterp == 0)
    {
        NoteFb(PoseInterpFb_Decompose);
        PublishInactive();
        return;
    }

    if (ValidateOn())
    {
        int ref = -1;
        for (uint16_t i = 0; i < curr.nodeCount && i < (uint16_t)kPoseMaxNodes; i++)
        {
            if (curr.nodes[i].worldOk && prev.nodes[i].worldOk)
            {
                ref = (int)i;
                break;
            }
        }
        if (ref >= 0)
        {
            PoseMtx3x4 w0{}, w1{};
            if (Pose_InterpRigid(prev.nodes[ref].world, curr.nodes[ref].world, 0.f, w0) &&
                Pose_InterpRigid(prev.nodes[ref].world, curr.nodes[ref].world, 1.f, w1))
            {
                s_st.endpointErr0 = Pose_MtxMaxAbsDiff(w0, prev.nodes[ref].world);
                s_st.endpointErr1 = Pose_MtxMaxAbsDiff(w1, curr.nodes[ref].world);
            }
        }
        s_st.relResMax = relMax;
        if (relMax > kPoseRelTol)
        {
            NoteFb(PoseInterpFb_Validate);
            PublishInactive();
            return;
        }
    }

    out.nodesInterp = nInterp;
    out.active = 1;

    {
        std::lock_guard<std::mutex> lk(s_mu);
        s_published = out;
    }

    s_st.applied++;
    s_st.nodesInterp += nInterp;
    s_st.viewRebuilt += nInterp;

    if (TraceOn() && (s_logN < 8 || (s_logN % 120) == 0))
    {
        LOG_INFO("PoseInterp apply α={:.3f} nodes={} seq={}/{} scn={:08x} viewEa={:08x} "
                 "relMax={:.5f} applied={} gx={} miss={}",
                 alpha, nInterp, prev.seq, curr.seq, scn, viewActive, relMax, s_st.applied,
                 s_st.gxOverrides, s_st.gxMiss);
    }
    s_logN++;
}

bool PoseInterp_OverrideIndexed(uint32_t guestEa, uint32_t index, uint32_t stride,
                                float outMtx12[12])
{
    if (!On() || !outMtx12) return false;
    if (stride != 0x30 && stride != 48) return false;

    std::lock_guard<std::mutex> lk(s_mu);
    if (!s_published.active) return false;
    if (index >= s_published.nView || index >= (uint32_t)kPoseMaxNodes) return false;
    if (!s_published.valid[index])
    {
        s_st.gxMiss++;
        return false;
    }
    // CP array bases are often physical; CanonAddr → 0x8/0x9 cached form.
    const uint32_t ea = CanonAddr(guestEa);
    const uint32_t base = CanonAddr(ea - index * stride);
    const uint32_t slot0 = CanonAddr(s_published.viewBaseEa);
    const uint32_t slot1 = CanonAddr(s_published.viewBaseEa + s_published.slotBytes);
    const uint32_t active = CanonAddr(s_published.viewActiveEa);
    if (base != slot0 && base != slot1 && base != active)
    {
        s_st.gxMiss++;
        static uint32_t s_missLog = 0;
        static uint32_t s_seen[8]{};
        if (TraceOn() && s_missLog < 8 && index < 64)
        {
            bool seen = false;
            for (uint32_t i = 0; i < s_missLog; i++)
                if (s_seen[i] == base) seen = true;
            if (!seen)
            {
                s_seen[s_missLog++] = base;
                LOG_INFO("PoseInterp gxMiss base={:08x} idx={} stride={} expect={:08x}/{:08x}",
                         base, index, stride, slot0, slot1);
            }
        }
        return false;
    }
    std::memcpy(outMtx12, s_published.viewPos[index].m, 12 * sizeof(float));
    s_st.gxOverrides++;
    return true;
}

void PoseInterp_GetStats(PoseInterpStats& out)
{
    std::lock_guard<std::mutex> lk(s_mu);
    out = s_st;
}

void PoseInterp_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!On()) return;
    PoseInterpStats st{};
    PoseInterp_GetStats(st);
    std::snprintf(buf, bufSize, "PoseI α=%.2f apply=%llu fb=%llu gx=%llu", st.alphaLast,
                  (unsigned long long)st.applied, (unsigned long long)st.fallback,
                  (unsigned long long)st.gxOverrides);
}
