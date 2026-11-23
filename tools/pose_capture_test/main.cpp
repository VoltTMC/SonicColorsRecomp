// Host-side tests for A3l pose math + ring (no guest / GPU).
#include "debug/pose_capture.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

int g_fail = 0;

void Expect(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        g_fail++;
    }
}

PoseMtx3x4 Ident()
{
    PoseMtx3x4 m{};
    m.m[0] = 1;
    m.m[5] = 1;
    m.m[10] = 1;
    return m;
}

PoseMtx3x4 Translate(float x, float y, float z)
{
    PoseMtx3x4 m = Ident();
    m.m[3] = x;
    m.m[7] = y;
    m.m[11] = z;
    return m;
}

PoseSnap MakeComplete(uint16_t nodes, uint64_t seqHint, uint32_t scn, uint32_t modelGen)
{
    PoseSnap s{};
    s.formatVersion = kPoseFmtVersion;
    s.flags = PoseFlag_Complete | PoseFlag_OwnOk | PoseFlag_HierOk | PoseFlag_WorldOk |
              PoseFlag_ViewOk | PoseFlag_VHatOk | PoseFlag_RelOk;
    s.nodeCount = nodes;
    s.inPaletteCount = nodes;
    s.nodeCapacity = kPoseMaxNodes;
    s.scnMdlEa = scn;
    s.resMdlEa = scn + 0x100;
    s.modelGen = modelGen;
    s.shellEa = 0x80b58b00;
    s.bodyEa = 0x90f9c6c0;
    s.instanceKey = s.shellEa ^ s.bodyEa;
    s.seq = seqHint;
    s.xfbIndex = seqHint;
    s.simFrameId = seqHint;
    s.vHat = Ident();
    s.vHatOk = 1;
    s.relOk = 1;
    s.relPassCount = nodes;
    s.root = Ident();
    s.rootOk = 1;
    for (uint16_t i = 0; i < nodes; i++)
    {
        s.nodes[i].nodeIdx = i;
        s.nodes[i].parentIdx = (i == 0) ? (int16_t)-1 : (int16_t)(i - 1);
        s.nodes[i].mtxID = i;
        s.nodes[i].world = Translate((float)i, 0, 0);
        s.nodes[i].viewPos = Translate((float)i, 0, 0);  // V̂=I → view=world
        s.nodes[i].worldOk = 1;
        s.nodes[i].viewOk = 1;
    }
    return s;
}

}  // namespace

int main()
{
    // --- Finite / invert / concat / V̂ ---
    {
        PoseMtx3x4 id = Ident();
        Expect(Pose_MtxFinite(id), "ident finite");
        PoseMtx3x4 inv{};
        Expect(Pose_MtxInvert(id, inv), "ident invertible");
        Expect(Pose_MtxMaxAbsDiff(id, inv) < 1e-6f, "ident inv ≈ I");

        PoseMtx3x4 bad = id;
        bad.m[0] = NAN;
        Expect(!Pose_MtxFinite(bad), "nan not finite");

        PoseMtx3x4 singular{};
        Expect(!Pose_MtxInvert(singular, inv), "zero singular");

        PoseMtx3x4 T = Translate(1, 2, 3);
        PoseMtx3x4 V = Translate(10, 0, 0);
        PoseMtx3x4 W = Translate(1, 0, 0);
        // View = V̂ · World with V̂ = Translate(9,0,0): ViewPos = (10,0,0) when W=(1,0,0)
        PoseMtx3x4 view = Translate(10, 0, 0);
        PoseMtx3x4 vHat{};
        Expect(Pose_DeriveVHat(view, W, vHat), "derive V̂");
        float res = 0;
        Expect(Pose_ValidateNodeRel(vHat, W, view, kPoseRelTol, &res), "rel node0");
        Expect(res < 1e-5f, "rel residual tiny");
        PoseMtx3x4 W2 = Translate(2, 0, 0);
        PoseMtx3x4 V2 = Translate(11, 0, 0);  // 9+2
        Expect(Pose_ValidateNodeRel(vHat, W2, V2, kPoseRelTol, &res), "rel node1");
        (void)T;
        (void)V;
    }

    // --- Ring init / publish / wraparound ---
    {
        PoseRingTest r{};
        PoseRing_Init(r);
        PoseSnap out{};
        Expect(!PoseRing_Latest(r, out), "empty latest");
        Expect(!PoseRing_Previous(r, out), "empty previous");

        PoseSnap incomplete = MakeComplete(4, 0, 0x809882e0, 1);
        incomplete.flags &= ~PoseFlag_Complete;
        Expect(!PoseRing_Publish(r, incomplete), "reject incomplete");
        Expect(r.count == 0, "count still 0");

        for (int i = 0; i < 5; i++)
        {
            PoseSnap s = MakeComplete(8, 0, 0x809882e0, 1);
            s.xfbIndex = (uint64_t)i;
            Expect(PoseRing_Publish(r, s), "publish complete");
        }
        Expect(r.count == (uint32_t)kPoseRingSlots, "ring full");
        Expect(r.overwrites == 2, "two overwrites after wrap");
        Expect(PoseRing_Latest(r, out), "latest ok");
        Expect(out.xfbIndex == 4, "latest is last");
        Expect(PoseRing_Previous(r, out), "previous ok");
        Expect(out.xfbIndex == 3, "previous is penultimate");
    }

    // --- Parent / overflow / model change correspondence ---
    {
        PoseSnap a = MakeComplete(8, 1, 0x809882e0, 1);
        PoseSnap b = MakeComplete(8, 2, 0x809882e0, 1);
        Expect(a.scnMdlEa == b.scnMdlEa && a.modelGen == b.modelGen, "same model gen");
        Expect(a.nodes[3].parentIdx == 2, "parent chain");
        Expect(a.nodes[0].parentIdx == -1, "root parent");

        PoseSnap c = MakeComplete(8, 3, 0x809882e0, 2);
        Expect(c.modelGen != a.modelGen, "model gen bump");

        PoseSnap over = MakeComplete(8, 4, 0x809882e0, 1);
        over.nodeCount = (uint16_t)(kPoseMaxNodes + 1);
        Expect(over.nodeCount > kPoseMaxNodes, "overflow detectable");
    }

    // --- Sequence gap detection (consumer-side) ---
    {
        PoseRingTest r{};
        PoseRing_Init(r);
        PoseSnap s1 = MakeComplete(4, 0, 0x809882e0, 1);
        s1.xfbIndex = 10;
        PoseRing_Publish(r, s1);
        PoseSnap s2 = MakeComplete(4, 0, 0x809882e0, 1);
        s2.xfbIndex = 12;  // gap: skipped 11
        PoseRing_Publish(r, s2);
        PoseSnap prev{}, cur{};
        Expect(PoseRing_Previous(r, prev) && PoseRing_Latest(r, cur), "pair");
        Expect(cur.xfbIndex == prev.xfbIndex + 2, "sequence gap visible");
        Expect(cur.scnMdlEa == prev.scnMdlEa && cur.modelGen == prev.modelGen,
               "identity stable across gap");
    }

    // --- Matrix classification ---
    {
        Expect(Pose_ClassifyMtx(Ident()).cls == PoseMtx_Rigid, "I is rigid");
        PoseMtx3x4 u = Ident();
        u.m[0] = 2;
        u.m[5] = 2;
        u.m[10] = 2;
        Expect(Pose_ClassifyMtx(u).cls == PoseMtx_UniformScale, "uniform scale");
        PoseMtx3x4 nu = Ident();
        nu.m[0] = 2;
        nu.m[5] = 1;
        nu.m[10] = 1;
        Expect(Pose_ClassifyMtx(nu).cls == PoseMtx_NonUniform, "non-uniform");
        PoseMtx3x4 bad{};
        Expect(Pose_ClassifyMtx(bad).cls == PoseMtx_Singular, "zero singular");
        PoseMtx3x4 nanM = Ident();
        nanM.m[0] = NAN;
        Expect(Pose_ClassifyMtx(nanM).cls == PoseMtx_Invalid, "nan invalid");
        PoseMtx3x4 shear = Ident();
        shear.m[1] = 0.5f;  // break orthonormality
        Expect(Pose_ClassifyMtx(shear).cls == PoseMtx_Shear, "shear");
    }

    // --- Pair evaluation ---
    {
        PoseSnap a = MakeComplete(4, 10, 0x809882e0, 1);
        PoseSnap b = MakeComplete(4, 11, 0x809882e0, 1);
        b.nodes[0].world = Translate(0.5f, 0, 0);
        b.nodes[0].viewPos = Translate(0.5f, 0, 0);
        PosePairVerdict ok = Pose_EvaluatePair(a, b);
        Expect(ok.reject == PosePair_Ok, "consecutive pair ok");

        PoseSnap gap = MakeComplete(4, 13, 0x809882e0, 1);
        Expect(Pose_EvaluatePair(a, gap).reject == PosePair_SeqGap, "seq gap");

        PoseSnap gen = MakeComplete(4, 11, 0x809882e0, 2);
        Expect(Pose_EvaluatePair(a, gen).reject == PosePair_ModelGen, "model gen");

        PoseSnap own = MakeComplete(4, 11, 0x809882e0, 1);
        own.shellEa = 0x11111111;
        own.instanceKey = own.shellEa ^ own.bodyEa;
        Expect(Pose_EvaluatePair(a, own).reject == PosePair_OwnMismatch, "own mismatch");

        PoseSnap hier = MakeComplete(4, 11, 0x809882e0, 1);
        hier.nodes[2].parentIdx = 0;
        Expect(Pose_EvaluatePair(a, hier).reject == PosePair_ParentTable, "parent table");

        PoseSnap tp = MakeComplete(4, 11, 0x809882e0, 1);
        tp.nodes[0].world = Translate(100.f, 0, 0);
        tp.nodes[0].viewPos = Translate(100.f, 0, 0);
        Expect(Pose_EvaluatePair(a, tp).reject == PosePair_Teleport, "teleport");

        PoseSnap inc = a;
        inc.flags &= ~PoseFlag_Complete;
        Expect(Pose_EvaluatePair(a, inc).reject == PosePair_Incomplete, "incomplete");

        PoseSnap warm = MakeComplete(4, 11, 0x809882e0, 1);
        warm.flags |= PoseFlag_Warmup;
        Expect(Pose_EvaluatePair(a, warm).reject == PosePair_Warmup, "warmup curr");
    }

    // --- Rigid TRS / SLERP (A3n) ---
    {
        PoseMtx3x4 a = Translate(0, 0, 0);
        PoseMtx3x4 b = Translate(10, 0, 0);
        PoseMtx3x4 mid{};
        Expect(Pose_InterpRigid(a, b, 0.f, mid), "α0 ok");
        Expect(Pose_MtxMaxAbsDiff(mid, a) < 1e-5f, "α0 exact");
        Expect(Pose_InterpRigid(a, b, 1.f, mid), "α1 ok");
        Expect(Pose_MtxMaxAbsDiff(mid, b) < 1e-5f, "α1 exact");
        Expect(Pose_InterpRigid(a, b, 0.5f, mid), "α0.5 ok");
        Expect(std::fabs(mid.m[3] - 5.f) < 1e-4f, "mid translation");

        // 90° about Y: column0 (1,0,0) → (0,0,-1) roughly via compose.
        PoseRigid ra{}, rb{};
        Expect(Pose_DecomposeRigid(Ident(), ra), "decomp I");
        rb = ra;
        // 180° around Y via quaternion (0,1,0,0)
        rb.q = { 0, 1, 0, 0 };
        PoseMtx3x4 r180{};
        Pose_ComposeRigid(rb, r180);
        PoseMtx3x4 r90{};
        Expect(Pose_InterpRigid(Ident(), r180, 0.5f, r90), "180 mid");
        PoseRigid rm{};
        Expect(Pose_DecomposeRigid(r90, rm), "mid decomp");
        // Shortest-path: opposite hemisphere quat should flip.
        PoseQuat qn = { 0, 0, 0, -1 };  // same as identity after normalize/sign
        PoseQuat qs = Pose_QuatSlerp({ 0, 0, 0, 1 }, qn, 0.5f);
        Expect(std::fabs(Pose_QuatDot(Pose_QuatNormalize(qs), { 0, 0, 0, 1 })) > 0.99f,
               "sign correction → near I");

        PoseMtx3x4 bad{};
        Expect(!Pose_InterpRigid(Ident(), bad, 0.5f, mid), "reject singular");
        PoseMtx3x4 nanM = Ident();
        nanM.m[0] = NAN;
        Expect(!Pose_InterpRigid(Ident(), nanM, 0.5f, mid), "reject nan");

        // ViewPos' = V̂' · W'
        PoseMtx3x4 v0 = Translate(0, 0, 0);
        PoseMtx3x4 v1 = Translate(2, 0, 0);
        PoseMtx3x4 w0 = Translate(1, 0, 0);
        PoseMtx3x4 w1 = Translate(3, 0, 0);
        PoseMtx3x4 vi{}, wi{}, pred{};
        Expect(Pose_InterpRigid(v0, v1, 0.5f, vi) && Pose_InterpRigid(w0, w1, 0.5f, wi),
               "view+world mid");
        Pose_MtxConcat(vi, wi, pred);
        Expect(std::fabs(pred.m[3] - 3.f) < 1e-4f, "V̂·W translation 1+2");
    }

    if (g_fail)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_fail);
        return 1;
    }
    std::printf("pose_capture_test: PASS\n");
    return 0;
}
