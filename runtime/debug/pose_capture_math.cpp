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
