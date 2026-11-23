// Approach A3a — draw-level PNMTXIDX attribution + scratch matrix override.
#include "a3a_draw_attr.h"
#include "a1b_pose_capture.h"
#include "a3e_budgets.h"
#include "a3e_player_palette.h"
#include "a3f_player_provenance.h"
#include "a3g_pose_aggregate.h"
#include "a3h_skeleton_palette.h"
#include "a3i_model_ownership.h"
#include "../gx/vertex_loader.h"
#include "../gx/gx_state.h"
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

std::atomic<int> s_attrEnv{ -1 };
std::atomic<int> s_ovrEnv{ -1 };

bool AttrOn()
{
    int v = s_attrEnv.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_A3A_DRAW_ATTR");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_attrEnv.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3a: draw-level transform attribution enabled (SCR_A3A_DRAW_ATTR)");
    }
    return v != 0;
}

bool OvrOn()
{
    int v = s_ovrEnv.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_A3A_MATRIX_OVERRIDE");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_ovrEnv.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3a: scratch matrix override enabled (SCR_A3A_MATRIX_OVERRIDE); live XF untouched");
    }
    return v != 0;
}

uint32_t HashBytes(const void* p, size_t n)
{
    const uint8_t* b = (const uint8_t*)p;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++)
    {
        h ^= b[i];
        h *= 16777619u;
    }
    return h;
}

uint32_t HashMtx(const float m[12])
{
    return HashBytes(m, 12 * sizeof(float));
}

void ReadXfRowMtx(uint8_t rowIdx, float out[12])
{
    // rowIdx is PNMTXIDX; matrix occupies rows rowIdx..rowIdx+2 → xfmem[row*4].
    const uint32_t base = (uint32_t)rowIdx * 4u;
    for (int i = 0; i < 12; i++)
    {
        union
        {
            uint32_t u;
            float f;
        } c;
        c.u = g_gx.xfmem[(base + (uint32_t)i) & 0xFFF];
        out[i] = c.f;
    }
}

void WriteXfRowMtx(uint8_t rowIdx, const float in[12])
{
    const uint32_t base = (uint32_t)rowIdx * 4u;
    for (int i = 0; i < 12; i++)
    {
        union
        {
            uint32_t u;
            float f;
        } c;
        c.f = in[i];
        g_gx.xfmem[(base + (uint32_t)i) & 0xFFF] = c.u;
    }
}

float MtxTx(const float m[12]) { return m[3]; }
float MtxTy(const float m[12]) { return m[7]; }
float MtxTz(const float m[12]) { return m[11]; }

bool Finite12(const float m[12])
{
    for (int i = 0; i < 12; i++)
        if (!std::isfinite(m[i])) return false;
    return true;
}

std::mutex s_mu;
A3aFrameSnap s_build{}, s_curr{}, s_prev{};
bool s_haveCurr = false, s_havePrev = false;
std::atomic<uint32_t> s_drawSeq{ 0 };
uint64_t s_simTag = 0;

A3aOverrideStats s_ovr{};
uint8_t s_autoRow = 0xFF;
bool s_ovrSuspended = false;

uint32_t ParseMode()
{
    const char* m = std::getenv("SCR_A3A_OVERRIDE_MODE");
    if (!m || !m[0]) return 1;  // default noop when override enabled
    if (m[0] == 't' || m[0] == 'T' || std::strcmp(m, "2") == 0) return 2;
    if (m[0] == 'n' || m[0] == 'N' || std::strcmp(m, "1") == 0) return 1;
    if (std::strcmp(m, "0") == 0) return 0;
    return 1;
}

uint8_t ParseRow()
{
    const char* r = std::getenv("SCR_A3A_OVERRIDE_ROW");
    if (!r || !r[0]) return 0xFF;  // auto
    int v = std::atoi(r);
    if (v < 0 || v > 63) return 0xFF;
    return (uint8_t)v;
}

float ParseF(const char* name, float def)
{
    const char* e = std::getenv(name);
    if (!e || !e[0]) return def;
    return (float)std::atof(e);
}

void EnsureDrawCap(A3aFrameSnap& snap)
{
    const uint32_t cap = A3a_CapMaxDraws();
    if (snap.drawCap != cap || snap.draws.size() != cap)
    {
        snap.draws.assign(cap, A3aDrawRec{});
        snap.drawCap = cap;
    }
}

void ResetBuild()
{
    const uint32_t cap = A3a_CapMaxDraws();
    s_build = A3aFrameSnap{};
    s_build.draws.assign(cap, A3aDrawRec{});
    s_build.drawCap = cap;
    s_drawSeq.store(0, std::memory_order_relaxed);
}

// Must stay aligned with A3dDrawCat indices (a3d_scene_coverage.h).
uint8_t OverflowCatOf(const A3aDrawRec& d)
{
    if (!d.inDL && d.mtxCount <= 1 && d.vtxCount <= 6 && d.vat >= 5) return 1;  // HudUi
    if (d.flags & 1) return 4;                                                  // SkinnedPlayer
    if (d.posMtxDirect && d.mtxCount >= 2) return 5;                             // SkinnedOther
    if (!d.inDL && d.vtxCount > 0 && d.vtxCount <= 12 && d.mtxCount <= 1 && d.vat < 5)
        return 6;  // ParticleLike
    if (d.mtxCount == 1) return 2;  // EnvStatic (no prev-frame move detect on overflow)
    return 7;                       // Other
}

uint32_t IdentityKey(const A3aDrawRec& d)
{
    // Stable across frames: geom/VCD/array — NOT matrix hash / slot alone.
    uint32_t h = d.vcdLo ^ (d.vcdHi * 0x9E3779B9u);
    h ^= d.arrayBase0 * 0x85EBCA6Bu;
    h ^= d.arrayStride0;
    h ^= ((uint32_t)d.prim << 24) | ((uint32_t)d.vat << 16) | (d.vtxCount & 0xFFFFu);
    h ^= d.geomSig;
    h ^= (uint32_t)d.posMtxDirect << 8;
    // Include which rows are used (pattern), not their contents.
    for (uint16_t i = 0; i < d.mtxCount; i++)
        h ^= ((uint32_t)d.mtx[i].rowIdx + 1u) * (0x1000193u + i * 97u);
    return h ? h : 1u;
}

void Correlate()
{
    if (!s_havePrev) return;
    s_curr.corrSame = s_curr.corrStruct = s_curr.corrAmbiguous = s_curr.corrAppeared = 0;
    uint32_t prevMatched = 0;
    for (uint32_t i = 0; i < s_curr.drawStored; i++)
    {
        A3aDrawRec& d = s_curr.draws[i];
        int hits = 0;
        int hitJ = -1;
        for (uint32_t j = 0; j < s_prev.drawStored; j++)
        {
            if (s_prev.draws[j].identityKey == d.identityKey)
            {
                hits++;
                hitJ = (int)j;
            }
        }
        if (hits == 0)
        {
            d.corrKind = A3aCorr_Appeared;
            s_curr.corrAppeared++;
        }
        else if (hits > 1)
        {
            d.corrKind = A3aCorr_Ambiguous;
            s_curr.corrAmbiguous++;
        }
        else
        {
            prevMatched |= 1u;  // mark activity
            const A3aDrawRec& p = s_prev.draws[hitJ];
            bool mtxChanged = false;
            if (d.mtxCount == p.mtxCount)
            {
                for (uint16_t k = 0; k < d.mtxCount; k++)
                    if (d.mtx[k].contentHash != p.mtx[k].contentHash ||
                        d.mtx[k].rowIdx != p.mtx[k].rowIdx)
                        mtxChanged = true;
            }
            else
                mtxChanged = true;
            if (mtxChanged)
            {
                d.corrKind = A3aCorr_SameObject;
                s_curr.corrSame++;
            }
            else
            {
                d.corrKind = A3aCorr_SameStructure;
                s_curr.corrStruct++;
            }
        }
        (void)prevMatched;
    }
}

void DumpFrameLocked()
{
    static int dumps = 0;
    A1bPoseSnap plyCheck{};
    const bool plyOk = A1b_GetCurrent(plyCheck) && plyCheck.playerValid;
    const bool interesting = s_curr.playerCandidates > 0 || s_curr.uniqueRows >= 3 ||
                             s_curr.drawCount > 500 || plyOk || (s_curr.xfbIndex % 200) == 0;
    if (!interesting) return;
    // Rate-limit: first 24 dumps free; then at most every 120 xfbs unless new ply edge.
    static bool everPly = false;
    const bool plyEdge = plyOk && !everPly;
    if (plyOk) everPly = true;
    if (dumps >= 24 && !plyEdge && (s_curr.xfbIndex % 120) != 0 && s_curr.playerCandidates == 0)
        return;
    if (dumps >= 24 && !plyEdge && (s_curr.xfbIndex % 60) != 0 && s_curr.playerCandidates > 0)
        return;
    dumps++;

    LOG_INFO("A3a frame: xfb={} sim={} draws={}/{} drop={} rows={} mask={:016x} "
             "playerCand={} ply={} corr same={} struct={} amb={} new={} ovfTot={}",
             s_curr.xfbIndex, s_curr.simFrameId, s_curr.drawStored, s_curr.drawCount,
             s_curr.drawsDropped, s_curr.uniqueRows, s_curr.rowMask, s_curr.playerCandidates,
             plyOk ? 1 : 0, s_curr.corrSame, s_curr.corrStruct, s_curr.corrAmbiguous,
             s_curr.corrAppeared, s_curr.overflowTotal);

    int logged = 0;
    for (uint32_t i = 0; i < s_curr.drawStored && logged < 8; i++)
    {
        const A3aDrawRec& d = s_curr.draws[i];
        if (!(d.flags & 1) && logged >= 2) continue;
        LOG_INFO("A3a draw#{} prim={} vat={} vtx={} dl={} posMtxDir={} defRow={} "
                 "arr={:08x}/{} geom={:08x} id={:08x} corr={} cand={} bestRow={} dist={:.1f} "
                 "mtxN={}",
                 d.drawSeq, d.prim, d.vat, d.vtxCount, d.inDL, d.posMtxDirect, d.defPosMtx,
                 d.arrayBase0, d.arrayStride0, d.geomSig, d.identityKey, d.corrKind,
                 (d.flags & 1) ? 1 : 0, d.bestRow, d.bestDist, d.mtxCount);
        for (uint16_t k = 0; k < d.mtxCount && k < 4; k++)
        {
            const A3aMtxSample& m = d.mtx[k];
            LOG_INFO("A3a   mtx row={} hash={:08x} T=({:.2f},{:.2f},{:.2f})", m.rowIdx,
                     m.contentHash, MtxTx(m.m), MtxTy(m.m), MtxTz(m.m));
        }
        logged++;
    }

    if (s_curr.playerCandidates && s_havePrev)
    {
        A1bPoseSnap ply{}, prevPly{};
        const bool havePly = A1b_GetCurrent(ply) && A1b_GetPrevious(prevPly) && ply.playerValid &&
                             prevPly.playerValid;
        if (havePly)
        {
            const float pdx = ply.playerPos[0] - prevPly.playerPos[0];
            const float pdy = ply.playerPos[1] - prevPly.playerPos[1];
            const float pdz = ply.playerPos[2] - prevPly.playerPos[2];
            const float vdx = ply.viewMtx.m[3] - prevPly.viewMtx.m[3];
            const float vdy = ply.viewMtx.m[7] - prevPly.viewMtx.m[7];
            const float vdz = ply.viewMtx.m[11] - prevPly.viewMtx.m[11];
            LOG_INFO("A3a playerΔ=({:.2f},{:.2f},{:.2f}) viewTΔ=({:.2f},{:.2f},{:.2f}) body={:08x}",
                     pdx, pdy, pdz, vdx, vdy, vdz, ply.playerBodyEa);
        }
        int deltaLogs = 0;
        for (uint32_t i = 0; i < s_curr.drawStored && deltaLogs < 6; i++)
        {
            const A3aDrawRec& d = s_curr.draws[i];
            if (!(d.flags & 1)) continue;
            for (uint32_t j = 0; j < s_prev.drawStored; j++)
            {
                if (s_prev.draws[j].identityKey != d.identityKey) continue;
                const A3aDrawRec& p = s_prev.draws[j];
                for (uint16_t k = 0; k < d.mtxCount; k++)
                {
                    for (uint16_t q = 0; q < p.mtxCount; q++)
                    {
                        if (d.mtx[k].rowIdx != p.mtx[q].rowIdx) continue;
                        const float dx = MtxTx(d.mtx[k].m) - MtxTx(p.mtx[q].m);
                        const float dy = MtxTy(d.mtx[k].m) - MtxTy(p.mtx[q].m);
                        const float dz = MtxTz(d.mtx[k].m) - MtxTz(p.mtx[q].m);
                        if (std::fabs(dx) + std::fabs(dy) + std::fabs(dz) < 0.05f) continue;
                        LOG_INFO("A3a candΔ id={:08x} row={} dT=({:.2f},{:.2f},{:.2f}) vtx={} mtxN={}",
                                 d.identityKey, d.mtx[k].rowIdx, dx, dy, dz, d.vtxCount,
                                 d.mtxCount);
                        if (s_autoRow == 0xFF)
                            s_autoRow = d.mtx[k].rowIdx;
                        deltaLogs++;
                    }
                }
            }
        }
    }
}

}  // namespace

bool A3a_DrawAttrEnabled() { return AttrOn(); }
bool A3a_MatrixOverrideEnabled() { return OvrOn(); }

uint8_t A3a_OverrideRow()
{
    uint8_t r = ParseRow();
    if (r != 0xFF) return r;
    return s_autoRow;
}

uint32_t A3a_OverrideMode() { return OvrOn() ? ParseMode() : 0; }

void A3a_OnDraw(int prim, int vat, uint32_t count, const void* hostVerts, uint32_t hostVertStride,
                const VertexLayout& layout)
{
    if (!AttrOn() || !hostVerts || count == 0 || Render_ReplayIsolationActive()) return;

    const uint8_t* base = (const uint8_t*)hostVerts;
    const uint32_t sampleN = count < 64 ? count : 64;

    uint8_t usedRows[64];
    uint8_t usedN = 0;
    auto addRow = [&](uint8_t row) {
        if (row > 63) return;
        for (uint8_t i = 0; i < usedN; i++)
            if (usedRows[i] == row) return;
        if (usedN < 64) usedRows[usedN++] = row;
    };

    const uint8_t defRow = (uint8_t)(g_gx.xfregs[XF_MATINDEX_A] & 63);
    const bool direct = layout.posMtxIdx != 0;

    for (uint32_t v = 0; v < sampleN; v++)
    {
        const HostVertex* hv = (const HostVertex*)(base + (size_t)v * hostVertStride);
        uint8_t row = direct ? (uint8_t)(hv->mtx[0] & 0xFF) : defRow;
        addRow(row);
        if (usedN >= kA3aMaxMtxPerDraw) break;
    }

    float geomBuf[48];
    uint32_t geomN = 0;
    for (uint32_t v = 0; v < sampleN && geomN < 48; v++)
    {
        const HostVertex* hv = (const HostVertex*)(base + (size_t)v * hostVertStride);
        geomBuf[geomN++] = hv->pos[0];
        geomBuf[geomN++] = hv->pos[1];
        geomBuf[geomN++] = hv->pos[2];
    }

    A3aDrawRec rec{};
    rec.simFrameId = s_simTag ? s_simTag : Perf_GuestFrameIndex();
    rec.drawSeq = s_drawSeq.fetch_add(1, std::memory_order_relaxed);
    rec.prim = (uint8_t)prim;
    rec.vat = (uint8_t)vat;
    rec.inDL = GX_InDisplayList() ? 1 : 0;
    rec.posMtxDirect = direct ? 1 : 0;
    rec.vtxCount = count > 0xFFFF ? 0xFFFF : (uint16_t)count;
    rec.vcdLo = g_gx.cp[CP_VCD_LO];
    rec.vcdHi = g_gx.cp[CP_VCD_HI];
    rec.arrayBase0 = g_gx.cp[CP_ARRAY_BASE + 0];
    rec.arrayStride0 = g_gx.cp[CP_ARRAY_STRIDE + 0];
    rec.geomSig = HashBytes(geomBuf, geomN * sizeof(float));
    rec.defPosMtx = defRow;

    A1bPoseSnap ply{};
    float eyePos[3]{};
    bool haveEye = false;
    if (A1b_GetCurrent(ply) && ply.playerValid)
    {
        rec.playerValid = true;
        rec.playerPos[0] = ply.playerPos[0];
        rec.playerPos[1] = ply.playerPos[1];
        rec.playerPos[2] = ply.playerPos[2];
        // GX character matrices are typically model×view; compare in view space.
        if (ply.viewMtxValid)
        {
            const float* V = ply.viewMtx.m;
            const float x = ply.playerPos[0], y = ply.playerPos[1], z = ply.playerPos[2];
            eyePos[0] = V[0] * x + V[1] * y + V[2] * z + V[3];
            eyePos[1] = V[4] * x + V[5] * y + V[6] * z + V[7];
            eyePos[2] = V[8] * x + V[9] * y + V[10] * z + V[11];
            haveEye = std::isfinite(eyePos[0]) && std::isfinite(eyePos[1]) &&
                      std::isfinite(eyePos[2]);
        }
    }

    const uint8_t storeN = usedN < kA3aMaxMtxPerDraw ? usedN : (uint8_t)kA3aMaxMtxPerDraw;
    rec.mtxCount = storeN;
    float bestDist = 1e9f;
    uint8_t bestRow = defRow;
    for (uint8_t i = 0; i < storeN; i++)
    {
        A3aMtxSample& s = rec.mtx[i];
        s.rowIdx = usedRows[i];
        ReadXfRowMtx(s.rowIdx, s.m);
        if (!Finite12(s.m))
            std::memset(s.m, 0, sizeof(s.m));
        s.contentHash = HashMtx(s.m);
        if (rec.playerValid)
        {
            const float* ref = haveEye ? eyePos : rec.playerPos;
            const float dx = MtxTx(s.m) - ref[0];
            const float dy = MtxTy(s.m) - ref[1];
            const float dz = MtxTz(s.m) - ref[2];
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestRow = s.rowIdx;
            }
        }
    }
    rec.bestRow = bestRow;
    rec.bestDist = bestDist;
    // Candidate: near player (view or world) OR skinned multi-mtx gameplay mesh.
    if (rec.vtxCount >= 8 &&
        ((rec.playerValid && bestDist < 80.f) || (direct && storeN >= 2 && rec.vtxCount >= 24)))
        rec.flags |= 1;

    rec.identityKey = IdentityKey(rec);

    std::lock_guard lk(s_mu);
    s_build.drawCount++;
    for (uint8_t i = 0; i < storeN; i++)
    {
        const uint8_t r = usedRows[i];
        if (r < 64) s_build.rowMask |= (1ull << r);
    }
    if (rec.flags & 1) s_build.playerCandidates++;
    EnsureDrawCap(s_build);
    if (s_build.drawStored < s_build.drawCap)
        s_build.draws[s_build.drawStored++] = rec;
    else
    {
        s_build.drawsDropped++;
        s_build.overflowTotal++;
        const uint8_t cat = OverflowCatOf(rec);
        if (cat < kA3aOverflowCatCount) s_build.overflowCat[cat]++;
    }
}

void A3a_OnSimFrameEnd()
{
    if (!AttrOn()) return;
    s_simTag = Perf_GuestFrameIndex();
}

void A3a_OnXfbCopy()
{
    if (!AttrOn()) return;
    std::lock_guard lk(s_mu);
    s_build.xfbIndex = Perf_XfbFrameIndex();
    s_build.simFrameId = s_simTag ? s_simTag : Perf_GuestFrameIndex();
    s_build.uniqueRows = 0;
    for (uint64_t m = s_build.rowMask; m; m &= m - 1)
        s_build.uniqueRows++;

    if (s_haveCurr)
    {
        s_prev = s_curr;
        s_havePrev = true;
    }
    s_curr = s_build;
    s_haveCurr = true;
    Correlate();
    A3e_NoteObservedDraws(s_curr.drawCount);
    A3e_PlayerPalette_OnFrame(s_curr, s_havePrev ? &s_prev : nullptr);
    A3f_OnFrame(s_curr, s_havePrev ? &s_prev : nullptr);
    A3g_OnFrame(s_curr, s_havePrev ? &s_prev : nullptr);
    A3h_OnFrame(s_curr, nullptr);  // pulls A3g latest internally when needed
    {
        A3gPoseSnap pose{};
        const A3gPoseSnap* posePtr = A3g_GetLatest(pose) ? &pose : nullptr;
        A3hFrameSnap a3h{};
        const A3hFrameSnap* a3hPtr = A3h_GetLatest(a3h) ? &a3h : nullptr;
        A3i_OnFrame(s_curr, posePtr, a3hPtr);
    }
    DumpFrameLocked();
    ResetBuild();
}

void A3a_OnXfLoadIsolation(uint32_t addr, uint32_t count)
{
    if (!OvrOn() || s_ovrSuspended || !Render_ReplayIsolationActive()) return;
    const uint8_t row = A3a_OverrideRow();
    if (row == 0xFF) return;
    const uint32_t matBase = (uint32_t)row * 4u;
    const uint32_t matEnd = matBase + 12u;
    if (addr + count <= matBase || addr >= matEnd) return;
    s_ovr.xfHookCount++;
    A3a_ApplyMatrixOverride();
}

void A3a_SuspendOverride(bool suspend) { s_ovrSuspended = suspend; }
bool A3a_OverrideSuspended() { return s_ovrSuspended; }

bool A3a_ApplyMatrixOverride()
{
    if (!OvrOn() || s_ovrSuspended || !Render_ReplayIsolationActive()) return false;
    const uint8_t row = A3a_OverrideRow();
    if (row == 0xFF) return false;
    const uint32_t mode = ParseMode();
    if (mode == 0) return false;

    float m[12];
    ReadXfRowMtx(row, m);
    if (!Finite12(m)) return false;

    s_ovr.rowIdx = row;
    s_ovr.mode = mode;
    s_ovr.dx = ParseF("SCR_A3A_OVERRIDE_DX", 25.f);
    s_ovr.dy = ParseF("SCR_A3A_OVERRIDE_DY", 0.f);
    s_ovr.dz = ParseF("SCR_A3A_OVERRIDE_DZ", 0.f);

    if (mode == 2)
    {
        m[3] += s_ovr.dx;
        m[7] += s_ovr.dy;
        m[11] += s_ovr.dz;
    }
    // mode 1 (noop): write identical values — still exercises the path.
    WriteXfRowMtx(row, m);
    Render_ForceXfDirty();
    s_ovr.applyCount++;
    s_ovr.exercised = true;
    return true;
}

void A3a_ResetOverrideStats() { s_ovr = A3aOverrideStats{}; }

A3aOverrideStats A3a_GetOverrideStats() { return s_ovr; }

void A3a_NoteReplayHashes(uint32_t baseline, uint32_t overridden, bool noopMode)
{
    s_ovr.baselineHash = baseline;
    s_ovr.overriddenHash = overridden;
    if (noopMode)
        s_ovr.noopMatch = (baseline != 0 && baseline == overridden);
    else
        s_ovr.modifiedDiff = (baseline != 0 && overridden != 0 && baseline != overridden);
    LOG_INFO("A3a override result: mode={} row={} applies={} xfHooks={} base={:08x} ovr={:08x} "
             "noopMatch={} modDiff={} exercised={}",
             s_ovr.mode, s_ovr.rowIdx, s_ovr.applyCount, s_ovr.xfHookCount, baseline, overridden,
             s_ovr.noopMatch ? 1 : 0, s_ovr.modifiedDiff ? 1 : 0, s_ovr.exercised ? 1 : 0);
}

bool A3a_GetCurrent(A3aFrameSnap& out)
{
    std::lock_guard lk(s_mu);
    if (!s_haveCurr) return false;
    out = s_curr;
    return true;
}

bool A3a_GetPrevious(A3aFrameSnap& out)
{
    std::lock_guard lk(s_mu);
    if (!s_havePrev) return false;
    out = s_prev;
    return true;
}

void A3a_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || bufSize == 0) return;
    buf[0] = 0;
    if (!AttrOn() && !OvrOn()) return;
    A3aFrameSnap cur{};
    const bool ok = A3a_GetCurrent(cur);
    std::snprintf(buf, bufSize, "A3a draws=%u/%u rows=%u cand=%u corr=%u/%u ovr=%u/%u",
                  ok ? cur.drawStored : 0, ok ? cur.drawCount : 0, ok ? cur.uniqueRows : 0,
                  ok ? cur.playerCandidates : 0, ok ? cur.corrSame : 0, ok ? cur.corrAmbiguous : 0,
                  s_ovr.applyCount, s_ovr.rowIdx);
}
