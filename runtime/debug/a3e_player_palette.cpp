#include "a3e_player_palette.h"
#include "a1b_pose_capture.h"
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

std::atomic<int> s_env{ -1 };
std::mutex s_mu;
A3ePlayerPaletteSnap s_curr{};
A3ePlayerPaletteSnap s_prev{};
bool s_haveCurr = false;
bool s_havePrev = false;
uint32_t s_dumps = 0;

bool EnvOn()
{
    int v = s_env.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_A3E_PLAYER_PALETTE");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_env.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3e: player palette capture enabled (SCR_A3E_PLAYER_PALETTE); "
                     "no interpolation; requires A3a+A1b");
    }
    return v != 0;
}

float Dist3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool MtxHashChanged(const A3ePlayerPaletteSnap& a, const A3ePlayerPaletteSnap& b)
{
    if (a.mtxCount != b.mtxCount) return true;
    for (uint8_t i = 0; i < a.mtxCount; i++)
    {
        if (a.mtx[i].rowIdx != b.mtx[i].rowIdx) return true;
        if (a.mtx[i].contentHash != b.mtx[i].contentHash) return true;
    }
    return false;
}

// Prefer A3a playerCandidate multi-row draws. When A1b body is valid, prefer
// nearest to playerPos. Without A1b, still accept cand=1 (identityKey is the
// cross-frame ownership key; body EA filled when A1b catches up).
int SelectPlayerDraw(const A3aFrameSnap& cur, const A1bPoseSnap* ply, float* outDist)
{
    int best = -1;
    float bestDist = 1e9f;
    const bool havePly = ply && ply->playerValid;
    for (uint32_t i = 0; i < cur.drawStored; i++)
    {
        const A3aDrawRec& d = cur.draws[i];
        if (d.mtxCount < 2) continue;
        const bool cand = (d.flags & 1) != 0;
        if (!cand && !(havePly && d.vtxCount >= 24 && d.posMtxDirect)) continue;
        float dist = d.bestDist;
        if (havePly && d.mtxCount > 0)
        {
            const float t[3] = { d.mtx[0].m[3], d.mtx[0].m[7], d.mtx[0].m[11] };
            const float dPos = Dist3(t, ply->playerPos);
            if (dPos < dist) dist = dPos;
            if (!cand && dist > 120.f) continue;
        }
        const bool bestCand = best >= 0 && (cur.draws[best].flags & 1) != 0;
        if (best >= 0 && bestCand && !cand) continue;
        if (best >= 0 && cand == bestCand)
        {
            if (havePly && dist > bestDist + 1.f) continue;
            if ((!havePly || std::fabs(dist - bestDist) <= 1.f) &&
                d.vtxCount < cur.draws[best].vtxCount)
                continue;
        }
        best = (int)i;
        bestDist = dist;
    }
    if (outDist) *outDist = bestDist;
    return best;
}

}  // namespace

bool A3e_PlayerPaletteEnabled() { return EnvOn(); }

void A3e_PlayerPalette_OnFrame(const A3aFrameSnap& cur, const A3aFrameSnap* /*prevSnap*/)
{
    if (!EnvOn()) return;

    A1bPoseSnap ply{};
    const bool havePly = A1b_GetCurrent(ply) && ply.playerValid;

    A3ePlayerPaletteSnap snap{};
    snap.xfbIndex = cur.xfbIndex;
    snap.simFrameId = cur.simFrameId;
    if (havePly)
    {
        snap.playerBodyEa = ply.playerBodyEa;
        snap.playerShellEa = ply.playerShellEa;
        snap.playerPos[0] = ply.playerPos[0];
        snap.playerPos[1] = ply.playerPos[1];
        snap.playerPos[2] = ply.playerPos[2];
    }

    float dist = 1e9f;
    const int idx = SelectPlayerDraw(cur, havePly ? &ply : nullptr, &dist);
    if (idx >= 0)
    {
        const A3aDrawRec& d = cur.draws[(uint32_t)idx];
        snap.valid = true;
        snap.identityKey = d.identityKey;
        snap.geomSig = d.geomSig;
        snap.drawSeq = d.drawSeq;
        snap.arrayBase0 = d.arrayBase0;
        snap.vtxCount = d.vtxCount;
        snap.vat = d.vat;
        snap.prim = d.prim;
        snap.mtxCount = (uint8_t)d.mtxCount;
        snap.bestDist = dist;
        for (uint8_t i = 0; i < d.mtxCount && i < kA3aMaxMtxPerDraw; i++)
        {
            snap.mtx[i].rowIdx = d.mtx[i].rowIdx;
            snap.mtx[i].contentHash = d.mtx[i].contentHash;
            std::memcpy(snap.mtx[i].m, d.mtx[i].m, sizeof(snap.mtx[i].m));
        }
    }

    std::lock_guard lk(s_mu);
    if (s_haveCurr)
    {
        s_prev = s_curr;
        s_havePrev = true;
    }
    // Stable ownership: same identityKey (+ same body EA when both known).
    const bool idMatch =
        snap.valid && s_havePrev && s_prev.valid && snap.identityKey == s_prev.identityKey;
    const bool bodyOk = !snap.playerBodyEa || !s_prev.playerBodyEa ||
                        snap.playerBodyEa == s_prev.playerBodyEa;
    if (idMatch && bodyOk)
    {
        snap.stableFrames = (uint8_t)std::min(255, (int)s_prev.stableFrames + 1);
        snap.changedFromPrev = MtxHashChanged(snap, s_prev);
        if (!snap.playerBodyEa && s_prev.playerBodyEa)
            snap.playerBodyEa = s_prev.playerBodyEa;
    }
    else if (snap.valid)
        snap.stableFrames = 1;

    s_curr = snap;
    s_haveCurr = true;

    const bool interesting =
        snap.valid && (snap.stableFrames >= 2 || snap.mtxCount >= 2) && s_dumps < 32;
    if (interesting && (snap.stableFrames <= 3 || (snap.xfbIndex % 60) == 0 || snap.changedFromPrev))
    {
        LOG_INFO("A3e palette: xfb={} body={:08x} shell={:08x} id={:08x} geom={:08x} "
                 "draw#={} vtx={} mtxN={} dist={:.1f} stable={} changed={} rows={}",
                 snap.xfbIndex, snap.playerBodyEa, snap.playerShellEa, snap.identityKey,
                 snap.geomSig, snap.drawSeq, snap.vtxCount, snap.mtxCount, snap.bestDist,
                 snap.stableFrames, snap.changedFromPrev ? 1 : 0, snap.mtxCount);
        for (uint8_t i = 0; i < snap.mtxCount && i < 8; i++)
        {
            LOG_INFO("A3e palette mtx[{}] row={} hash={:08x} T=({:.2f},{:.2f},{:.2f})",
                     i, snap.mtx[i].rowIdx, snap.mtx[i].contentHash, snap.mtx[i].m[3],
                     snap.mtx[i].m[7], snap.mtx[i].m[11]);
        }
        s_dumps++;
    }
}

bool A3e_PlayerPalette_GetCurrent(A3ePlayerPaletteSnap& out)
{
    std::lock_guard lk(s_mu);
    if (!s_haveCurr || !s_curr.valid) return false;
    out = s_curr;
    return true;
}

bool A3e_PlayerPalette_GetPrevious(A3ePlayerPaletteSnap& out)
{
    std::lock_guard lk(s_mu);
    if (!s_havePrev || !s_prev.valid) return false;
    out = s_prev;
    return true;
}

void A3e_PlayerPalette_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!EnvOn()) return;
    A3ePlayerPaletteSnap s{};
    if (!A3e_PlayerPalette_GetCurrent(s))
    {
        std::snprintf(buf, bufSize, "A3e pal: (none)");
        return;
    }
    std::snprintf(buf, bufSize, "A3e pal id=%08x mtx=%u stable=%u chg=%d body=%08x",
                  s.identityKey, s.mtxCount, s.stableFrames, s.changedFromPrev ? 1 : 0,
                  s.playerBodyEa);
}
