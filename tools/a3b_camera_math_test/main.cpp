// Deterministic A3b camera math unit tests (no game / GPU).
#include "../../runtime/debug/a3b_camera_math.h"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            g_fail++;                                                                                                  \
        }                                                                                                              \
    } while (0)

static A3bMtx3x4 Translate(float x, float y, float z)
{
    A3bMtx3x4 m{};
    A3b_MtxIdentity(m);
    m.m[3] = x;
    m.m[7] = y;
    m.m[11] = z;
    return m;
}

static A3bMtx3x4 RotY(float rad)
{
    A3bMtx3x4 m{};
    A3b_MtxIdentity(m);
    const float c = std::cos(rad), s = std::sin(rad);
    m.m[0] = c;
    m.m[2] = s;
    m.m[8] = -s;
    m.m[10] = c;
    return m;
}

int main()
{
    // Identical poses → any alpha equals input.
    {
        A3bMtx3x4 v = Translate(1, 2, 3);
        A3bMtx3x4 out{};
        uint32_t fail = 0;
        CHECK(A3b_InterpolateView(v, v, 0.5f, out, &fail));
        CHECK(fail == 0);
        CHECK(A3b_MtxNearEqual(out, v, 1e-4f));
    }

    // Pure translation lerp.
    {
        A3bMtx3x4 a = Translate(0, 0, 0);
        A3bMtx3x4 b = Translate(10, 0, 0);
        A3bMtx3x4 mid{};
        CHECK(A3b_InterpolateView(a, b, 0.5f, mid, nullptr));
        // view T 0→10 with R=I → camWorld T 0→-10 → mid cam T=-5 → mid view T=+5
        CHECK(std::fabs(mid.m[3] - 5.f) < 1e-3f);
        A3bMtx3x4 end0{}, end1{};
        CHECK(A3b_InterpolateView(a, b, 0.f, end0, nullptr));
        CHECK(A3b_InterpolateView(a, b, 1.f, end1, nullptr));
        CHECK(A3b_MtxNearEqual(end0, a, 1e-3f));
        CHECK(A3b_MtxNearEqual(end1, b, 1e-3f));
    }

    // Pure rotation (Y 90°) endpoints + midpoint not identity.
    {
        A3bMtx3x4 a{};
        A3b_MtxIdentity(a);
        A3bMtx3x4 b = RotY(3.14159265f * 0.5f);
        A3bMtx3x4 mid{};
        CHECK(A3b_InterpolateView(a, b, 0.5f, mid, nullptr));
        CHECK(!A3b_MtxNearEqual(mid, a, 1e-2f));
        CHECK(!A3b_MtxNearEqual(mid, b, 1e-2f));
        A3bMtx3x4 e0{}, e1{};
        CHECK(A3b_InterpolateView(a, b, 0.f, e0, nullptr));
        CHECK(A3b_InterpolateView(a, b, 1.f, e1, nullptr));
        CHECK(A3b_MtxNearEqual(e0, a, 1e-3f));
        CHECK(A3b_MtxNearEqual(e1, b, 1e-3f));
    }

    // Combined translate + rotate.
    {
        A3bMtx3x4 a = Translate(0, 1, 0);
        A3bMtx3x4 r = RotY(0.4f);
        A3bMtx3x4 b{};
        A3b_MtxMul(r, Translate(4, 1, 2), b);
        A3bMtx3x4 mid{};
        CHECK(A3b_InterpolateView(a, b, 0.5f, mid, nullptr));
        CHECK(A3b_MtxFinite(mid));
    }

    // Quaternion sign equivalence: q and -q same rotation.
    {
        A3bRigid ra{}, rb{};
        A3bMtx3x4 m = RotY(0.3f);
        CHECK(A3b_Decompose(m, ra, nullptr));
        rb = ra;
        rb.q = { -ra.q.x, -ra.q.y, -ra.q.z, -ra.q.w };
        A3bQuat s = A3b_QuatSlerp(ra.q, rb.q, 0.5f);
        // Should stay near ra.q (shortest path flips -q).
        CHECK(std::fabs(A3b_QuatDot(A3b_QuatNormalize(s), A3b_QuatNormalize(ra.q))) > 0.99f);
    }

    // Small angular change.
    {
        A3bMtx3x4 a{};
        A3b_MtxIdentity(a);
        A3bMtx3x4 b = RotY(0.01f);
        A3bMtx3x4 mid{};
        CHECK(A3b_InterpolateView(a, b, 0.5f, mid, nullptr));
        CHECK(A3b_MtxFinite(mid));
    }

    // Large angular change (near 180°).
    {
        A3bMtx3x4 a{};
        A3b_MtxIdentity(a);
        A3bMtx3x4 b = RotY(3.0f);
        A3bMtx3x4 mid{};
        CHECK(A3b_InterpolateView(a, b, 0.5f, mid, nullptr));
        CHECK(A3b_MtxFinite(mid));
    }

    // Invalid / singular.
    {
        A3bMtx3x4 bad{};
        A3b_MtxIdentity(bad);
        bad.m[0] = 0;
        bad.m[5] = 0;
        bad.m[10] = 0;
        A3bRigid r{};
        uint32_t fail = 0;
        CHECK(!A3b_Decompose(bad, r, &fail));
        CHECK(fail & A3bFail_Singular);
        A3bMtx3x4 out{};
        A3bMtx3x4 id{};
        A3b_MtxIdentity(id);
        CHECK(!A3b_InterpolateView(bad, id, 0.5f, out, &fail));
    }

    // Non-finite.
    {
        A3bMtx3x4 bad{};
        A3b_MtxIdentity(bad);
        bad.m[3] = 0.f / 0.f;
        A3bRigid r{};
        uint32_t fail = 0;
        CHECK(!A3b_Decompose(bad, r, &fail));
        CHECK(fail & A3bFail_NonFinite);
    }

    // Camera delta identity when views equal.
    {
        A3bMtx3x4 v = Translate(2, 3, 4);
        A3bMtx3x4 d{};
        CHECK(A3b_CameraDelta(v, v, d));
        A3bMtx3x4 id{};
        A3b_MtxIdentity(id);
        CHECK(A3b_MtxNearEqual(d, id, 1e-4f));
    }

    // Reflection rejected.
    {
        A3bMtx3x4 m{};
        A3b_MtxIdentity(m);
        m.m[0] = -1.f;  // det < 0
        A3bRigid r{};
        uint32_t fail = 0;
        CHECK(!A3b_Decompose(m, r, &fail));
        CHECK(fail & A3bFail_Reflection);
    }

    // Shear rejected.
    {
        A3bMtx3x4 m{};
        A3b_MtxIdentity(m);
        m.m[1] = 0.5f;
        A3bRigid r{};
        uint32_t fail = 0;
        CHECK(!A3b_Decompose(m, r, &fail));
        CHECK(fail & A3bFail_Shear);
    }

    // Apply delta: M' = D * M with D = V1 * inv(V0).
    {
        A3bMtx3x4 v0 = Translate(0, 0, 0);
        A3bMtx3x4 v1 = Translate(5, 0, 0);
        A3bMtx3x4 model = Translate(1, 2, 3);  // pretend model in view0 space as V0*Model≈Model
        A3bMtx3x4 mv0{};
        A3b_MtxMul(v0, model, mv0);
        A3bMtx3x4 d{}, mv1exp{}, mv1{};
        CHECK(A3b_CameraDelta(v0, v1, d));
        A3b_MtxMul(v1, model, mv1exp);
        A3b_ApplyCameraDelta(d, mv0, mv1);
        CHECK(A3b_MtxNearEqual(mv1, mv1exp, 1e-3f));
    }

    // A3c archive-at-curr: D = Vα * inv(V_curr). α=1 → identity; α=0 → V0*inv(V1).
    {
        A3bMtx3x4 v0 = Translate(0, 0, 0);
        A3bMtx3x4 v1 = Translate(8, 0, 0);
        A3bMtx3x4 vA{}, dId{}, dPrev{};
        CHECK(A3b_InterpolateView(v0, v1, 1.f, vA, nullptr));
        CHECK(A3b_CameraDelta(v1, vA, dId));  // base = curr
        A3bMtx3x4 id{};
        A3b_MtxIdentity(id);
        CHECK(A3b_MtxNearEqual(dId, id, 1e-4f));
        CHECK(A3b_InterpolateView(v0, v1, 0.f, vA, nullptr));
        CHECK(A3b_CameraDelta(v1, vA, dPrev));
        CHECK(!A3b_MtxNearEqual(dPrev, id, 1e-3f));
        A3bMtx3x4 model = Translate(2, 3, 4);
        A3bMtx3x4 mvCurr{}, mvPrevExp{}, mvPrev{};
        A3b_MtxMul(v1, model, mvCurr);
        A3b_MtxMul(v0, model, mvPrevExp);
        A3b_ApplyCameraDelta(dPrev, mvCurr, mvPrev);
        CHECK(A3b_MtxNearEqual(mvPrev, mvPrevExp, 1e-3f));
        // Identity delta preserves matrix exactly; applying twice stays equal.
        A3bMtx3x4 once{}, twice{};
        A3b_ApplyCameraDelta(dId, mvCurr, once);
        A3b_ApplyCameraDelta(dId, once, twice);
        CHECK(A3b_MtxNearEqual(once, mvCurr, 1e-5f));
        CHECK(A3b_MtxNearEqual(twice, mvCurr, 1e-5f));
        // Non-identity applied twice ≠ once.
        A3bMtx3x4 a1{}, a2{};
        A3b_ApplyCameraDelta(dPrev, mvCurr, a1);
        A3b_ApplyCameraDelta(dPrev, a1, a2);
        CHECK(!A3b_MtxNearEqual(a2, a1, 1e-3f));
    }

    // Midpoint with base=curr: Vα between V0 and V1; D = Vα*inv(V1) moves toward V0.
    {
        A3bMtx3x4 v0 = Translate(0, 0, 0);
        A3bMtx3x4 v1 = Translate(10, 0, 0);
        A3bMtx3x4 vA{}, d{}, model = Translate(0, 0, 0), mv1{}, mvA{}, mvAexp{};
        CHECK(A3b_InterpolateView(v0, v1, 0.5f, vA, nullptr));
        CHECK(A3b_CameraDelta(v1, vA, d));
        A3b_MtxMul(v1, model, mv1);
        A3b_MtxMul(vA, model, mvAexp);
        A3b_ApplyCameraDelta(d, mv1, mvA);
        CHECK(A3b_MtxNearEqual(mvA, mvAexp, 1e-3f));
        CHECK(std::fabs(vA.m[3] - 5.f) < 1e-3f);
    }

    if (g_fail)
    {
        fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    printf("a3b_camera_math_test OK\n");
    return 0;
}
