#include "a3d_scene_coverage.h"
#include "a2b_fifo_archive.h"
#include "a2c_scratch_replay.h"
#include "../log.h"
#include "../perf.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_env{ -1 };
A3dCoverageReport s_rep{};
std::mutex s_mu;
uint32_t s_dumps = 0;

bool EnvOn()
{
    int v = s_env.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_A3D_SCENE_COVERAGE");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_env.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3d: full-scene coverage audit enabled (SCR_A3D_SCENE_COVERAGE); "
                     "opt-in only, no second XFB");
    }
    return v != 0;
}

bool MtxTChanged(const A3aDrawRec& cur, const A3aFrameSnap& prev)
{
    if (cur.identityKey == 0 || cur.mtxCount == 0) return false;
    for (uint32_t j = 0; j < prev.drawStored; j++)
    {
        const A3aDrawRec& p = prev.draws[j];
        if (p.identityKey != cur.identityKey) continue;
        if (p.mtxCount == 0 || p.mtx[0].rowIdx != cur.mtx[0].rowIdx) return true;
        // Translation components of first matrix sample.
        const float d0 = cur.mtx[0].m[3] - p.mtx[0].m[3];
        const float d1 = cur.mtx[0].m[7] - p.mtx[0].m[7];
        const float d2 = cur.mtx[0].m[11] - p.mtx[0].m[11];
        return (d0 * d0 + d1 * d1 + d2 * d2) > 0.01f;  // ~0.1 unit
    }
    return false;
}

}  // namespace

bool A3d_SceneCoverageEnabled() { return EnvOn(); }

const char* A3d_CatName(A3dDrawCat c)
{
    switch (c)
    {
    case A3dCat_HudUi:
        return "hud_ui";
    case A3dCat_EnvStatic:
        return "env_static";
    case A3dCat_EnvMoving:
        return "env_moving";
    case A3dCat_SkinnedPlayer:
        return "skinned_player";
    case A3dCat_SkinnedOther:
        return "skinned_other";
    case A3dCat_ParticleLike:
        return "particle_like";
    case A3dCat_Other:
        return "other";
    default:
        return "unknown";
    }
}

A3dDrawCat A3d_ClassifyDraw(const A3aDrawRec& d, const A3aFrameSnap* prev)
{
    // HUD / screen-space: vat-heavy tiny quads outside DLs.
    if (!d.inDL && d.mtxCount <= 1 && d.vtxCount <= 6 && d.vat >= 5)
        return A3dCat_HudUi;
    if (d.flags & 1)
        return A3dCat_SkinnedPlayer;
    if (d.posMtxDirect && d.mtxCount >= 2)
        return A3dCat_SkinnedOther;
    // Particle-like: very small non-UI strips/points, often not in DL.
    if (!d.inDL && d.vtxCount > 0 && d.vtxCount <= 12 && d.mtxCount <= 1 && d.vat < 5)
        return A3dCat_ParticleLike;
    if (d.mtxCount == 1)
    {
        const bool moving = prev && MtxTChanged(d, *prev);
        if (moving || d.corrKind == A3aCorr_Appeared || d.corrKind == A3aCorr_Ambiguous)
            return A3dCat_EnvMoving;
        return A3dCat_EnvStatic;
    }
    return A3dCat_Other;
}

void A3d_OnXfbCopy()
{
    if (!EnvOn()) return;

    A3dCoverageReport r{};
    r.xfbIndex = Perf_XfbFrameIndex();
    r.simFrameId = Perf_GuestFrameIndex();

    A3aFrameSnap cur{}, prev{};
    const bool haveCur = A3a_DrawAttrEnabled() && A3a_GetCurrent(cur);
    const bool havePrev = A3a_DrawAttrEnabled() && A3a_GetPrevious(prev);
    if (haveCur)
    {
        r.drawsObserved = cur.drawCount;
        r.drawsCaptured = cur.drawStored;
        r.drawsDropped = cur.drawsDropped;
        r.uniqueRows = cur.uniqueRows;
        r.rowMask = cur.rowMask;
        r.playerCandidates = cur.playerCandidates;
        r.a3aCapped = (cur.drawsDropped > 0) || (cur.drawCount > cur.drawStored);
        r.overflowTotal = cur.overflowTotal;
        for (int c = 0; c < 8; c++)
            r.overflowCat[c] = cur.overflowCat[c];

        for (uint32_t i = 0; i < cur.drawStored; i++)
        {
            const A3aDrawRec& d = cur.draws[i];
            const A3dDrawCat cat = A3d_ClassifyDraw(d, havePrev ? &prev : nullptr);
            A3dCatStats& cs = r.cat[cat];
            cs.observed++;
            if (d.corrKind == A3aCorr_SameObject || d.corrKind == A3aCorr_SameStructure)
                cs.stableCorr++;
            else if (d.corrKind == A3aCorr_Ambiguous || d.corrKind == A3aCorr_Appeared)
                cs.weakCorr++;
            if (d.mtxCount >= 2) cs.multiRow++;
            if (d.inDL) cs.inDL++;
            if (d.vtxCount >= 3) cs.depthHint++;
        }
        // Merge overflow tallies into category observed (attribution incomplete for these).
        for (int c = 0; c < 8; c++)
            r.cat[c].observed += cur.overflowCat[c];
    }

    A2bFrameSummary a2b{};
    if (A2b_FifoCaptureEnabled() && A2b_GetCurrentSummary(a2b))
    {
        r.a2bDraws = a2b.drawCount;
        r.a2bBytes = a2b.totalBytes;
        r.a2bSubs = a2b.submissionCount;
        r.a2bRes = a2b.resourceCount;
        r.a2bResBytes = a2b.resourceBytes;
        r.a2bRequired = a2b.requiredResources;
        r.a2bReqUnresolved = a2b.requiredUnresolved;
        r.a2bTruncated = a2b.truncatedResources;
        r.a2bFlags = a2b.flags;
        r.a2bEfbIn = a2b.efbIncomingValid;
        r.a2bComplete = (a2b.requiredUnresolved == 0) && !(a2b.flags & A2bFrame_Incomplete);
        r.a2bCapped = (a2b.flags & A2bFrame_Incomplete) != 0;
        r.a2bEfbCopies = A2b_GetCrossEfbEdgeCount();
    }

    {
        std::lock_guard lk(s_mu);
        // Preserve last replay note across XFB if set this frame.
        r.lastReplayFamily = s_rep.lastReplayFamily;
        r.lastReplayReady = s_rep.lastReplayReady;
        r.lastReplayMatch = s_rep.lastReplayMatch;
        r.lastReplayPassBytes = s_rep.lastReplayPassBytes;
        s_rep = r;
    }

    // Bounded dumps: prefer gameplay / caps / skinned; do not burn quota on title HUD.
    static uint32_t s_peakObs = 0;
    static uint32_t s_incompDumps = 0;
    const bool newPeak = r.drawsObserved > s_peakObs + 20;
    if (newPeak) s_peakObs = r.drawsObserved;
    const bool gameplayish = r.drawsObserved >= 150 || r.uniqueRows >= 4 ||
                             r.playerCandidates > 0 ||
                             r.cat[A3dCat_SkinnedPlayer].observed > 0 ||
                             r.cat[A3dCat_SkinnedOther].observed > 0;
    const bool incomp = (r.a2bReqUnresolved > 0 || r.a2bCapped || r.a3aCapped) &&
                        (gameplayish || s_incompDumps < 4);
    if (incomp && (r.a2bReqUnresolved > 0 || r.a2bCapped || r.a3aCapped)) s_incompDumps++;
    const bool interesting =
        gameplayish || newPeak || incomp || (r.a2bComplete && r.a2bDraws >= 150);
    if (s_dumps < 64 && (interesting || (r.xfbIndex % 500) == 0))
    {
        const uint32_t capPct =
            r.drawsObserved ? (r.drawsCaptured * 100u) / r.drawsObserved : 100u;
        LOG_INFO("A3d cover: xfb={} sim={} a3a obs/cap/drop={}/{}/{} ({}%) rows={} plyCand={} "
                 "a2b draws={} bytes={} res={}/{} reqUnres={} trunc={} complete={} efbIn={} "
                 "flags={:02x} capped={}/{}",
                 r.xfbIndex, r.simFrameId, r.drawsObserved, r.drawsCaptured, r.drawsDropped, capPct,
                 r.uniqueRows, r.playerCandidates, r.a2bDraws, r.a2bBytes, r.a2bRes, r.a2bRequired,
                 r.a2bReqUnresolved, r.a2bTruncated, r.a2bComplete ? 1 : 0, r.a2bEfbIn ? 1 : 0,
                 r.a2bFlags, r.a3aCapped ? 1 : 0, r.a2bCapped ? 1 : 0);
        LOG_INFO("A3d cats: hud={} envS={} envM={} skinP={} skinO={} part={} other={} "
                 "stable/weak envS={}/{} skinP_multi={} ovf={}/{}/{}/{}/{}/{}/{}/{}",
                 r.cat[A3dCat_HudUi].observed, r.cat[A3dCat_EnvStatic].observed,
                 r.cat[A3dCat_EnvMoving].observed, r.cat[A3dCat_SkinnedPlayer].observed,
                 r.cat[A3dCat_SkinnedOther].observed, r.cat[A3dCat_ParticleLike].observed,
                 r.cat[A3dCat_Other].observed, r.cat[A3dCat_EnvStatic].stableCorr,
                 r.cat[A3dCat_EnvStatic].weakCorr, r.cat[A3dCat_SkinnedPlayer].multiRow,
                 r.overflowCat[0], r.overflowCat[1], r.overflowCat[2], r.overflowCat[3],
                 r.overflowCat[4], r.overflowCat[5], r.overflowCat[6], r.overflowCat[7]);
        s_dumps++;
    }
}

void A3d_NoteReplay(uint32_t family, uint32_t passBytes, bool ready, bool imageMatch)
{
    if (!EnvOn()) return;
    std::lock_guard lk(s_mu);
    s_rep.lastReplayFamily = family;
    s_rep.lastReplayPassBytes = passBytes;
    s_rep.lastReplayReady = ready;
    s_rep.lastReplayMatch = imageMatch;
    static uint32_t notes = 0;
    if (notes < 24)
    {
        LOG_INFO("A3d replay-note: xfb={} family={} passBytes={} ready={} MATCH={} "
                 "(validated≠full-scene)",
                 s_rep.xfbIndex, family, passBytes, ready ? 1 : 0, imageMatch ? 1 : 0);
        notes++;
    }
}

A3dCoverageReport A3d_GetReport()
{
    std::lock_guard lk(s_mu);
    return s_rep;
}

void A3d_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!EnvOn()) return;
    A3dCoverageReport r = A3d_GetReport();
    const uint32_t capPct =
        r.drawsObserved ? (r.drawsCaptured * 100u) / r.drawsObserved : 0u;
    std::snprintf(buf, bufSize,
                  "A3d obs/cap=%u/%u (%u%%) drop=%u a2b=%u%s reqU=%u hud=%u env=%u/%u skin=%u/%u",
                  r.drawsObserved, r.drawsCaptured, capPct, r.drawsDropped, r.a2bDraws,
                  r.a2bComplete ? "*" : "", r.a2bReqUnresolved, r.cat[A3dCat_HudUi].observed,
                  r.cat[A3dCat_EnvStatic].observed, r.cat[A3dCat_EnvMoving].observed,
                  r.cat[A3dCat_SkinnedPlayer].observed, r.cat[A3dCat_SkinnedOther].observed);
}
