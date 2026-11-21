#pragma once
#include <cstdint>

// A3b camera math — pure affine/quaternion helpers (no guest/GX deps).
// Guest MTX is 3×4 row-major: rows [r0 r1 r2 tx], [r3 r4 r5 ty], [r6 r7 r8 tz].
// Column-vector convention: out = R * p + t  (matches GX / host VS).

struct A3bMtx3x4
{
    float m[12];
};

struct A3bVec3
{
    float x, y, z;
};

struct A3bQuat
{
    float x, y, z, w;  // (x,y,z,w), w = scalar
};

struct A3bRigid
{
    A3bVec3 t{};
    A3bQuat q{};
    float scale = 1.f;     // uniform scale extracted from R; 1 if rigid
    bool hasScale = false;
    bool hasShear = false;
    bool hasReflection = false;
    bool valid = false;
};

enum A3bInterpFail : uint32_t
{
    A3bFail_None = 0,
    A3bFail_NonFinite = 1u << 0,
    A3bFail_Singular = 1u << 1,
    A3bFail_Shear = 1u << 2,
    A3bFail_Reflection = 1u << 3,
    A3bFail_Scale = 1u << 4,   // non-uniform or unexpected scale
    A3bFail_Cut = 1u << 5,
    A3bFail_Missing = 1u << 6,
    A3bFail_ProjChange = 1u << 7,
    A3bFail_CamChange = 1u << 8,
};

void A3b_MtxIdentity(A3bMtx3x4& out);
bool A3b_MtxFinite(const A3bMtx3x4& m);
bool A3b_MtxNearEqual(const A3bMtx3x4& a, const A3bMtx3x4& b, float eps = 1e-4f);

// 4×4-style multiply of affines: out(p) = a(b(p)).
void A3b_MtxMul(const A3bMtx3x4& a, const A3bMtx3x4& b, A3bMtx3x4& out);
bool A3b_MtxInvertRigid(const A3bMtx3x4& in, A3bMtx3x4& out);  // assumes near-orthogonal R

A3bVec3 A3b_MtxTransformPoint(const A3bMtx3x4& m, const A3bVec3& p);
A3bVec3 A3b_MtxTransformDir(const A3bMtx3x4& m, const A3bVec3& d);

bool A3b_Decompose(const A3bMtx3x4& viewOrCam, A3bRigid& out, uint32_t* failBits = nullptr);
void A3b_Compose(const A3bRigid& r, A3bMtx3x4& out);

A3bQuat A3b_QuatIdentity();
A3bQuat A3b_QuatNormalize(const A3bQuat& q);
float A3b_QuatDot(const A3bQuat& a, const A3bQuat& b);
A3bQuat A3b_QuatSlerp(const A3bQuat& a, const A3bQuat& b, float alpha);
A3bVec3 A3b_VecLerp(const A3bVec3& a, const A3bVec3& b, float alpha);

// Interpolate camera-to-world rigid poses; returns world-to-view = inv(camWorld).
// alpha 0 = a, 1 = b. Fails closed on non-rigid / cut flags.
bool A3b_InterpolateView(const A3bMtx3x4& view0, const A3bMtx3x4& view1, float alpha,
                         A3bMtx3x4& outView, uint32_t* failBits = nullptr);

// Camera delta D = viewNew * inv(viewOld) for left-multiply on model-view matrices:
//   M' = D * M   (if M = View * Model).
bool A3b_CameraDelta(const A3bMtx3x4& viewOld, const A3bMtx3x4& viewNew, A3bMtx3x4& outDelta);

// Apply D * M → out.
void A3b_ApplyCameraDelta(const A3bMtx3x4& delta, const A3bMtx3x4& modelView, A3bMtx3x4& out);

uint32_t A3b_HashMtx(const A3bMtx3x4& m);
uint32_t A3b_HashProj7(const float proj[7]);
