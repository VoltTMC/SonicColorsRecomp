// Approach A3f — player draw ownership + skeletal-palette provenance.
#include "a3f_player_provenance.h"
#include "a1b_pose_capture.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../perf.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_trace{ -1 };
std::atomic<int> s_pal{ -1 };
std::atomic<int> s_dump{ -1 };

std::mutex s_mu;
A3fFrameReport s_report{};
bool s_haveReport = false;

A3fPaletteRec s_ring[kA3fRingSize]{};
uint32_t s_ringCount = 0;
uint32_t s_ringWrite = 0;
uint64_t s_seq = 0;
uint32_t s_dumps = 0;

std::atomic<uint32_t> s_playerBodyEa{ 0 };
std::atomic<uint32_t> s_bodyRefreshXfb{ 0 };
std::atomic<uint32_t> s_lastBodyHash{ 0 };

bool EnvFlag(const char* name)
{
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

bool TraceOn()
{
    int v = s_trace.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3F_PLAYER_TRACE") ? 1 : 0;
        s_trace.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3f: player ownership trace enabled (SCR_A3F_PLAYER_TRACE); "
                     "requires SCR_A3A_DRAW_ATTR; auto-uses A1b shell/body sources");
    }
    return v != 0;
}

bool PalOn()
{
    int v = s_pal.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3F_PALETTE_CAPTURE") ? 1 : 0;
        s_pal.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3f: palette capture enabled (SCR_A3F_PALETTE_CAPTURE); "
                     "draw-time XF only; no interpolation");
    }
    return v != 0;
}

bool DumpOn()
{
    int v = s_dump.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3F_DUMP") ? 1 : 0;
        s_dump.store(v, std::memory_order_relaxed);
    }
    return v != 0;
}

bool AnyOn() { return TraceOn() || PalOn(); }

float Dist3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void MulViewPos(const A1bMtx3x4& V, const float p[3], float out[3])
{
    const float* m = V.m;
    out[0] = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
    out[1] = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7];
    out[2] = m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11];
}

const char* StatusName(uint8_t s)
{
    switch (s)
    {
    case A3fStatus_Pass: return "PASS";
    case A3fStatus_Partial: return "PARTIAL";
    case A3fStatus_Fail: return "FAIL";
    case A3fStatus_NA: return "N/A";
    default: return "UNVERIFIED";
    }
}

const char* StageName(int i)
{
    static const char* k[] = {
        "candidate", "guestPlayer", "renderBody", "drawAssoc", "ownership",
        "mtxRows", "mtxSpace", "paletteCap", "paletteComplete", "consecutive",
    };
    return (i >= 0 && i < A3fStage_Count) ? k[i] : "?";
}

uint32_t StructKey(const A3aDrawRec& d)
{
    // Stable under animation: exclude geomSig / matrix contents.
    uint32_t h = d.vcdLo ^ (d.vcdHi * 0x9E3779B9u);
    h ^= d.arrayBase0 * 0x85EBCA6Bu;
    h ^= d.arrayStride0;
    h ^= ((uint32_t)d.prim << 24) | ((uint32_t)d.vat << 16) | ((uint32_t)d.vtxCount & 0xFFFFu);
    h ^= (uint32_t)d.posMtxDirect << 8;
    for (uint16_t i = 0; i < d.mtxCount; i++)
        h ^= ((uint32_t)d.mtx[i].rowIdx + 1u) * (0x1000193u + i * 97u);
    return h ? h : 1u;
}

// Prefer eye-nearest *skeletal* draw (posMtxDirect + mtxCount≥2). Single-matrix
// near hits are often root/env sharing the player eye location — not a palette.
int FindTargetOrBest(const A3aFrameSnap& cur, const A1bPoseSnap* ply, float* outDist,
                     float* outTargetDist)
{
    int bestTarget = -1;
    int bestNear = -1;
    int bestSkinned = -1;
    float bestDist = 1e9f;
    float bestSkinnedDist = 1e9f;
    float targetDist = 1e9f;
    const bool haveEye = ply && ply->playerValid && ply->viewMtxValid;
    float eye[3]{};
    if (haveEye) MulViewPos(ply->viewMtx, ply->playerPos, eye);

    for (uint32_t i = 0; i < cur.drawStored; i++)
    {
        const A3aDrawRec& d = cur.draws[i];
        const bool skinned = d.posMtxDirect && d.mtxCount >= 2;
        if (!(d.flags & 1) && !skinned) continue;
        float dMin = 1e9f;
        if (haveEye)
        {
            for (uint16_t k = 0; k < d.mtxCount; k++)
            {
                const float t[3] = { d.mtx[k].m[3], d.mtx[k].m[7], d.mtx[k].m[11] };
                const float dist = Dist3(t, eye);
                if (dist < dMin) dMin = dist;
            }
            if (dMin < bestDist)
            {
                bestDist = dMin;
                bestNear = (int)i;
            }
            if (skinned && dMin < bestSkinnedDist)
            {
                bestSkinnedDist = dMin;
                bestSkinned = (int)i;
            }
        }
        else if (skinned && bestSkinned < 0)
            bestSkinned = (int)i;
        if (d.identityKey == kA3fTargetIdentity)
        {
            bestTarget = (int)i;
            targetDist = dMin;
        }
    }
    // Accept skinned if within 8 units of absolute nearest (or nearest overall).
    const int pick =
        (haveEye && bestSkinned >= 0 && bestSkinnedDist <= bestDist + 8.f) ? bestSkinned
        : (haveEye && bestNear >= 0)                                         ? bestNear
        : (bestSkinned >= 0)                                                 ? bestSkinned
        : bestTarget;
    if (outDist)
        *outDist = (pick == bestSkinned && bestSkinned >= 0) ? bestSkinnedDist : bestDist;
    if (outTargetDist) *outTargetDist = targetDist;
    return pick;
}

void ClassifyMtx(A3fMtxProv& mp, bool haveEye, const float eye[3], bool haveView)
{
    mp.tEye[0] = mp.m[3];
    mp.tEye[1] = mp.m[7];
    mp.tEye[2] = mp.m[11];
    if (haveEye)
        mp.distToPlayerEye = Dist3(mp.tEye, eye);
    // Draw-time XF pos mtx on this path is model×view (GX convention).
    if (haveView)
        mp.kind = A3fMtxKind_ModelViewBone;
    else
        mp.kind = A3fMtxKind_Unknown;
}

void PushRing(const A3fPaletteRec& rec)
{
    if (s_ringCount > 0)
    {
        const uint32_t prevIdx = (s_ringWrite + kA3fRingSize - 1) % kA3fRingSize;
        A3fPaletteRec& prev = s_ring[prevIdx];
        const bool same = prev.structKey != 0 && rec.structKey != 0 &&
                          prev.structKey == rec.structKey &&
                          (!prev.playerBodyEa || !rec.playerBodyEa ||
                           prev.playerBodyEa == rec.playerBodyEa);
        prev.sameInstanceNext = same ? 1 : 0;
    }
    s_ring[s_ringWrite] = rec;
    s_ringWrite = (s_ringWrite + 1) % kA3fRingSize;
    if (s_ringCount < kA3fRingSize) s_ringCount++;
}

}  // namespace

bool A3f_PlayerTraceEnabled() { return TraceOn(); }
bool A3f_PaletteCaptureEnabled() { return PalOn(); }
bool A3f_NeedsPlayerIdentity() { return AnyOn(); }

void A3f_NotePlayerBody(uint32_t bodyEa)
{
    if (!AnyOn() || !bodyEa) return;
    s_playerBodyEa.store(bodyEa, std::memory_order_relaxed);
}

void A3f_OnBodyWorldMtx(uint32_t bodyEa, const float* mtx12, uint32_t /*dirtyFlags*/)
{
    if (!AnyOn() || !bodyEa || !mtx12) return;
    const uint32_t player = s_playerBodyEa.load(std::memory_order_relaxed);
    if (player && bodyEa == player)
        s_bodyRefreshXfb.fetch_add(1, std::memory_order_relaxed);
    // Content hash of body+0x0C (orientation-heavy; T often 0).
    uint32_t h = 2166136261u;
    for (int i = 0; i < 12; i++)
    {
        uint32_t u;
        std::memcpy(&u, &mtx12[i], 4);
        h ^= u;
        h *= 16777619u;
    }
    s_lastBodyHash.store(h, std::memory_order_relaxed);
}

void A3f_OnXfbBegin()
{
    if (!AnyOn()) return;
    s_bodyRefreshXfb.store(0, std::memory_order_relaxed);
}

void A3f_OnFrame(const A3aFrameSnap& cur, const A3aFrameSnap* /*prev*/)
{
    if (!AnyOn()) return;

    A1bPoseSnap ply{};
    const bool havePly = A1b_GetCurrent(ply) && ply.playerValid;
    if (havePly)
        s_playerBodyEa.store(ply.playerBodyEa, std::memory_order_relaxed);

    A3fFrameReport rep{};
    rep.xfbIndex = cur.xfbIndex;
    rep.simFrameId = cur.simFrameId;
    for (int i = 0; i < A3fStage_Count; i++)
        rep.stage[i] = A3fStatus_Unverified;
    rep.missing = "";

    // Stage: guest player
    if (havePly)
    {
        rep.stage[A3fStage_GuestPlayerIdentified] = A3fStatus_Pass;
        rep.playerShellEa = ply.playerShellEa;
        rep.playerBodyEa = ply.playerBodyEa;
    }
    else
    {
        rep.stage[A3fStage_GuestPlayerIdentified] = A3fStatus_Fail;
        rep.missing = "no A1b player shell/body (enable SCR_A1B_POSE_CAPTURE or A3f)";
    }

    // Stage: render component = body + natural BodyGetWorldMtx refreshes
    rep.bodyRefreshCount = s_bodyRefreshXfb.load(std::memory_order_relaxed);
    if (havePly && rep.bodyRefreshCount > 0)
        rep.stage[A3fStage_RenderComponentIdentified] = A3fStatus_Pass;
    else if (havePly)
    {
        rep.stage[A3fStage_RenderComponentIdentified] = A3fStatus_Partial;
        if (!rep.missing[0])
            rep.missing = "body EA known but BodyGetWorldMtx refresh=0 this XFB";
    }
    else
        rep.stage[A3fStage_RenderComponentIdentified] = A3fStatus_Fail;

    float nearDist = 1e9f;
    float targetDist = 1e9f;
    const int idx =
        FindTargetOrBest(cur, havePly ? &ply : nullptr, &nearDist, &targetDist);
    uint32_t skinned = 0;
    for (uint32_t i = 0; i < cur.drawStored; i++)
        if ((cur.draws[i].flags & 1) || (cur.draws[i].posMtxDirect && cur.draws[i].mtxCount >= 2))
            skinned++;
    rep.skinnedCandCount = skinned;

    if (idx < 0)
    {
        rep.stage[A3fStage_CandidateFound] = A3fStatus_Fail;
        rep.stage[A3fStage_DrawSubmissionAssociated] = A3fStatus_Fail;
        rep.verdict = A3fCand_Unknown;
        if (!rep.missing[0]) rep.missing = "no skinned candidate draw in A3a store";
    }
    else
    {
        const A3aDrawRec& d = cur.draws[(uint32_t)idx];
        const uint32_t structKey = StructKey(d);
        rep.candidateKey = d.identityKey;
        // Candidate stage: PASS if we have an eye-nearest skinned draw with player,
        // or the historical target when player is unknown.
        if (havePly && nearDist < 80.f)
            rep.stage[A3fStage_CandidateFound] = A3fStatus_Pass;
        else if (d.identityKey == kA3fTargetIdentity)
            rep.stage[A3fStage_CandidateFound] = A3fStatus_Partial;
        else
            rep.stage[A3fStage_CandidateFound] = A3fStatus_Partial;

        // Shared geometry / array peers (shared resources ≠ shared instance)
        for (uint32_t i = 0; i < cur.drawStored; i++)
        {
            if ((int)i == idx) continue;
            if (cur.draws[i].geomSig == d.geomSig) rep.sharedGeomPeers++;
            if (d.arrayBase0 && cur.draws[i].arrayBase0 == d.arrayBase0)
                rep.sharedArrayPeers++;
        }

        rep.stage[A3fStage_DrawSubmissionAssociated] = A3fStatus_Pass;
        // PASS = multi-row direct PNMTXIDX palette; PARTIAL = single/indirect row only.
        if (d.posMtxDirect && d.mtxCount >= 2)
            rep.stage[A3fStage_MatrixRowsIdentified] = A3fStatus_Pass;
        else if (d.mtxCount > 0)
            rep.stage[A3fStage_MatrixRowsIdentified] = A3fStatus_Partial;
        else
            rep.stage[A3fStage_MatrixRowsIdentified] = A3fStatus_Fail;

        float eye[3]{};
        bool haveEye = false;
        if (havePly && ply.viewMtxValid)
        {
            MulViewPos(ply.viewMtx, ply.playerPos, eye);
            haveEye = std::isfinite(eye[0]) && std::isfinite(eye[1]) && std::isfinite(eye[2]);
        }

        A3fPaletteRec prec{};
        prec.simFrameId = cur.simFrameId;
        prec.xfbIndex = cur.xfbIndex;
        prec.seq = ++s_seq;
        prec.playerShellEa = havePly ? ply.playerShellEa : 0;
        prec.playerBodyEa = havePly ? ply.playerBodyEa : 0;
        prec.identityKey = d.identityKey;
        prec.structKey = structKey;
        prec.geomSig = d.geomSig;
        prec.arrayBase0 = d.arrayBase0;
        prec.arrayStride0 = d.arrayStride0;
        prec.drawSeq = d.drawSeq;
        prec.bodyRefreshCount = rep.bodyRefreshCount;
        prec.vtxCount = d.vtxCount;
        prec.mtxCount = d.mtxCount;
        prec.mtxUsedTotal = d.mtxCount;
        prec.prim = d.prim;
        prec.vat = d.vat;
        prec.inDL = d.inDL;
        prec.posMtxDirect = d.posMtxDirect;
        prec.target6630DistOk = (targetDist < 80.f) ? 1 : 0;
        if (havePly)
        {
            prec.playerPos[0] = ply.playerPos[0];
            prec.playerPos[1] = ply.playerPos[1];
            prec.playerPos[2] = ply.playerPos[2];
        }
        if (haveEye)
        {
            prec.playerEye[0] = eye[0];
            prec.playerEye[1] = eye[1];
            prec.playerEye[2] = eye[2];
        }

        float maxPair = 0.f;
        float best = 1e9f;
        for (uint16_t i = 0; i < d.mtxCount && i < kA3fMaxMtx; i++)
        {
            A3fMtxProv& mp = prec.mtx[i];
            mp.rowIdx = d.mtx[i].rowIdx;
            mp.contentHash = d.mtx[i].contentHash;
            std::memcpy(mp.m, d.mtx[i].m, sizeof(mp.m));
            mp.usedByVtx = 1;
            mp.vtxRefs = 1;
            ClassifyMtx(mp, haveEye, eye, havePly && ply.viewMtxValid);
            if (mp.distToPlayerEye < best) best = mp.distToPlayerEye;
            for (uint16_t j = 0; j < i; j++)
            {
                const float t0[3] = { prec.mtx[j].tEye[0], prec.mtx[j].tEye[1],
                                      prec.mtx[j].tEye[2] };
                const float r = Dist3(mp.tEye, t0);
                if (r > maxPair) maxPair = r;
            }
        }
        prec.mtxCount = d.mtxCount < kA3fMaxMtx ? d.mtxCount : (uint16_t)kA3fMaxMtx;
        prec.bestDist = best;
        prec.clusterRadius = maxPair;
        prec.paletteComplete =
            (d.vtxCount <= 64 && d.mtxCount <= kA3aMaxMtxPerDraw && d.mtxCount > 0) ? 1 : 0;
        prec.spaceVerified = (haveEye && best < 80.f) ? 1 : 0;

        // Ownership (draw → player): guest shell/body + BodyGetWorldMtx refresh +
        // eye-space bone T proximity + multi-row direct PNMTXIDX. Do NOT prove via
        // geomSig / 66308a11 alone.
        const bool skeletal = d.posMtxDirect && d.mtxCount >= 2;
        const bool ownOk = havePly && haveEye && best < 25.f && rep.playerBodyEa != 0 &&
                           rep.bodyRefreshCount > 0 && skeletal;
        if (ownOk)
        {
            rep.stage[A3fStage_DrawOwnershipVerified] = A3fStatus_Pass;
            rep.verdict = A3fCand_ProvenPlayer;
            prec.ownership = A3fStatus_Pass;
            prec.candVerdict = A3fCand_ProvenPlayer;
            if (d.identityKey == kA3fTargetIdentity)
            {
                // Historical target coincidentally near — still OK, but rare in TR1.
            }
            else if (targetDist < 80.f)
            {
                if (!rep.missing[0])
                    rep.missing = "proven nearest skeletal; 66308a11 also within 80";
            }
            else
            {
                if (!rep.missing[0])
                    rep.missing = "66308a11 REJECTED as player draw (nearest skeletal differs)";
            }
        }
        else if (havePly && haveEye && best < 25.f && rep.playerBodyEa != 0 && !skeletal)
        {
            rep.stage[A3fStage_DrawOwnershipVerified] = A3fStatus_Partial;
            rep.verdict = A3fCand_Partial;
            prec.ownership = A3fStatus_Partial;
            prec.candVerdict = A3fCand_Partial;
            if (!rep.missing[0])
                rep.missing = "near player but not multi-row direct PNMTXIDX (no palette)";
        }
        else if (d.identityKey == kA3fTargetIdentity && havePly && haveEye && best >= 80.f)
        {
            rep.stage[A3fStage_DrawOwnershipVerified] = A3fStatus_Fail;
            rep.verdict = A3fCand_Rejected;
            prec.ownership = A3fStatus_Fail;
            prec.candVerdict = A3fCand_Rejected;
            rep.missing = "66308a11 bone Ts far from player eye — not player-owned";
        }
        else if (d.identityKey == kA3fTargetIdentity && !havePly)
        {
            rep.stage[A3fStage_DrawOwnershipVerified] = A3fStatus_Partial;
            rep.verdict = A3fCand_Partial;
            prec.ownership = A3fStatus_Partial;
            prec.candVerdict = A3fCand_Partial;
            if (!rep.missing[0])
                rep.missing = "66308a11 pre-gameplay only — guest player unresolved";
        }
        else if (havePly && haveEye && best < 80.f)
        {
            rep.stage[A3fStage_DrawOwnershipVerified] = A3fStatus_Partial;
            rep.verdict = A3fCand_Partial;
            prec.ownership = A3fStatus_Partial;
            prec.candVerdict = A3fCand_Partial;
            if (!rep.missing[0])
                rep.missing = (rep.bodyRefreshCount == 0)
                                  ? "near eye but BodyGetWorldMtx refresh=0"
                                  : "eye proximity weak (>=25) or incomplete chain";
            if (best >= 25.f) rep.rejectNearMiss++;
        }
        else
        {
            rep.stage[A3fStage_DrawOwnershipVerified] = A3fStatus_Unverified;
            rep.verdict = A3fCand_Unknown;
            prec.ownership = A3fStatus_Unverified;
        }

        // Separate A3e-target disposition when player known and target absent/far.
        if (havePly && haveEye && targetDist >= 80.f && ownOk)
        {
            // Nearest proven; historical candidate not the player draw.
        }

        rep.stage[A3fStage_MatrixSpaceVerified] =
            prec.spaceVerified ? A3fStatus_Pass
                               : (haveEye ? A3fStatus_Fail : A3fStatus_Partial);

        if (PalOn())
        {
            if (s_ringCount > 0)
            {
                const uint32_t prevIdx = (s_ringWrite + kA3fRingSize - 1) % kA3fRingSize;
                const A3fPaletteRec& prev = s_ring[prevIdx];
                // Prefer structKey + body EA (survives anim); identityKey alone churns.
                const bool same =
                    prev.structKey != 0 && prec.structKey != 0 &&
                    prev.structKey == prec.structKey &&
                    (!prev.playerBodyEa || !prec.playerBodyEa ||
                     prev.playerBodyEa == prec.playerBodyEa);
                prec.sameInstancePrev = same ? 1 : 0;
                rep.stage[A3fStage_ConsecutiveCorrespondence] =
                    same ? A3fStatus_Pass : A3fStatus_Partial;
            }
            else
            {
                prec.sameInstancePrev = 0;
                rep.stage[A3fStage_ConsecutiveCorrespondence] = A3fStatus_Partial;
            }

            rep.stage[A3fStage_PaletteCaptured] = A3fStatus_Pass;
            rep.stage[A3fStage_PaletteComplete] =
                prec.paletteComplete ? A3fStatus_Pass : A3fStatus_Partial;

            std::lock_guard lk(s_mu);
            PushRing(prec);
        }
        else
        {
            rep.stage[A3fStage_PaletteCaptured] = A3fStatus_NA;
            rep.stage[A3fStage_PaletteComplete] = A3fStatus_NA;
            rep.stage[A3fStage_ConsecutiveCorrespondence] = A3fStatus_NA;
        }
    }

    {
        std::lock_guard lk(s_mu);
        s_report = rep;
        s_haveReport = true;
    }

    const bool dump = DumpOn() || TraceOn();
    if (dump && s_dumps < 48 &&
        (rep.stage[A3fStage_CandidateFound] != A3fStatus_Fail) &&
        (s_dumps < 8 || (cur.xfbIndex % 30) == 0 ||
         rep.verdict == A3fCand_ProvenPlayer || rep.verdict == A3fCand_Rejected))
    {
        LOG_INFO("A3f report: xfb={} sim={} verdict={} key={:08x} shell={:08x} body={:08x} "
                 "bodyRefr={} near={:.2f} skinCand={} geomPeers={} arrPeers={} missing={}",
                 rep.xfbIndex, rep.simFrameId,
                 rep.verdict == A3fCand_ProvenPlayer   ? "PROVEN"
                 : rep.verdict == A3fCand_Rejected     ? "REJECTED"
                 : rep.verdict == A3fCand_Partial      ? "PARTIAL"
                                                      : "UNKNOWN",
                 rep.candidateKey, rep.playerShellEa, rep.playerBodyEa, rep.bodyRefreshCount,
                 nearDist, rep.skinnedCandCount, rep.sharedGeomPeers, rep.sharedArrayPeers,
                 rep.missing && rep.missing[0] ? rep.missing : "-");
        for (int i = 0; i < A3fStage_Count; i++)
            LOG_INFO("A3f stage {}: {}", StageName(i), StatusName(rep.stage[i]));

        if (PalOn() && s_ringCount > 0)
        {
            const uint32_t idxR = (s_ringWrite + kA3fRingSize - 1) % kA3fRingSize;
            const A3fPaletteRec& p = s_ring[idxR];
            LOG_INFO("A3f palette: xfb={} id={:08x} struct={:08x} body={:08x} vtx={} "
                     "mtxN={}/{} complete={} space={} own={} bestDist={:.2f} cluster={:.2f} "
                     "samePrev={} tgtNear={} rows={}",
                     p.xfbIndex, p.identityKey, p.structKey, p.playerBodyEa, p.vtxCount,
                     p.mtxCount, p.mtxUsedTotal, p.paletteComplete, p.spaceVerified,
                     StatusName(p.ownership), p.bestDist, p.clusterRadius, p.sameInstancePrev,
                     p.target6630DistOk, p.mtxCount);
            for (uint16_t i = 0; i < p.mtxCount; i++)
            {
                LOG_INFO("A3f mtx[{}] row={} kind={} hash={:08x} T=({:.2f},{:.2f},{:.2f}) "
                         "distEye={:.2f}",
                         i, p.mtx[i].rowIdx,
                         p.mtx[i].kind == A3fMtxKind_ModelViewBone ? "modelViewBone" : "unknown",
                         p.mtx[i].contentHash, p.mtx[i].tEye[0], p.mtx[i].tEye[1],
                         p.mtx[i].tEye[2], p.mtx[i].distToPlayerEye);
            }
        }
        s_dumps++;
    }

    // Body refreshes counted for this XFB window; clear for the next.
    s_bodyRefreshXfb.store(0, std::memory_order_relaxed);
}

bool A3f_GetLatestReport(A3fFrameReport& out)
{
    std::lock_guard lk(s_mu);
    if (!s_haveReport) return false;
    out = s_report;
    return true;
}

bool A3f_GetLatestPalette(A3fPaletteRec& out)
{
    std::lock_guard lk(s_mu);
    if (s_ringCount == 0) return false;
    const uint32_t idx = (s_ringWrite + kA3fRingSize - 1) % kA3fRingSize;
    out = s_ring[idx];
    return true;
}

uint32_t A3f_PaletteRingCount()
{
    std::lock_guard lk(s_mu);
    return s_ringCount;
}

bool A3f_GetPaletteAt(uint32_t idxFromNewest, A3fPaletteRec& out)
{
    std::lock_guard lk(s_mu);
    if (idxFromNewest >= s_ringCount) return false;
    const uint32_t idx =
        (s_ringWrite + kA3fRingSize - 1 - idxFromNewest) % kA3fRingSize;
    out = s_ring[idx];
    return true;
}

void A3f_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!AnyOn()) return;
    A3fFrameReport r{};
    if (!A3f_GetLatestReport(r))
    {
        std::snprintf(buf, bufSize, "A3f: waiting");
        return;
    }
    const char* v = r.verdict == A3fCand_ProvenPlayer   ? "PROVEN"
                    : r.verdict == A3fCand_Rejected     ? "REJ"
                    : r.verdict == A3fCand_Partial      ? "PART"
                                                       : "?";
    std::snprintf(buf, bufSize, "A3f %s key=%08x body=%08x refr=%u nearMiss=%u", v,
                  r.candidateKey, r.playerBodyEa, r.bodyRefreshCount, r.rejectNearMiss);
}
