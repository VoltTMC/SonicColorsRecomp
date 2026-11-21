#include "a3b_camera_math.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{

constexpr float kEps = 1e-6f;
constexpr float kRigidEps = 2e-3f;

float Dot3(float ax, float ay, float az, float bx, float by, float bz)
{
    return ax * bx + ay * by + az * bz;
}

float Len3(float x, float y, float z) { return std::sqrt(Dot3(x, y, z, x, y, z)); }

void Cross3(float ax, float ay, float az, float bx, float by, float bz, float& ox, float& oy,
            float& oz)
{
    ox = ay * bz - az * by;
    oy = az * bx - ax * bz;
    oz = ax * by - ay * bx;
}

}  // namespace

void A3b_MtxIdentity(A3bMtx3x4& out)
{
    std::memset(out.m, 0, sizeof(out.m));
    out.m[0] = out.m[5] = out.m[10] = 1.f;
}

bool A3b_MtxFinite(const A3bMtx3x4& m)
{
    for (int i = 0; i < 12; i++)
        if (!std::isfinite(m.m[i])) return false;
    return true;
}

bool A3b_MtxNearEqual(const A3bMtx3x4& a, const A3bMtx3x4& b, float eps)
{
    for (int i = 0; i < 12; i++)
        if (std::fabs(a.m[i] - b.m[i]) > eps) return false;
    return true;
}

void A3b_MtxMul(const A3bMtx3x4& a, const A3bMtx3x4& b, A3bMtx3x4& out)
{
    // out_R = a_R * b_R; out_t = a_R * b_t + a_t
    A3bMtx3x4 r{};
    for (int row = 0; row < 3; row++)
    {
        const float* ar = a.m + row * 4;
        for (int col = 0; col < 3; col++)
            r.m[row * 4 + col] = ar[0] * b.m[col] + ar[1] * b.m[4 + col] + ar[2] * b.m[8 + col];
        r.m[row * 4 + 3] =
            ar[0] * b.m[3] + ar[1] * b.m[7] + ar[2] * b.m[11] + ar[3];
    }
    out = r;
}

bool A3b_MtxInvertRigid(const A3bMtx3x4& in, A3bMtx3x4& out)
{
    if (!A3b_MtxFinite(in)) return false;
    // R^T for orthogonal; t' = -R^T * t
    const float* m = in.m;
    A3bMtx3x4 r{};
    r.m[0] = m[0];
    r.m[1] = m[4];
    r.m[2] = m[8];
    r.m[4] = m[1];
    r.m[5] = m[5];
    r.m[6] = m[9];
    r.m[8] = m[2];
    r.m[9] = m[6];
    r.m[10] = m[10];
    r.m[3] = -(r.m[0] * m[3] + r.m[1] * m[7] + r.m[2] * m[11]);
    r.m[7] = -(r.m[4] * m[3] + r.m[5] * m[7] + r.m[6] * m[11]);
    r.m[11] = -(r.m[8] * m[3] + r.m[9] * m[7] + r.m[10] * m[11]);
    out = r;
    return A3b_MtxFinite(out);
}

A3bVec3 A3b_MtxTransformPoint(const A3bMtx3x4& m, const A3bVec3& p)
{
    return { m.m[0] * p.x + m.m[1] * p.y + m.m[2] * p.z + m.m[3],
             m.m[4] * p.x + m.m[5] * p.y + m.m[6] * p.z + m.m[7],
             m.m[8] * p.x + m.m[9] * p.y + m.m[10] * p.z + m.m[11] };
}

A3bVec3 A3b_MtxTransformDir(const A3bMtx3x4& m, const A3bVec3& d)
{
    return { m.m[0] * d.x + m.m[1] * d.y + m.m[2] * d.z,
             m.m[4] * d.x + m.m[5] * d.y + m.m[6] * d.z,
             m.m[8] * d.x + m.m[9] * d.y + m.m[10] * d.z };
}

A3bQuat A3b_QuatIdentity() { return { 0, 0, 0, 1 }; }

A3bQuat A3b_QuatNormalize(const A3bQuat& q)
{
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (n < kEps) return A3b_QuatIdentity();
    return { q.x / n, q.y / n, q.z / n, q.w / n };
}

float A3b_QuatDot(const A3bQuat& a, const A3bQuat& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

A3bQuat A3b_QuatSlerp(const A3bQuat& aIn, const A3bQuat& bIn, float alpha)
{
    A3bQuat a = A3b_QuatNormalize(aIn);
    A3bQuat b = A3b_QuatNormalize(bIn);
    float dot = A3b_QuatDot(a, b);
    // Shortest path: flip sign if needed.
    if (dot < 0.f)
    {
        b = { -b.x, -b.y, -b.z, -b.w };
        dot = -dot;
    }
    alpha = std::clamp(alpha, 0.f, 1.f);
    if (dot > 0.9995f)
    {
        // Near-identical — nlerp.
        A3bQuat r{ a.x + alpha * (b.x - a.x), a.y + alpha * (b.y - a.y),
                   a.z + alpha * (b.z - a.z), a.w + alpha * (b.w - a.w) };
        return A3b_QuatNormalize(r);
    }
    const float theta = std::acos(std::clamp(dot, -1.f, 1.f));
    const float s = std::sin(theta);
    const float w0 = std::sin((1.f - alpha) * theta) / s;
    const float w1 = std::sin(alpha * theta) / s;
    return A3b_QuatNormalize({ w0 * a.x + w1 * b.x, w0 * a.y + w1 * b.y, w0 * a.z + w1 * b.z,
                               w0 * a.w + w1 * b.w });
}

A3bVec3 A3b_VecLerp(const A3bVec3& a, const A3bVec3& b, float alpha)
{
    alpha = std::clamp(alpha, 0.f, 1.f);
    return { a.x + alpha * (b.x - a.x), a.y + alpha * (b.y - a.y), a.z + alpha * (b.z - a.z) };
}

bool A3b_Decompose(const A3bMtx3x4& viewOrCam, A3bRigid& out, uint32_t* failBits)
{
    out = A3bRigid{};
    uint32_t fail = 0;
    if (!A3b_MtxFinite(viewOrCam))
    {
        fail |= A3bFail_NonFinite;
        if (failBits) *failBits = fail;
        return false;
    }
    const float* m = viewOrCam.m;
    // Column lengths of R (scale).
    const float sx = Len3(m[0], m[4], m[8]);
    const float sy = Len3(m[1], m[5], m[9]);
    const float sz = Len3(m[2], m[6], m[10]);
    if (sx < kEps || sy < kEps || sz < kEps)
    {
        fail |= A3bFail_Singular;
        if (failBits) *failBits = fail;
        return false;
    }
    const float sAvg = (sx + sy + sz) / 3.f;
    if (std::fabs(sx - sy) > kRigidEps * sAvg || std::fabs(sy - sz) > kRigidEps * sAvg ||
        std::fabs(sx - sz) > kRigidEps * sAvg)
    {
        out.hasScale = true;
        fail |= A3bFail_Scale;
    }
    if (std::fabs(sAvg - 1.f) > 0.05f)
    {
        out.hasScale = true;
        out.scale = sAvg;
        // Uniform scale is OK for camera; flag but continue.
    }
    else
        out.scale = 1.f;

    // Orthonormalize columns.
    float c0x = m[0] / sx, c0y = m[4] / sx, c0z = m[8] / sx;
    float c1x = m[1] / sy, c1y = m[5] / sy, c1z = m[9] / sy;
    float c2x = m[2] / sz, c2y = m[6] / sz, c2z = m[10] / sz;
    // Shear: columns should be orthogonal.
    if (std::fabs(Dot3(c0x, c0y, c0z, c1x, c1y, c1z)) > kRigidEps ||
        std::fabs(Dot3(c0x, c0y, c0z, c2x, c2y, c2z)) > kRigidEps ||
        std::fabs(Dot3(c1x, c1y, c1z, c2x, c2y, c2z)) > kRigidEps)
    {
        out.hasShear = true;
        fail |= A3bFail_Shear;
    }
    // Reflection: det < 0
    float cx, cy, cz;
    Cross3(c0x, c0y, c0z, c1x, c1y, c1z, cx, cy, cz);
    const float det = Dot3(cx, cy, cz, c2x, c2y, c2z);
    if (det < 0.f)
    {
        out.hasReflection = true;
        fail |= A3bFail_Reflection;
        c2x = -c2x;
        c2y = -c2y;
        c2z = -c2z;
    }

    // Re-orthogonalize (Gram-Schmidt) for quat extraction.
    Cross3(c0x, c0y, c0z, c1x, c1y, c1z, c2x, c2y, c2z);
    float n2 = Len3(c2x, c2y, c2z);
    if (n2 < kEps)
    {
        fail |= A3bFail_Singular;
        if (failBits) *failBits = fail;
        return false;
    }
    c2x /= n2;
    c2y /= n2;
    c2z /= n2;
    Cross3(c2x, c2y, c2z, c0x, c0y, c0z, c1x, c1y, c1z);

    // Rotation matrix rows from orthonormal columns.
    const float r00 = c0x, r01 = c1x, r02 = c2x;
    const float r10 = c0y, r11 = c1y, r12 = c2y;
    const float r20 = c0z, r21 = c1z, r22 = c2z;

    // Shepperd quaternion from rotation matrix.
    A3bQuat q{};
    const float tr = r00 + r11 + r22;
    if (tr > 0.f)
    {
        const float s = std::sqrt(tr + 1.f) * 2.f;
        q.w = 0.25f * s;
        q.x = (r21 - r12) / s;
        q.y = (r02 - r20) / s;
        q.z = (r10 - r01) / s;
    }
    else if (r00 > r11 && r00 > r22)
    {
        const float s = std::sqrt(1.f + r00 - r11 - r22) * 2.f;
        q.w = (r21 - r12) / s;
        q.x = 0.25f * s;
        q.y = (r01 + r10) / s;
        q.z = (r02 + r20) / s;
    }
    else if (r11 > r22)
    {
        const float s = std::sqrt(1.f + r11 - r00 - r22) * 2.f;
        q.w = (r02 - r20) / s;
        q.x = (r01 + r10) / s;
        q.y = 0.25f * s;
        q.z = (r12 + r21) / s;
    }
    else
    {
        const float s = std::sqrt(1.f + r22 - r00 - r11) * 2.f;
        q.w = (r10 - r01) / s;
        q.x = (r02 + r20) / s;
        q.y = (r12 + r21) / s;
        q.z = 0.25f * s;
    }
    out.q = A3b_QuatNormalize(q);
    out.t = { m[3], m[7], m[11] };
    // Fail closed on shear/reflection; uniform scale alone is tolerated with flag.
    if (fail & (A3bFail_Shear | A3bFail_Reflection | A3bFail_Singular | A3bFail_NonFinite))
    {
        if (failBits) *failBits = fail;
        return false;
    }
    // Non-uniform scale already set Fail_Scale — reject for camera interp.
    if (fail & A3bFail_Scale)
    {
        if (failBits) *failBits = fail;
        return false;
    }
    out.valid = true;
    if (failBits) *failBits = fail;
    return true;
}

void A3b_Compose(const A3bRigid& r, A3bMtx3x4& out)
{
    const A3bQuat q = A3b_QuatNormalize(r.q);
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    const float s = r.scale;
    out.m[0] = s * (1.f - 2.f * (yy + zz));
    out.m[1] = s * (2.f * (xy - wz));
    out.m[2] = s * (2.f * (xz + wy));
    out.m[3] = r.t.x;
    out.m[4] = s * (2.f * (xy + wz));
    out.m[5] = s * (1.f - 2.f * (xx + zz));
    out.m[6] = s * (2.f * (yz - wx));
    out.m[7] = r.t.y;
    out.m[8] = s * (2.f * (xz - wy));
    out.m[9] = s * (2.f * (yz + wx));
    out.m[10] = s * (1.f - 2.f * (xx + yy));
    out.m[11] = r.t.z;
}

bool A3b_InterpolateView(const A3bMtx3x4& view0, const A3bMtx3x4& view1, float alpha,
                         A3bMtx3x4& outView, uint32_t* failBits)
{
    uint32_t fail = 0;
    if (!A3b_MtxFinite(view0) || !A3b_MtxFinite(view1))
    {
        fail |= A3bFail_NonFinite;
        if (failBits) *failBits = fail;
        return false;
    }
    alpha = std::clamp(alpha, 0.f, 1.f);
    // Exact endpoints / identical poses: no decompose round-trip (preserves captured bits).
    if (alpha <= 1e-6f || A3b_MtxNearEqual(view0, view1, 1e-5f))
    {
        outView = view0;
        if (failBits) *failBits = 0;
        return true;
    }
    if (alpha >= 1.f - 1e-6f)
    {
        outView = view1;
        if (failBits) *failBits = 0;
        return true;
    }
    // view = world-to-view. Camera-to-world = inv(view).
    A3bMtx3x4 cam0{}, cam1{};
    if (!A3b_MtxInvertRigid(view0, cam0) || !A3b_MtxInvertRigid(view1, cam1))
    {
        fail |= A3bFail_Singular;
        if (failBits) *failBits = fail;
        return false;
    }
    A3bRigid r0{}, r1{};
    uint32_t f0 = 0, f1 = 0;
    if (!A3b_Decompose(cam0, r0, &f0) || !A3b_Decompose(cam1, r1, &f1))
    {
        fail |= f0 | f1 | A3bFail_Singular;
        if (failBits) *failBits = fail;
        return false;
    }
    A3bRigid ri{};
    ri.t = A3b_VecLerp(r0.t, r1.t, alpha);
    ri.q = A3b_QuatSlerp(r0.q, r1.q, alpha);
    ri.scale = 1.f;
    ri.valid = true;
    A3bMtx3x4 camI{};
    A3b_Compose(ri, camI);
    if (!A3b_MtxInvertRigid(camI, outView))
    {
        fail |= A3bFail_Singular;
        if (failBits) *failBits = fail;
        return false;
    }
    if (failBits) *failBits = fail;
    return A3b_MtxFinite(outView);
}

bool A3b_CameraDelta(const A3bMtx3x4& viewOld, const A3bMtx3x4& viewNew, A3bMtx3x4& outDelta)
{
    A3bMtx3x4 invOld{};
    if (!A3b_MtxInvertRigid(viewOld, invOld)) return false;
    // D = viewNew * inv(viewOld)
    A3b_MtxMul(viewNew, invOld, outDelta);
    return A3b_MtxFinite(outDelta);
}

void A3b_ApplyCameraDelta(const A3bMtx3x4& delta, const A3bMtx3x4& modelView, A3bMtx3x4& out)
{
    A3b_MtxMul(delta, modelView, out);
}

uint32_t A3b_HashMtx(const A3bMtx3x4& m)
{
    uint32_t h = 2166136261u;
    const uint8_t* b = (const uint8_t*)m.m;
    for (size_t i = 0; i < sizeof(m.m); i++)
    {
        h ^= b[i];
        h *= 16777619u;
    }
    return h;
}

uint32_t A3b_HashProj7(const float proj[7])
{
    uint32_t h = 2166136261u;
    const uint8_t* b = (const uint8_t*)proj;
    for (size_t i = 0; i < 7 * sizeof(float); i++)
    {
        h ^= b[i];
        h *= 16777619u;
    }
    return h;
}
