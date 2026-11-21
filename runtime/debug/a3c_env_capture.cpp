#include "a3c_env_capture.h"
#include "a2b_fifo_archive.h"
#include "../log.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_cap{ -1 };
std::atomic<int> s_interp{ -1 };
A3cEnvStats s_stats{};
A3cCoverageReport s_cov{};
std::mutex s_mu;
uint32_t s_probeUsed = 0;

bool CapOn()
{
    int v = s_cap.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_A3C_ENV_CAPTURE");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_cap.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3c: env capture enabled (SCR_A3C_ENV_CAPTURE); coherent TR1 subset");
    }
    return v != 0;
}

bool InterpOn()
{
    int v = s_interp.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_A3C_ENV_INTERP");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_interp.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3c: env camera interp enabled (SCR_A3C_ENV_INTERP); natural poses only");
    }
    return v != 0;
}

}  // namespace

bool A3c_EnvCaptureEnabled() { return CapOn(); }
bool A3c_EnvInterpEnabled() { return InterpOn(); }

A3cDrawKind A3c_ClassifyDraw(const A3aDrawRec& d)
{
    // Title/HUD quads: vat-heavy, tiny vtx, not in DL, single default row.
    if (!d.inDL && d.mtxCount <= 1 && d.vtxCount <= 6 && d.vat >= 5)
        return A3cKind_Ui;
    if (d.flags & 1)  // playerCandidate
        return A3cKind_Skinned;
    if (d.posMtxDirect && d.mtxCount >= 2)
        return A3cKind_Skinned;
    // Static/world: single matrix, often in DL, or larger non-UI draws.
    if (d.mtxCount == 1 && (d.inDL || d.vtxCount >= 8 || d.prim == 2 || d.prim == 3))
        return A3cKind_Env;
    if (d.mtxCount == 1)
        return A3cKind_Env;
    return A3cKind_Other;
}

void A3c_OnXfbCopy()
{
    if (!CapOn() && !InterpOn()) return;
    if (!A3a_DrawAttrEnabled()) return;

    A3aFrameSnap cur{}, prev{};
    const bool haveCur = A3a_GetCurrent(cur);
    const bool havePrev = A3a_GetPrevious(prev);
    if (!haveCur) return;

    A3cEnvStats st{};
    st.totalDraws = cur.drawStored;
    uint64_t mask = 0;
    uint32_t envRows = 0;
    for (uint32_t i = 0; i < cur.drawStored; i++)
    {
        const A3aDrawRec& d = cur.draws[i];
        const A3cDrawKind k = A3c_ClassifyDraw(d);
        if (k == A3cKind_Ui)
            st.uiDraws++;
        else if (k == A3cKind_Skinned)
            st.skinnedDraws++;
        else if (k == A3cKind_Env)
        {
            st.envDraws++;
            if (d.corrKind == A3aCorr_SameObject) st.envCorrSame++;
            else if (d.corrKind == A3aCorr_SameStructure) st.envCorrStruct++;
            else if (d.corrKind == A3aCorr_Ambiguous) st.envCorrAmb++;
            else if (d.corrKind == A3aCorr_Appeared) st.envCorrNew++;
            for (uint16_t m = 0; m < d.mtxCount; m++)
            {
                const uint8_t row = d.mtx[m].rowIdx;
                if (row < 64 && !(mask & (1ull << row)))
                {
                    mask |= (1ull << row);
                    envRows++;
                }
            }
            // Geom/resource identity vs previous frame (not camera matrices).
            if (havePrev && d.identityKey != 0)
            {
                bool found = false;
                bool sameGeom = false;
                for (uint32_t j = 0; j < prev.drawStored; j++)
                {
                    const A3aDrawRec& p = prev.draws[j];
                    if (A3c_ClassifyDraw(p) != A3cKind_Env) continue;
                    if (p.identityKey != d.identityKey) continue;
                    found = true;
                    sameGeom = (p.geomSig == d.geomSig && p.vtxCount == d.vtxCount &&
                                p.prim == d.prim && p.arrayBase0 == d.arrayBase0);
                    break;
                }
                if (!found)
                    st.envGeomMissingPrev++;
                else if (sameGeom)
                    st.envGeomStable++;
                else
                    st.envGeomChanged++;
            }
        }
        else
            st.otherDraws++;
    }
    st.envRowMask = mask;
    st.envRows = envRows;
    const uint32_t geomChecked = st.envGeomStable + st.envGeomChanged + st.envGeomMissingPrev;
    st.envGeomUnchanged =
        havePrev && geomChecked > 0 && st.envGeomChanged == 0 &&
        st.envGeomMissingPrev * 4 <= geomChecked;  // allow ≤25% newly appeared

    // Correspondence quality among env draws that existed previously.
    const uint32_t envStable = st.envCorrSame + st.envCorrStruct;
    const uint32_t envKnown = envStable + st.envCorrAmb + st.envCorrNew;
    if (st.envDraws == 0)
        st.failBits |= A3cFail_NoEnv;
    if (havePrev && envKnown > 0 && envStable * 4 < envKnown * 3)  // <75% stable
        st.failBits |= A3cFail_CorrWeak;
    if (havePrev && st.envGeomChanged > 0)
        st.failBits |= A3cFail_GeomChanged;

    A3bCameraPair cam{};
    if (A3b_GetPair(cam) && cam.interpolable)
    {
        st.naturalCam = (cam.prev.viewHash != cam.curr.viewHash);
    }
    else
        st.failBits |= A3cFail_CamInvalid;

    st.captureReady = (st.envDraws >= 8 && st.envRows >= 1 && !(st.failBits & A3cFail_NoEnv));
    st.interpReady = st.captureReady && !(st.failBits & (A3cFail_CorrWeak | A3cFail_CamInvalid)) &&
                     cam.interpolable;

    {
        std::lock_guard lk(s_mu);
        // Preserve latest pass metadata; archive fail bits come only from NotePass.
        const uint32_t archFail =
            s_stats.failBits & (A3cFail_Incomplete | A3cFail_Unresolved | A3cFail_Overflow);
        st.passBytes = s_stats.passBytes;
        st.depCount = s_stats.depCount;
        st.failBits |= archFail;
        if (st.failBits & (A3cFail_Incomplete | A3cFail_Unresolved | A3cFail_Overflow))
        {
            st.captureReady = false;
            st.interpReady = false;
        }
        s_stats = st;
        s_cov.selectedEnv = st.envDraws;
        s_cov.capturedDraws = st.totalDraws;
        s_cov.skippedNonEnv = st.uiDraws + st.skinnedDraws + st.otherDraws;
        s_cov.corrSame = st.envCorrSame;
        s_cov.corrStruct = st.envCorrStruct;
        s_cov.corrWeak = st.envCorrAmb + st.envCorrNew;
        s_cov.geomStable = st.envGeomStable;
        s_cov.geomChanged = st.envGeomChanged;
        s_cov.envRows = st.envRows;
        s_cov.envRowMask = st.envRowMask;
        s_cov.passBytes = st.passBytes;
        s_cov.deps = st.depCount;
        s_cov.failBits = st.failBits;
    }

    static int dumps = 0;
    if (dumps < 16 && (st.envDraws >= 8 || st.interpReady || (cur.xfbIndex % 400) == 0))
    {
        LOG_INFO("A3c env: xfb={} draws={}/{} env={} ui={} skin={} rows={} mask={:016x} "
                 "corr s/st/a/n={}/{}/{}/{} geom ok/ch/miss={}/{}/{} cam={} ready={}/{} fail={:03x}",
                 cur.xfbIndex, cur.drawStored, cur.drawCount, st.envDraws, st.uiDraws,
                 st.skinnedDraws, st.envRows, (unsigned long long)st.envRowMask, st.envCorrSame,
                 st.envCorrStruct, st.envCorrAmb, st.envCorrNew, st.envGeomStable,
                 st.envGeomChanged, st.envGeomMissingPrev, st.naturalCam ? 1 : 0,
                 st.captureReady ? 1 : 0, st.interpReady ? 1 : 0, st.failBits);
        dumps++;
    }
}

void A3c_NotePass(uint32_t passBytes, uint32_t depCount, uint32_t unresolved, uint32_t flags,
                  bool replayReady)
{
    if (!CapOn() && !InterpOn()) return;
    std::lock_guard lk(s_mu);
    s_stats.passBytes = passBytes;
    s_stats.depCount = depCount;
    // Replace archive fail bits each evaluate (do not sticky-OR across frames).
    s_stats.failBits &= ~(A3cFail_Incomplete | A3cFail_Unresolved | A3cFail_Overflow);
    if (flags & A2bFrame_Incomplete)
        s_stats.failBits |= A3cFail_Incomplete | A3cFail_Overflow;
    if (unresolved)
        s_stats.failBits |= A3cFail_Unresolved;
    if (!replayReady && (flags & A2bFrame_Incomplete))
        s_stats.captureReady = false;
    (void)replayReady;
}

A3cEnvStats A3c_GetStats()
{
    std::lock_guard lk(s_mu);
    return s_stats;
}

A3cCoverageReport A3c_GetCoverage()
{
    std::lock_guard lk(s_mu);
    return s_cov;
}

uint64_t A3c_EnvRowMask()
{
    std::lock_guard lk(s_mu);
    return s_stats.envRowMask;
}

bool A3c_InterpReady()
{
    std::lock_guard lk(s_mu);
    return s_stats.interpReady;
}

bool A3c_EnvGeomUnchanged()
{
    std::lock_guard lk(s_mu);
    return s_stats.envGeomUnchanged;
}

void A3c_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!CapOn() && !InterpOn()) return;
    A3cEnvStats st = A3c_GetStats();
    std::snprintf(buf, bufSize, "A3c env=%u rows=%u ready=%d/%d nat=%d geom=%d fail=%03x",
                  st.envDraws, st.envRows, st.captureReady ? 1 : 0, st.interpReady ? 1 : 0,
                  st.naturalCam ? 1 : 0, st.envGeomUnchanged ? 1 : 0, st.failBits);
}

void A3c_LogCoverage(uint64_t archiveXfb)
{
    A3cCoverageReport c = A3c_GetCoverage();
    LOG_INFO("A3c coverage: archive_frame={} selected={} captured={} skipped={} "
             "corr same/struct/weak={}/{}/{} geom ok/ch={}/{} rows={} mask={:016x} "
             "passBytes={} deps={} fail={:03x}",
             archiveXfb, c.selectedEnv, c.capturedDraws, c.skippedNonEnv, c.corrSame, c.corrStruct,
             c.corrWeak, c.geomStable, c.geomChanged, c.envRows, (unsigned long long)c.envRowMask,
             c.passBytes, c.deps, c.failBits);
}

bool A3c_NaturalInterpEligible()
{
    if (!InterpOn()) return false;
    const char* fdx = std::getenv("SCR_A3B_FORCE_VIEW_DX");
    if (fdx && fdx[0] && fdx[0] != '0') return false;
    // Env classification (ignore sticky archive overflow from other frames).
    A3cEnvStats st = A3c_GetStats();
    if (st.envDraws < 8 || st.envRows < 1) return false;
    // CorrWeak often reflects HUD/actor churn around the env subset. Prefer
    // geomUnchanged (identityKey+geomSig) as the camera-only safety signal.
    if ((st.failBits & A3cFail_CorrWeak) && !st.envGeomUnchanged) return false;
    A3bCameraPair pair{};
    if (!A3b_GetPair(pair) || !pair.interpolable) return false;
    if (pair.prev.viewHash == pair.curr.viewHash) return false;
    // Require meaningful camera motion so midpoint is observable in EFB hash.
    A3bMtx3x4 cam0{}, cam1{};
    if (!A3b_MtxInvertRigid(pair.prev.view, cam0) || !A3b_MtxInvertRigid(pair.curr.view, cam1))
        return false;
    const float dx = cam0.m[3] - cam1.m[3];
    const float dy = cam0.m[7] - cam1.m[7];
    const float dz = cam0.m[11] - cam1.m[11];
    return std::sqrt(dx * dx + dy * dy + dz * dz) >= 0.5f;
}

bool A3c_TakeProbeSlot()
{
    uint32_t limit = 4;
    if (const char* e = std::getenv("SCR_A3C_PROBE_LIMIT"))
    {
        if (e[0]) limit = (uint32_t)std::atoi(e);
    }
    if (limit == 0) return false;
    if (s_probeUsed >= limit) return false;
    s_probeUsed++;
    return true;
}

uint32_t A3c_ProbeSlotsUsed() { return s_probeUsed; }
