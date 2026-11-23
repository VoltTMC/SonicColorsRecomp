// Approach A3g — player multi-draw pose aggregation + hierarchy reconstruction.
#include "a3g_pose_aggregate.h"
#include "a3f_player_provenance.h"
#include "../log.h"
#include "../perf.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_agg{ -1 };
std::atomic<int> s_hier{ -1 };
std::atomic<int> s_dump{ -1 };

std::mutex s_mu;
A3gPoseSnap s_ring[kA3gRingSize]{};
uint32_t s_ringCount = 0;
uint32_t s_ringWrite = 0;
uint64_t s_seq = 0;
uint32_t s_dumps = 0;
A3gPoseSnap* s_lastBuild = nullptr;

bool EnvFlag(const char* name)
{
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

bool AggOn()
{
    int v = s_agg.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3G_POSE_AGGREGATE") ? 1 : 0;
        s_agg.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3g: pose aggregation enabled (SCR_A3G_POSE_AGGREGATE); "
                     "multi-draw player palette; no interpolation");
    }
    return v != 0;
}

bool HierOn()
{
    int v = s_hier.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3G_HIERARCHY_TRACE") ? 1 : 0;
        s_hier.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3g: hierarchy trace enabled (SCR_A3G_HIERARCHY_TRACE); "
                     "view/body/root only — bone parents unresolved without guest tree");
    }
    return v != 0;
}

bool DumpOn()
{
    int v = s_dump.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3G_POSE_DUMP") ? 1 : 0;
        s_dump.store(v, std::memory_order_relaxed);
    }
    return v != 0;
}

bool AnyOn() { return AggOn() || HierOn(); }

float Dist3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void MulViewPos(const A3bMtx3x4& V, const float p[3], float out[3])
{
    const float* m = V.m;
    out[0] = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
    out[1] = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7];
    out[2] = m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11];
}

uint32_t HashU32(uint32_t a, uint32_t b)
{
    uint32_t h = 2166136261u;
    h ^= a;
    h *= 16777619u;
    h ^= b;
    h *= 16777619u;
    return h ? h : 1u;
}

uint32_t StructKey(const A3aDrawRec& d)
{
    uint32_t h = d.vcdLo ^ (d.vcdHi * 0x9E3779B9u);
    h ^= d.arrayBase0 * 0x85EBCA6Bu;
    h ^= d.arrayStride0;
    h ^= ((uint32_t)d.prim << 24) | ((uint32_t)d.vat << 16) | ((uint32_t)d.vtxCount & 0xFFFFu);
    h ^= (uint32_t)d.posMtxDirect << 8;
    for (uint16_t i = 0; i < d.mtxCount; i++)
        h ^= ((uint32_t)d.mtx[i].rowIdx + 1u) * (0x1000193u + i * 97u);
    return h ? h : 1u;
}

uint32_t ObsKey(uint32_t structKey, uint8_t row)
{
    return HashU32(structKey, (uint32_t)row + 1u);
}

float DrawBestDist(const A3aDrawRec& d, const float eye[3])
{
    float best = 1e9f;
    for (uint16_t k = 0; k < d.mtxCount; k++)
    {
        const float t[3] = { d.mtx[k].m[3], d.mtx[k].m[7], d.mtx[k].m[11] };
        const float dist = Dist3(t, eye);
        if (dist < best) best = dist;
    }
    return best;
}

bool IsPlayerDraw(const A3aDrawRec& d, bool haveEye, const float eye[3], float* outDist)
{
    const bool skeletal = d.posMtxDirect && d.mtxCount >= 2;
    if (!skeletal) return false;
    if (!haveEye) return false;
    const float dist = DrawBestDist(d, eye);
    if (outDist) *outDist = dist;
    return dist < kA3gEyeNear;
}

void CopyMtx12(const float* src, float* dst) { std::memcpy(dst, src, 12 * sizeof(float)); }

void ToA3b(const float* m12, A3bMtx3x4& out) { CopyMtx12(m12, out.m); }

bool NearMtx(const float* a, const float* b, float eps)
{
    for (int i = 0; i < 12; i++)
        if (std::fabs(a[i] - b[i]) > eps) return false;
    return true;
}

void ClassifyInterp(A3gMtxObs& o, bool matchedPrev)
{
    A3bMtx3x4 mv{};
    ToA3b(o.m, mv);
    A3bRigid rigid{};
    uint32_t fail = 0;
    const bool ok = A3b_Decompose(mv, rigid, &fail);
    o.failBits = fail;
    o.matchedPrev = matchedPrev ? 1 : 0;
    if (!ok || (fail & (A3bFail_NonFinite | A3bFail_Singular | A3bFail_Shear |
                        A3bFail_Reflection)))
    {
        o.interp = A3gInterp_Unsupported;
        return;
    }
    // Non-uniform scale is a soft fail for future bone interp.
    if (fail & A3bFail_Scale)
    {
        o.interp = A3gInterp_Unsupported;
        return;
    }
    if (!matchedPrev)
        o.interp = A3gInterp_Unmatched;
    else
        o.interp = A3gInterp_Ready;
}

void PushRing(const A3gPoseSnap& snap)
{
    if (s_ringCount > 0)
    {
        const uint32_t prevIdx = (s_ringWrite + kA3gRingSize - 1) % kA3gRingSize;
        A3gPoseSnap& prev = s_ring[prevIdx];
        const bool same = prev.playerInstanceKey != 0 &&
                          prev.playerInstanceKey == snap.playerInstanceKey;
        prev.sameInstanceNext = same ? 1 : 0;
    }
    // Explicit memcpy — avoid compiler stack temps for ~300 KiB assignment.
    std::memcpy(&s_ring[s_ringWrite], &snap, sizeof(A3gPoseSnap));
    s_ringWrite = (s_ringWrite + 1) % kA3gRingSize;
    if (s_ringCount < kA3gRingSize) s_ringCount++;
}

const char* StatusName(uint8_t s)
{
    switch (s)
    {
    case A3gStatus_Pass: return "PASS";
    case A3gStatus_Partial: return "PARTIAL";
    case A3gStatus_Fail: return "FAIL";
    case A3gStatus_NA: return "N/A";
    default: return "UNVERIFIED";
    }
}

}  // namespace

bool A3g_PoseAggregateEnabled() { return AggOn(); }
bool A3g_HierarchyTraceEnabled() { return HierOn(); }
bool A3g_NeedsPlayerIdentity() { return AnyOn(); }

void A3g_OnFrame(const A3aFrameSnap& cur, const A3aFrameSnap* /*prev*/)
{
    if (!AnyOn()) return;

    A1bPoseSnap ply{};
    const bool havePly = A1b_GetCurrent(ply) && ply.playerValid;

    // Snap is hundreds of KiB after A3h cap raise — never build it on the stack.
    static A3gPoseSnap s_build{};
    A3gPoseSnap& snap = s_build;
    s_lastBuild = &s_build;
    snap = A3gPoseSnap{};
    snap.simFrameId = cur.simFrameId;
    snap.xfbIndex = cur.xfbIndex;
    snap.seq = ++s_seq;

    float eye[3]{};
    bool haveEye = false;
    if (havePly)
    {
        snap.playerValid = 1;
        snap.playerShellEa = ply.playerShellEa;
        snap.playerBodyEa = ply.playerBodyEa;
        snap.playerInstanceKey = HashU32(ply.playerShellEa, ply.playerBodyEa);
        snap.playerPos[0] = ply.playerPos[0];
        snap.playerPos[1] = ply.playerPos[1];
        snap.playerPos[2] = ply.playerPos[2];
        CopyMtx12(ply.playerWorld.m, snap.bodyWorld.m);
        snap.bodyWorldValid =
            A3b_MtxFinite(snap.bodyWorld) ? 1 : 0;
        if (ply.viewMtxValid)
        {
            CopyMtx12(ply.viewMtx.m, snap.view.m);
            snap.viewValid = A3b_MtxFinite(snap.view) ? 1 : 0;
            if (snap.viewValid)
            {
                MulViewPos(snap.view, snap.playerPos, eye);
                haveEye = std::isfinite(eye[0]) && std::isfinite(eye[1]) && std::isfinite(eye[2]);
                if (haveEye)
                {
                    snap.playerEye[0] = eye[0];
                    snap.playerEye[1] = eye[1];
                    snap.playerEye[2] = eye[2];
                }
            }
        }
    }

    // Body refresh count from A3f (same XFB window).
    A3fFrameReport freport{};
    if (A3f_GetLatestReport(freport) && freport.xfbIndex == cur.xfbIndex)
        snap.bodyRefreshCount = freport.bodyRefreshCount;

    A3bMtx3x4 invView{};
    bool haveInvView = false;
    A3bMtx3x4 viewTimesBody{};
    bool haveViewBody = false;
    if (HierOn() && snap.viewValid)
        haveInvView = A3b_MtxInvertRigid(snap.view, invView);
    if (HierOn() && snap.viewValid && snap.bodyWorldValid)
    {
        A3b_MtxMul(snap.view, snap.bodyWorld, viewTimesBody);
        haveViewBody = true;
    }

    // Collect eye-near skeletal draws, then keep the nearest kA3gMaxDraws
    // (first-seen order alone overflowed with 600+ candidates in busy TR1).
    struct Cand
    {
        uint32_t idx;
        float dist;
    };
    Cand cands[1024];
    uint32_t candN = 0;
    uint32_t candTotal = 0;
    for (uint32_t i = 0; i < cur.drawStored; i++)
    {
        const A3aDrawRec& d = cur.draws[i];
        float dist = 1e9f;
        if (!IsPlayerDraw(d, haveEye, eye, &dist)) continue;
        candTotal++;
        if (candN < 1024)
        {
            cands[candN].idx = i;
            cands[candN].dist = dist;
            candN++;
        }
    }
    // Partial insertion-sort by dist (ascending) for the stored candidates.
    for (uint32_t a = 1; a < candN; a++)
    {
        Cand tmp = cands[a];
        uint32_t b = a;
        while (b > 0 && cands[b - 1].dist > tmp.dist)
        {
            cands[b] = cands[b - 1];
            b--;
        }
        cands[b] = tmp;
    }
    const uint32_t take = candN < (uint32_t)kA3gMaxDraws ? candN : (uint32_t)kA3gMaxDraws;
    if (candTotal > take) snap.drawOverflow = (uint16_t)(candTotal - take);

    for (uint32_t ci = 0; ci < take; ci++)
    {
        const A3aDrawRec& d = cur.draws[cands[ci].idx];
        A3gDrawRef& dr = snap.draws[snap.drawCount];
        dr.drawSeq = d.drawSeq;
        dr.identityKey = d.identityKey;
        dr.structKey = StructKey(d);
        dr.geomSig = d.geomSig;
        dr.arrayBase0 = d.arrayBase0;
        dr.vtxCount = d.vtxCount;
        dr.mtxCount = d.mtxCount;
        dr.prim = d.prim;
        dr.vat = d.vat;
        dr.inDL = d.inDL;
        dr.posMtxDirect = d.posMtxDirect;
        dr.bestDist = cands[ci].dist;
        dr.obsBegin = snap.obsCount;

        for (uint16_t k = 0; k < d.mtxCount && k < kA3aMaxMtxPerDraw; k++)
        {
            if (snap.obsCount >= kA3gMaxObs)
            {
                snap.obsOverflow++;
                break;
            }
            A3gMtxObs& o = snap.obs[snap.obsCount];
            o.structKey = dr.structKey;
            o.rowIdx = d.mtx[k].rowIdx;
            o.obsKey = ObsKey(dr.structKey, o.rowIdx);
            o.contentHash = d.mtx[k].contentHash;
            o.drawSeq = d.drawSeq;
            CopyMtx12(d.mtx[k].m, o.m);
            o.tEye[0] = o.m[3];
            o.tEye[1] = o.m[7];
            o.tEye[2] = o.m[11];
            o.distToPlayerEye = Dist3(o.tEye, eye);

            if (HierOn() && haveInvView)
            {
                A3bMtx3x4 mv{}, model{};
                ToA3b(o.m, mv);
                A3b_MtxMul(invView, mv, model);
                CopyMtx12(model.m, o.modelApprox);
                o.modelApproxValid = 1;
            }

            if (HierOn() && haveViewBody)
            {
                if (NearMtx(o.m, viewTimesBody.m, 0.35f))
                {
                    o.flags |= A3gObs_RootCandidate;
                    o.parentResolved = 1;
                    snap.rootCandidates++;
                }
            }

            snap.obsCount++;
            dr.obsCount++;
        }
        snap.drawCount++;
    }

    // Duplicate / conflict analysis within the frame (evidence-based, not value-only).
    for (uint16_t a = 0; a < snap.obsCount; a++)
    {
        for (uint16_t b = (uint16_t)(a + 1); b < snap.obsCount; b++)
        {
            A3gMtxObs& oa = snap.obs[a];
            A3gMtxObs& ob = snap.obs[b];
            if (oa.rowIdx == ob.rowIdx && oa.drawSeq != ob.drawSeq)
            {
                oa.flags |= A3gObs_DuplicateRow;
                ob.flags |= A3gObs_DuplicateRow;
            }
            if (oa.contentHash == ob.contentHash && oa.drawSeq != ob.drawSeq)
            {
                oa.flags |= A3gObs_DuplicateHash;
                ob.flags |= A3gObs_DuplicateHash;
            }
            if (oa.obsKey == ob.obsKey && oa.contentHash != ob.contentHash)
            {
                oa.flags |= A3gObs_Conflict;
                ob.flags |= A3gObs_Conflict;
                snap.conflictCount++;
            }
        }
    }
    for (uint16_t i = 0; i < snap.obsCount; i++)
    {
        if (snap.obs[i].flags & A3gObs_DuplicateRow) snap.duplicateRowCount++;
        if (snap.obs[i].flags & A3gObs_DuplicateHash) snap.duplicateHashCount++;
    }

    // Unique obsKeys
    for (uint16_t i = 0; i < snap.obsCount; i++)
    {
        bool seen = false;
        for (uint16_t j = 0; j < i; j++)
            if (snap.obs[j].obsKey == snap.obs[i].obsKey)
            {
                seen = true;
                break;
            }
        if (!seen) snap.uniqueObsKeys++;
    }

    // Hierarchy: only view + optional body/root resolved; remaining parents unresolved.
    if (HierOn())
    {
        snap.hierarchyPartial = (snap.viewValid || snap.bodyWorldValid) ? 1 : 0;
        for (uint16_t i = 0; i < snap.obsCount; i++)
            if (!snap.obs[i].parentResolved) snap.unresolvedParents++;
    }
    else
    {
        snap.unresolvedParents = snap.obsCount;  // not traced
    }

    // Temporal match vs previous ring entry (same playerInstanceKey + obsKey).
    bool matchedAny = false;
    if (s_ringCount > 0 && snap.playerInstanceKey != 0)
    {
        const uint32_t prevIdx = (s_ringWrite + kA3gRingSize - 1) % kA3gRingSize;
        const A3gPoseSnap& prev = s_ring[prevIdx];
        const bool sameInst = prev.playerInstanceKey == snap.playerInstanceKey;
        snap.sameInstancePrev = sameInst ? 1 : 0;
        if (sameInst)
        {
            matchedAny = true;
            for (uint16_t i = 0; i < snap.obsCount; i++)
            {
                bool hit = false;
                for (uint16_t j = 0; j < prev.obsCount; j++)
                {
                    if (prev.obs[j].obsKey == snap.obs[i].obsKey)
                    {
                        hit = true;
                        break;
                    }
                }
                ClassifyInterp(snap.obs[i], hit);
            }
        }
        else
        {
            for (uint16_t i = 0; i < snap.obsCount; i++)
                ClassifyInterp(snap.obs[i], false);
        }
    }
    else
    {
        for (uint16_t i = 0; i < snap.obsCount; i++)
            ClassifyInterp(snap.obs[i], false);
    }

    for (uint16_t i = 0; i < snap.obsCount; i++)
    {
        if (snap.obs[i].interp == A3gInterp_Ready) snap.interpReady++;
        else if (snap.obs[i].interp == A3gInterp_Unsupported) snap.interpUnsupported++;
        else if (snap.obs[i].interp == A3gInterp_Unmatched) snap.interpUnmatched++;
    }

    // Completeness of the *aggregated observation* (not a full skeleton):
    // PASS = player known, ≥1 draw stored, no store overflow/conflict.
    // PARTIAL = overflow or hierarchy parents unresolved (expected without guest tree).
    if (!havePly || !haveEye)
        snap.completeness = A3gStatus_Fail;
    else if (snap.drawCount == 0)
        snap.completeness = A3gStatus_Fail;
    else if (snap.drawOverflow || snap.obsOverflow || snap.conflictCount)
        snap.completeness = A3gStatus_Partial;
    else if (HierOn() && snap.unresolvedParents > 0)
        snap.completeness = A3gStatus_Partial;  // bone parents unknown
    else
        snap.completeness = A3gStatus_Pass;

    // poseComplete = all collected draws fit without overflow (hierarchy may still be partial).
    snap.poseComplete =
        (havePly && haveEye && snap.drawCount > 0 && snap.drawOverflow == 0 &&
         snap.obsOverflow == 0 && snap.conflictCount == 0)
            ? 1
            : 0;

    if (AggOn() || HierOn())
    {
        std::lock_guard lk(s_mu);
        PushRing(snap);
    }

    const bool dump = DumpOn() || AggOn() || HierOn();
    if (dump && s_dumps < 40 && snap.drawCount > 0 &&
        (s_dumps < 6 || (cur.xfbIndex % 45) == 0 || snap.poseComplete))
    {
        LOG_INFO("A3g pose: xfb={} sim={} inst={:08x} shell={:08x} body={:08x} "
                 "draws={}/{}+ovf{} obs={}/{}+ovf{} uniq={} dupRow={} dupHash={} "
                 "conflict={} roots={} unresPar={} interpReady={}/unsup={}/unmatch={} "
                 "complete={} samePrev={} hier={} matched={}",
                 snap.xfbIndex, snap.simFrameId, snap.playerInstanceKey, snap.playerShellEa,
                 snap.playerBodyEa, snap.drawCount, kA3gMaxDraws, snap.drawOverflow,
                 snap.obsCount, kA3gMaxObs, snap.obsOverflow, snap.uniqueObsKeys,
                 snap.duplicateRowCount, snap.duplicateHashCount, snap.conflictCount,
                 snap.rootCandidates, snap.unresolvedParents, snap.interpReady,
                 snap.interpUnsupported, snap.interpUnmatched, StatusName(snap.completeness),
                 snap.sameInstancePrev, snap.hierarchyPartial, matchedAny ? 1 : 0);
        for (uint16_t i = 0; i < snap.drawCount && i < 8; i++)
        {
            const A3gDrawRef& dr = snap.draws[i];
            LOG_INFO("A3g draw[{}] seq={} struct={:08x} geom={:08x} vtx={} mtxN={} "
                     "dist={:.2f} obs={}+{}",
                     i, dr.drawSeq, dr.structKey, dr.geomSig, dr.vtxCount, dr.mtxCount,
                     dr.bestDist, dr.obsBegin, dr.obsCount);
        }
        if (HierOn())
        {
            LOG_INFO("A3g hier: view={} bodyWorld={} invView={} view*body={} "
                     "roots={} unresolvedParents={} "
                     "(bone parent links NOT reconstructed — no guest skeleton tree)",
                     snap.viewValid, snap.bodyWorldValid, haveInvView ? 1 : 0,
                     haveViewBody ? 1 : 0, snap.rootCandidates, snap.unresolvedParents);
        }
        // Sample a few obs
        for (uint16_t i = 0; i < snap.obsCount && i < 6; i++)
        {
            const A3gMtxObs& o = snap.obs[i];
            LOG_INFO("A3g obs[{}] key={:08x} row={} hash={:08x} flags={:02x} interp={} "
                     "dist={:.2f} T=({:.2f},{:.2f},{:.2f}) modelApprox={}",
                     i, o.obsKey, o.rowIdx, o.contentHash, o.flags,
                     o.interp == A3gInterp_Ready         ? "READY"
                     : o.interp == A3gInterp_Unsupported ? "UNSUP"
                     : o.interp == A3gInterp_Unmatched   ? "UNMATCH"
                                                         : "?",
                     o.distToPlayerEye, o.tEye[0], o.tEye[1], o.tEye[2],
                     o.modelApproxValid);
        }
        s_dumps++;
    }
}

bool A3g_GetLatest(A3gPoseSnap& out)
{
    std::lock_guard lk(s_mu);
    if (s_ringCount == 0) return false;
    const uint32_t idx = (s_ringWrite + kA3gRingSize - 1) % kA3gRingSize;
    std::memcpy(&out, &s_ring[idx], sizeof(A3gPoseSnap));
    return true;
}

bool A3g_GetLatestIdentity(uint32_t* outInst, uint32_t* outShell, uint32_t* outBody,
                           uint16_t* outObsCount)
{
    // Prefer the in-flight build (same XFB as A3h_OnFrame) before the ring copy.
    if (s_lastBuild && s_lastBuild->playerValid)
    {
        if (outInst) *outInst = s_lastBuild->playerInstanceKey;
        if (outShell) *outShell = s_lastBuild->playerShellEa;
        if (outBody) *outBody = s_lastBuild->playerBodyEa;
        if (outObsCount) *outObsCount = s_lastBuild->obsCount;
        return true;
    }
    std::lock_guard lk(s_mu);
    if (s_ringCount == 0) return false;
    const uint32_t idx = (s_ringWrite + kA3gRingSize - 1) % kA3gRingSize;
    const A3gPoseSnap& s = s_ring[idx];
    if (outInst) *outInst = s.playerInstanceKey;
    if (outShell) *outShell = s.playerShellEa;
    if (outBody) *outBody = s.playerBodyEa;
    if (outObsCount) *outObsCount = s.obsCount;
    return s.playerValid != 0;
}

const A3gPoseSnap* A3g_PeekLastBuild()
{
    return s_lastBuild;
}

uint32_t A3g_RingCount()
{
    std::lock_guard lk(s_mu);
    return s_ringCount;
}

bool A3g_GetAt(uint32_t idxFromNewest, A3gPoseSnap& out)
{
    std::lock_guard lk(s_mu);
    if (idxFromNewest >= s_ringCount) return false;
    const uint32_t idx =
        (s_ringWrite + kA3gRingSize - 1 - idxFromNewest) % kA3gRingSize;
    out = s_ring[idx];
    return true;
}

void A3g_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!AnyOn()) return;
    A3gPoseSnap s{};
    if (!A3g_GetLatest(s))
    {
        std::snprintf(buf, bufSize, "A3g: waiting");
        return;
    }
    std::snprintf(buf, bufSize, "A3g %s draws=%u obs=%u uniq=%u ready=%u roots=%u",
                  StatusName(s.completeness), s.drawCount, s.obsCount, s.uniqueObsKeys,
                  s.interpReady, s.rootCandidates);
}
