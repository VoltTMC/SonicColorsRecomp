// Approach A3n/A3o — live TR1 Sonic pose interpolation (SCR_POSE_INTERP).
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
std::atomic<int> s_timeDriven{ -1 };
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
    PoseMtx3x4 viewPos[kPoseMaxNodes]{};
    uint8_t valid[kPoseMaxNodes]{};
};

InterpOut s_published{};
uint32_t s_logN = 0;
PoseInterpFallback s_lastFb{ PoseInterpFb_None };

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
        const bool td = EnvFlag("SCR_DECOUPLED_PRESENT");
        s_timeDriven.store(td ? 1 : 0, std::memory_order_relaxed);
        if (v)
        {
            if (td)
                LOG_INFO("PoseInterp: enabled; time-driven alpha (SCR_DECOUPLED_PRESENT)");
            else
                LOG_INFO("PoseInterp: enabled; fixed alpha={:.3f} (set SCR_DECOUPLED_PRESENT=1 "
                         "for target-time alpha)",
                         s_fixedAlpha);
        }
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

bool TimeDriven()
{
    On();
    return s_timeDriven.load(std::memory_order_relaxed) != 0;
}

void NoteFb(PoseInterpFallback why)
{
    s_st.fallback++;
    s_lastFb = why;
    if (why < 8) s_st.fbReason[why]++;
}

void PublishInactive()
{
    std::lock_guard<std::mutex> lk(s_mu);
    s_published = {};
}

bool BuildFromPair(const PoseSnap& prev, const PoseSnap& curr, float alpha)
{
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
        return false;
    }

    if (alpha < 0.f) alpha = 0.f;
    if (alpha > 1.f) alpha = 1.f;
    s_st.alphaLast = alpha;
    if (alpha < s_st.alphaMin) s_st.alphaMin = alpha;
    if (alpha > s_st.alphaMax) s_st.alphaMax = alpha;
    if (TimeDriven()) s_st.timeDriven++;

    const uint32_t scn = CanonAddr(curr.scnMdlEa);
    if (!scn)
    {
        NoteFb(PoseInterpFb_NoPair);
        PublishInactive();
        return false;
    }
    const uint32_t viewBase = CanonAddr(Read32(scn + kPoseScn_ViewPos));
    const uint16_t nView = Read16(scn + kPoseScn_NumView);
    if (!viewBase || nView == 0 || nView > (uint16_t)kPoseMaxNodes)
    {
        NoteFb(PoseInterpFb_NoPair);
        PublishInactive();
        return false;
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
        return false;
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
            return false;
        }
        PoseMtx3x4 vp{};
        Pose_MtxConcat(out.vHat, w, vp);
        if (!Pose_MtxFinite(vp))
        {
            NoteFb(PoseInterpFb_Validate);
            PublishInactive();
            return false;
        }
        out.viewPos[mid] = vp;
        out.valid[mid] = 1;
        nInterp++;

        if (ValidateOn())
        {
            float r = 0;
            Pose_ValidateNodeRel(out.vHat, w, vp, kPoseRelTol, &r);
            if (r > relMax) relMax = r;
        }
    }

    if (nInterp == 0)
    {
        NoteFb(PoseInterpFb_Decompose);
        PublishInactive();
        return false;
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
            return false;
        }
    }

    out.nodesInterp = nInterp;
    out.active = 1;

    {
        std::lock_guard<std::mutex> lk(s_mu);
        s_published = out;
    }

    s_lastFb = PoseInterpFb_None;
    s_st.applied++;
    s_st.nodesInterp += nInterp;
    s_st.viewRebuilt += nInterp;

    if (TraceOn() && (s_logN < 8 || (s_logN % 120) == 0))
    {
        LOG_INFO("PoseInterp apply α={:.3f} nodes={} seq={}/{} scn={:08x} viewEa={:08x} "
                 "relMax={:.5f} applied={} gx={} miss={} time={}",
                 alpha, nInterp, prev.seq, curr.seq, scn, viewActive, relMax, s_st.applied,
                 s_st.gxOverrides, s_st.gxMiss, TimeDriven() ? 1 : 0);
    }
    s_logN++;
    return true;
}

}  // namespace

bool PoseInterp_Enabled() { return On(); }

bool PoseInterp_TimeDriven() { return TimeDriven(); }

bool PoseInterp_Active()
{
    if (!On()) return false;
    std::lock_guard<std::mutex> lk(s_mu);
    return s_published.active != 0;
}

bool PoseInterp_BuildAtAlpha(float alpha)
{
    if (!On()) return false;
    if (!PoseCapture_Enabled()) return false;
    s_st.attempts++;
    PoseSnap prev{}, curr{};
    if (!PoseCapture_GetLatest(curr) || !PoseCapture_GetPrevious(prev))
    {
        NoteFb(PoseInterpFb_NoPair);
        PublishInactive();
        return false;
    }
    return BuildFromPair(prev, curr, alpha);
}

bool PoseInterp_BuildForHostTime(uint64_t hostTicks, float* outAlpha)
{
    if (!On()) return false;
    if (!PoseCapture_Enabled()) return false;
    s_st.attempts++;
    PoseSnap prev{}, curr{};
    float alpha = 0.f;
    if (!PoseCapture_GetBracket(hostTicks, prev, curr, &alpha))
    {
        NoteFb(PoseInterpFb_Unbracketed);
        PublishInactive();
        return false;
    }
    if (outAlpha) *outAlpha = alpha;
    return BuildFromPair(prev, curr, alpha);
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

    // Live XFB path: time-driven uses mid-bracket of the just-published pair as a
    // stable delayed sample (presentation scheduler rebuilds for exact targets).
    float alpha = s_fixedAlpha;
    if (TimeDriven())
    {
        if (curr.hostTicks > prev.hostTicks)
            alpha = 0.5f;  // endpoint XFB uses mid until scheduler rebuilds
        else
            alpha = s_fixedAlpha;
    }
    BuildFromPair(prev, curr, alpha);
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
    std::snprintf(buf, bufSize, "PoseI α=%.2f%s apply=%llu fb=%llu gx=%llu", st.alphaLast,
                  TimeDriven() ? "T" : "", (unsigned long long)st.applied,
                  (unsigned long long)st.fallback, (unsigned long long)st.gxOverrides);
}

PoseInterpFallback PoseInterp_LastFallback() { return s_lastFb; }

const char* PoseInterp_FallbackName(PoseInterpFallback fb)
{
    switch (fb)
    {
    case PoseInterpFb_None: return "none";
    case PoseInterpFb_Disabled: return "disabled";
    case PoseInterpFb_NoPair: return "no_pair";
    case PoseInterpFb_PairReject: return "pair_reject";
    case PoseInterpFb_Decompose: return "decompose";
    case PoseInterpFb_Validate: return "validate";
    case PoseInterpFb_Unbracketed: return "unbracketed";
    default: return "unknown";
    }
}

uint64_t PoseInterp_GxOverrideCount()
{
    return s_st.gxOverrides;
}

bool PoseInterp_GetPublished(uint32_t* viewBaseEa, uint32_t* viewActiveEa, uint16_t* nodesInterp,
                             uint16_t* nView, uint64_t* seqPrev, uint64_t* seqCurr, float* alpha)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (!s_published.active) return false;
    if (viewBaseEa) *viewBaseEa = s_published.viewBaseEa;
    if (viewActiveEa) *viewActiveEa = s_published.viewActiveEa;
    if (nodesInterp) *nodesInterp = s_published.nodesInterp;
    if (nView) *nView = s_published.nView;
    if (seqPrev) *seqPrev = s_published.seqPrev;
    if (seqCurr) *seqCurr = s_published.seqCurr;
    if (alpha) *alpha = s_published.alpha;
    return true;
}

void PoseInterp_ClearPublished() { PublishInactive(); }
