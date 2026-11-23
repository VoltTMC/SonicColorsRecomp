// Host-testable pose math + synthetic ring (no guest / GPU).
#include "pose_capture.h"

#include <cmath>
#include <cstring>

bool Pose_MtxFinite(const PoseMtx3x4& m)
{
    for (int i = 0; i < 12; i++)
        if (!std::isfinite(m.m[i])) return false;
    return true;
}

void Pose_MtxConcat(const PoseMtx3x4& a, const PoseMtx3x4& b, PoseMtx3x4& c)
{
    for (int r = 0; r < 3; r++)
    {
        const float* ar = a.m + r * 4;
        for (int col = 0; col < 3; col++)
            c.m[r * 4 + col] = ar[0] * b.m[col] + ar[1] * b.m[4 + col] + ar[2] * b.m[8 + col];
        c.m[r * 4 + 3] = ar[0] * b.m[3] + ar[1] * b.m[7] + ar[2] * b.m[11] + ar[3];
    }
}

bool Pose_MtxInvert(const PoseMtx3x4& m, PoseMtx3x4& out)
{
    const float a00 = m.m[0], a01 = m.m[1], a02 = m.m[2];
    const float a10 = m.m[4], a11 = m.m[5], a12 = m.m[6];
    const float a20 = m.m[8], a21 = m.m[9], a22 = m.m[10];
    const float det =
        a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);
    if (!std::isfinite(det) || std::fabs(det) < 1e-8f) return false;
    const float inv = 1.f / det;
    out.m[0] = (a11 * a22 - a12 * a21) * inv;
    out.m[1] = (a02 * a21 - a01 * a22) * inv;
    out.m[2] = (a01 * a12 - a02 * a11) * inv;
    out.m[4] = (a12 * a20 - a10 * a22) * inv;
    out.m[5] = (a00 * a22 - a02 * a20) * inv;
    out.m[6] = (a02 * a10 - a00 * a12) * inv;
    out.m[8] = (a10 * a21 - a11 * a20) * inv;
    out.m[9] = (a01 * a20 - a00 * a21) * inv;
    out.m[10] = (a00 * a11 - a01 * a10) * inv;
    const float tx = m.m[3], ty = m.m[7], tz = m.m[11];
    out.m[3] = -(out.m[0] * tx + out.m[1] * ty + out.m[2] * tz);
    out.m[7] = -(out.m[4] * tx + out.m[5] * ty + out.m[6] * tz);
    out.m[11] = -(out.m[8] * tx + out.m[9] * ty + out.m[10] * tz);
    return Pose_MtxFinite(out);
}

float Pose_MtxMaxAbsDiff(const PoseMtx3x4& a, const PoseMtx3x4& b)
{
    float mx = 0;
    for (int i = 0; i < 12; i++)
    {
        const float d = std::fabs(a.m[i] - b.m[i]);
        if (d > mx) mx = d;
    }
    return mx;
}

bool Pose_DeriveVHat(const PoseMtx3x4& view0, const PoseMtx3x4& world0, PoseMtx3x4& vHat)
{
    PoseMtx3x4 invW{};
    if (!Pose_MtxInvert(world0, invW)) return false;
    Pose_MtxConcat(view0, invW, vHat);
    return Pose_MtxFinite(vHat);
}

bool Pose_ValidateNodeRel(const PoseMtx3x4& vHat, const PoseMtx3x4& world, const PoseMtx3x4& view,
                          float tol, float* outRes)
{
    PoseMtx3x4 pred{};
    Pose_MtxConcat(vHat, world, pred);
    const float r = Pose_MtxMaxAbsDiff(view, pred);
    if (outRes) *outRes = r;
    return r <= tol;
}

void PoseRing_Init(PoseRingTest& r)
{
    r = {};
}

bool PoseRing_Publish(PoseRingTest& r, const PoseSnap& snap)
{
    if (!(snap.flags & PoseFlag_Complete)) return false;
    if (r.count == (uint32_t)kPoseRingSlots) r.overwrites++;
    r.slots[r.write] = snap;
    r.slots[r.write].seq = ++r.published;
    r.write = (r.write + 1) % kPoseRingSlots;
    if (r.count < (uint32_t)kPoseRingSlots) r.count++;
    return true;
}

bool PoseRing_Latest(const PoseRingTest& r, PoseSnap& out)
{
    if (!r.count) return false;
    const uint32_t idx = (r.write + kPoseRingSlots - 1) % kPoseRingSlots;
    out = r.slots[idx];
    return (out.flags & PoseFlag_Complete) != 0;
}

bool PoseRing_Previous(const PoseRingTest& r, PoseSnap& out)
{
    if (r.count < 2) return false;
    const uint32_t idx = (r.write + kPoseRingSlots - 2) % kPoseRingSlots;
    out = r.slots[idx];
    return (out.flags & PoseFlag_Complete) != 0;
}

PoseMtxClassInfo Pose_ClassifyMtx(const PoseMtx3x4& m)
{
    PoseMtxClassInfo info{};
    if (!Pose_MtxFinite(m))
    {
        info.cls = PoseMtx_Invalid;
        return info;
    }
    // Columns of the 3×3 linear part (column-vector convention).
    const float c0[3] = { m.m[0], m.m[4], m.m[8] };
    const float c1[3] = { m.m[1], m.m[5], m.m[9] };
    const float c2[3] = { m.m[2], m.m[6], m.m[10] };
    auto len = [](const float* v) {
        return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    };
    auto dot = [](const float* a, const float* b) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    };
    info.scale[0] = len(c0);
    info.scale[1] = len(c1);
    info.scale[2] = len(c2);
    info.det = c0[0] * (c1[1] * c2[2] - c1[2] * c2[1]) - c0[1] * (c1[0] * c2[2] - c1[2] * c2[0]) +
               c0[2] * (c1[0] * c2[1] - c1[1] * c2[0]);
    if (!std::isfinite(info.det) || std::fabs(info.det) < 1e-8f)
    {
        info.cls = PoseMtx_Singular;
        return info;
    }
    info.reflected = (info.det < 0.f) ? 1 : 0;

    float n0[3], n1[3], n2[3];
    for (int i = 0; i < 3; i++)
    {
        n0[i] = (info.scale[0] > 1e-8f) ? c0[i] / info.scale[0] : 0.f;
        n1[i] = (info.scale[1] > 1e-8f) ? c1[i] / info.scale[1] : 0.f;
        n2[i] = (info.scale[2] > 1e-8f) ? c2[i] / info.scale[2] : 0.f;
    }
    const float o01 = std::fabs(dot(n0, n1));
    const float o02 = std::fabs(dot(n0, n2));
    const float o12 = std::fabs(dot(n1, n2));
    info.orthoErr = o01;
    if (o02 > info.orthoErr) info.orthoErr = o02;
    if (o12 > info.orthoErr) info.orthoErr = o12;

    constexpr float kOrthoTol = 1e-2f;
    constexpr float kScaleUniformTol = 1e-2f;
    constexpr float kRigidScaleTol = 1e-2f;
    if (info.orthoErr > kOrthoTol)
    {
        info.cls = PoseMtx_Shear;
        return info;
    }
    if (info.reflected)
    {
        info.cls = PoseMtx_Reflection;
        return info;
    }
    const float sMax = std::fmax(info.scale[0], std::fmax(info.scale[1], info.scale[2]));
    const float sMin = std::fmin(info.scale[0], std::fmin(info.scale[1], info.scale[2]));
    if (sMax - sMin > kScaleUniformTol * std::fmax(1.f, sMax))
    {
        info.cls = PoseMtx_NonUniform;
        return info;
    }
    const float sAvg = (info.scale[0] + info.scale[1] + info.scale[2]) / 3.f;
    if (std::fabs(sAvg - 1.f) > kRigidScaleTol)
    {
        info.cls = PoseMtx_UniformScale;
        return info;
    }
    info.cls = PoseMtx_Rigid;
    return info;
}

static float TransDelta(const PoseMtx3x4& a, const PoseMtx3x4& b)
{
    const float dx = a.m[3] - b.m[3];
    const float dy = a.m[7] - b.m[7];
    const float dz = a.m[11] - b.m[11];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static int FirstInPalette(const PoseSnap& s)
{
    for (uint16_t i = 0; i < s.nodeCount && i < kPoseMaxNodes; i++)
        if (s.nodes[i].worldOk && s.nodes[i].viewOk) return (int)i;
    return -1;
}

PosePairVerdict Pose_EvaluatePair(const PoseSnap& prev, const PoseSnap& curr)
{
    PosePairVerdict v{};
    if (!(prev.flags & PoseFlag_Complete) || !(curr.flags & PoseFlag_Complete))
    {
        v.reject = PosePair_Incomplete;
        return v;
    }
    if (curr.flags & PoseFlag_Warmup)
    {
        v.reject = PosePair_Warmup;
        return v;
    }
    if (prev.shellEa != curr.shellEa || prev.bodyEa != curr.bodyEa ||
        prev.instanceKey != curr.instanceKey)
    {
        v.reject = PosePair_OwnMismatch;
        return v;
    }
    if (prev.scnMdlEa != curr.scnMdlEa || prev.resMdlEa != curr.resMdlEa)
    {
        v.reject = PosePair_ScnMismatch;
        return v;
    }
    if (prev.modelGen != curr.modelGen || (curr.flags & PoseFlag_ModelChanged))
    {
        v.reject = PosePair_ModelGen;
        return v;
    }
    if (prev.nodeCount != curr.nodeCount || prev.inPaletteCount != curr.inPaletteCount)
    {
        v.reject = PosePair_NodeCount;
        return v;
    }
    for (uint16_t i = 0; i < curr.nodeCount && i < kPoseMaxNodes; i++)
    {
        if (prev.nodes[i].parentIdx != curr.nodes[i].parentIdx ||
            prev.nodes[i].mtxID != curr.nodes[i].mtxID ||
            prev.nodes[i].nodeIdx != curr.nodes[i].nodeIdx)
            v.parentMismatches++;
    }
    if (v.parentMismatches)
    {
        v.reject = PosePair_ParentTable;
        return v;
    }
    if (curr.seq != prev.seq + 1)
    {
        v.seqGap = 1;
        v.reject = PosePair_SeqGap;
        return v;
    }
    if (curr.xfbIndex > prev.xfbIndex + 1)
    {
        v.xfbGap = 1;
        // Soft: still pairable for pose if seq consecutive; mark gap for timing.
        // Reject only if xfb jumped while seq somehow consecutive (shouldn't happen).
    }
    if (curr.simFrameId > prev.simFrameId + 2)
    {
        // Guest sim may advance >1 between XFBs rarely; treat large jumps as gap.
        v.seqGap = 1;
        v.reject = PosePair_SeqGap;
        return v;
    }

    const int ri = FirstInPalette(curr);
    if (ri < 0)
    {
        v.reject = PosePair_Incomplete;
        return v;
    }
    v.worldTDelta = TransDelta(prev.nodes[ri].world, curr.nodes[ri].world);
    v.viewTDelta = TransDelta(prev.nodes[ri].viewPos, curr.nodes[ri].viewPos);
    if (prev.vHatOk && curr.vHatOk) v.vHatTDelta = TransDelta(prev.vHat, curr.vHat);

    if (v.worldTDelta > kPoseTeleportWorldT)
    {
        v.reject = PosePair_Teleport;
        return v;
    }
    if (v.vHatTDelta > kPoseCamCutVHatT && v.worldTDelta < 1.f)
    {
        // Large view change with nearly static world → camera cut / discontinuous V̂.
        v.reject = PosePair_CamCut;
        return v;
    }

    // Unsupported class on reference or any in-palette node blocks first-impl TRS.
    for (uint16_t i = 0; i < curr.nodeCount && i < kPoseMaxNodes; i++)
    {
        if (!curr.nodes[i].worldOk) continue;
        const PoseMtxClassInfo ci = Pose_ClassifyMtx(curr.nodes[i].world);
        if (ci.cls == PoseMtx_Invalid || ci.cls == PoseMtx_Singular || ci.cls == PoseMtx_Shear ||
            ci.cls == PoseMtx_Reflection)
        {
            v.reject = PosePair_BadClass;
            return v;
        }
    }

    if (curr.xfbIndex > prev.xfbIndex + 1) v.xfbGap = 1;
    v.reject = PosePair_Ok;
    return v;
}
