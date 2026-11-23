// Approach A3m — snapshot pair qualification (SCR_POSE_QUALIFY). No interpolation.
#include "pose_qualify.h"
#include "../log.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_on{ -1 };
std::mutex s_mu;
PoseQualifyStats s_st{};
uint32_t s_logN = 0;

bool EnvFlag(const char* name)
{
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

bool On()
{
    int v = s_on.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_POSE_QUALIFY") ? 1 : 0;
        s_on.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("PoseQualify: enabled (SCR_POSE_QUALIFY); pair + matrix-class counters only");
    }
    return v != 0;
}

void AccClass(uint64_t* bins, PoseMtxClass c)
{
    const unsigned i = (unsigned)c;
    if (i < 8) bins[i]++;
}

}  // namespace

bool PoseQualify_Enabled() { return On(); }

void PoseQualify_OnXfb()
{
    if (!On()) return;
    if (!PoseCapture_Enabled()) return;

    PoseSnap prev{}, curr{};
    if (!PoseCapture_GetLatest(curr)) return;
    if (!PoseCapture_GetPrevious(prev))
    {
        std::lock_guard<std::mutex> lk(s_mu);
        s_st.reject[PosePair_MissingPrev]++;
        return;
    }

    // Matrix class histogram (current snap in-palette World / ViewPos + root).
    {
        std::lock_guard<std::mutex> lk(s_mu);
        if (curr.rootOk)
        {
            s_st.classRoot++;
            const PoseMtxClassInfo ri = Pose_ClassifyMtx(curr.root);
            if (ri.cls == PoseMtx_Rigid || ri.cls == PoseMtx_UniformScale) s_st.classRootRigid++;
        }
        for (uint16_t i = 0; i < curr.nodeCount && i < (uint16_t)kPoseMaxNodes; i++)
        {
            if (curr.nodes[i].worldOk)
                AccClass(s_st.classWorld, Pose_ClassifyMtx(curr.nodes[i].world).cls);
            if (curr.nodes[i].viewOk)
                AccClass(s_st.classView, Pose_ClassifyMtx(curr.nodes[i].viewPos).cls);
        }
    }

    const PosePairVerdict v = Pose_EvaluatePair(prev, curr);

    std::lock_guard<std::mutex> lk(s_mu);
    s_st.pairsExamined++;
    if (v.reject < 16) s_st.reject[v.reject]++;

    if (v.reject == PosePair_Ok)
    {
        s_st.pairsOk++;
        s_st.pairsOkSameGen++;
        s_st.worldTDeltaSum += v.worldTDelta;
        s_st.vHatTDeltaSum += v.vHatTDelta;
        s_st.viewTDeltaSum += v.viewTDelta;
        if (v.worldTDelta > s_st.worldTDeltaMax) s_st.worldTDeltaMax = v.worldTDelta;
        if (v.vHatTDelta > s_st.vHatTDeltaMax) s_st.vHatTDeltaMax = v.vHatTDelta;
        if (v.viewTDelta > s_st.viewTDeltaMax) s_st.viewTDeltaMax = v.viewTDelta;
        if (v.xfbGap) s_st.xfbGapOk++;
        if (v.worldTDelta < 1e-4f) s_st.unchangedPose++;
        // Camera vs pose dominance (translation magnitudes).
        if (v.vHatTDelta > v.worldTDelta * 2.f && v.vHatTDelta > 0.05f) s_st.camDominated++;
        else if (v.worldTDelta > v.vHatTDelta * 2.f && v.worldTDelta > 0.05f) s_st.poseDominated++;
    }

    if (s_logN < 6 || (s_logN % 180) == 0)
    {
        LOG_INFO("PoseQualify pair reject={} worldΔT={:.3f} vHatΔT={:.3f} viewΔT={:.3f} "
                 "ok={}/{} xfbGapOk={} camDom={} poseDom={}",
                 (unsigned)v.reject, v.worldTDelta, v.vHatTDelta, v.viewTDelta, s_st.pairsOk,
                 s_st.pairsExamined, s_st.xfbGapOk, s_st.camDominated, s_st.poseDominated);
        LOG_INFO("PoseQualify classW rigid={} unif={} nonu={} shear={} sing={} inv={} refl={} "
                 "rootRigid={}/{}",
                 s_st.classWorld[PoseMtx_Rigid], s_st.classWorld[PoseMtx_UniformScale],
                 s_st.classWorld[PoseMtx_NonUniform], s_st.classWorld[PoseMtx_Shear],
                 s_st.classWorld[PoseMtx_Singular], s_st.classWorld[PoseMtx_Invalid],
                 s_st.classWorld[PoseMtx_Reflection], s_st.classRootRigid, s_st.classRoot);
        LOG_INFO("PoseQualify rejects missPrev={} incomplete={} own={} scn={} gen={} nodes={} "
                 "parent={} seq={} teleport={} camCut={} badCls={}",
                 s_st.reject[PosePair_MissingPrev], s_st.reject[PosePair_Incomplete],
                 s_st.reject[PosePair_OwnMismatch], s_st.reject[PosePair_ScnMismatch],
                 s_st.reject[PosePair_ModelGen], s_st.reject[PosePair_NodeCount],
                 s_st.reject[PosePair_ParentTable], s_st.reject[PosePair_SeqGap],
                 s_st.reject[PosePair_Teleport], s_st.reject[PosePair_CamCut],
                 s_st.reject[PosePair_BadClass]);
    }
    s_logN++;
}

void PoseQualify_GetStats(PoseQualifyStats& out)
{
    std::lock_guard<std::mutex> lk(s_mu);
    out = s_st;
}

void PoseQualify_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!On()) return;
    PoseQualifyStats st{};
    PoseQualify_GetStats(st);
    std::snprintf(buf, bufSize, "PoseQ ok=%llu/%llu wΔ=%.2f vΔ=%.2f",
                  (unsigned long long)st.pairsOk, (unsigned long long)st.pairsExamined,
                  st.worldTDeltaMax, st.vHatTDeltaMax);
}
