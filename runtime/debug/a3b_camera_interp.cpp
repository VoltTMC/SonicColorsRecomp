#include "a3b_camera_interp.h"
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

std::atomic<int> s_env{ -1 };
bool s_suspended = false;
float s_probeAlpha = NAN;  // NaN → use env / default

bool EnvOn()
{
    int v = s_env.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_A3B_CAMERA_INTERP");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_env.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3b: camera-pose interpolation enabled (SCR_A3B_CAMERA_INTERP); "
                     "scratch-only, live XF untouched");
    }
    return v != 0;
}

float ParseAlpha()
{
    if (std::isfinite(s_probeAlpha))
    {
        float a = s_probeAlpha;
        if (a < 0.f) a = 0.f;
        if (a > 1.f) a = 1.f;
        return a;
    }
    const char* e = std::getenv("SCR_A3B_CAMERA_ALPHA");
    if (!e || !e[0]) return 0.5f;
    float a = (float)std::atof(e);
    if (a < 0.f) a = 0.f;
    if (a > 1.f) a = 1.f;
    return a;
}

std::mutex s_mu;
A3bCameraPose s_prev{}, s_curr{}, s_building{};
bool s_havePrev = false, s_haveCurr = false;

A3bMtx3x4 s_delta{};
bool s_deltaActive = false;
A3bCameraStats s_stats{};
uint64_t s_rowMask = 0;  // 0 = default PNMTX0..9
A3bCameraPair s_frozen{};
bool s_haveFrozen = false;

bool RowAllowed(uint32_t row)
{
    if (s_rowMask == 0)
        return (row % 3u) == 0u && row < 30u;  // GX_PNMTX0..9
    return row < 64u && (s_rowMask & (1ull << row)) != 0;
}

constexpr float kCutTrans = 25.f;     // metres-ish guest units
constexpr float kCutQuatDot = 0.5f;   // >60° roughly

void ReadXfMtx(uint32_t rowIdx, A3bMtx3x4& out)
{
    const uint32_t base = rowIdx * 4u;
    for (int i = 0; i < 12; i++)
    {
        union
        {
            uint32_t u;
            float f;
        } c;
        c.u = g_gx.xfmem[(base + (uint32_t)i) & 0xFFF];
        out.m[i] = c.f;
    }
}

void WriteXfMtx(uint32_t rowIdx, const A3bMtx3x4& in)
{
    const uint32_t base = rowIdx * 4u;
    for (int i = 0; i < 12; i++)
    {
        union
        {
            uint32_t u;
            float f;
        } c;
        c.f = in.m[i];
        g_gx.xfmem[(base + (uint32_t)i) & 0xFFF] = c.u;
    }
}

void FillFromA1b(A3bCameraPose& p, const A1bPoseSnap& s)
{
    p.simFrameId = s.simFrameId;
    p.xfbIndex = s.xfbIndex;
    p.seq = s.seq;
    p.cameraNodeEa = s.cameraNodeEa;
    p.cameraMgrEa = s.cameraMgrEa;
    p.cameraNodeValid = s.cameraNodeValid;
    p.cameraPos[0] = s.cameraPos[0];
    p.cameraPos[1] = s.cameraPos[1];
    p.cameraPos[2] = s.cameraPos[2];
    if (s.viewMtxValid)
    {
        std::memcpy(p.view.m, s.viewMtx.m, sizeof(p.view.m));
        p.viewValid = A3b_MtxFinite(p.view);
        p.viewHash = A3b_HashMtx(p.view);
    }
    else if (s.xfValid)
    {
        std::memcpy(p.view.m, s.gxPosMtx0.m, sizeof(p.view.m));
        p.viewValid = A3b_MtxFinite(p.view);
        p.viewHash = A3b_HashMtx(p.view);
    }
    if (s.xfValid)
    {
        std::memcpy(p.proj, s.xfProj, sizeof(p.proj));
        std::memcpy(p.viewport, s.xfViewport, sizeof(p.viewport));
        p.projValid = true;
        for (int i = 0; i < 7; i++)
            if (!std::isfinite(p.proj[i])) p.projValid = false;
        p.projHash = A3b_HashProj7(p.proj);
    }
}

uint32_t DetectFails(const A3bCameraPose& a, const A3bCameraPose& b)
{
    uint32_t f = 0;
    if (!a.viewValid || !b.viewValid || !a.projValid || !b.projValid)
        f |= A3bFail_Missing;
    if (!A3b_MtxFinite(a.view) || !A3b_MtxFinite(b.view))
        f |= A3bFail_NonFinite;
    if (a.projHash != b.projHash)
        f |= A3bFail_ProjChange;
    if (a.cameraNodeValid && b.cameraNodeValid && a.cameraNodeEa != b.cameraNodeEa)
        f |= A3bFail_CamChange | A3bFail_Cut;
    if (!(f & A3bFail_Missing))
    {
        A3bMtx3x4 camA{}, camB{};
        if (A3b_MtxInvertRigid(a.view, camA) && A3b_MtxInvertRigid(b.view, camB))
        {
            const float dx = camA.m[3] - camB.m[3];
            const float dy = camA.m[7] - camB.m[7];
            const float dz = camA.m[11] - camB.m[11];
            if (std::sqrt(dx * dx + dy * dy + dz * dz) > kCutTrans)
                f |= A3bFail_Cut;
            A3bRigid ra{}, rb{};
            if (A3b_Decompose(camA, ra) && A3b_Decompose(camB, rb))
            {
                float dot = A3b_QuatDot(ra.q, rb.q);
                if (dot < 0.f) dot = -dot;
                if (dot < kCutQuatDot)
                    f |= A3bFail_Cut;
            }
        }
    }
    // seq must be consecutive for sim-step pairing
    if (a.seq && b.seq && b.seq != a.seq + 1)
        f |= A3bFail_Cut;
    return f;
}

void ApplyDeltaToSelectedPosMtx()
{
    if (!s_deltaActive) return;
    if (s_rowMask == 0)
    {
        for (int slot = 0; slot < 10; slot++)
        {
            const uint32_t row = (uint32_t)slot * 3u;
            A3bMtx3x4 m{}, out{};
            ReadXfMtx(row, m);
            if (!A3b_MtxFinite(m)) continue;
            A3b_ApplyCameraDelta(s_delta, m, out);
            WriteXfMtx(row, out);
            s_stats.rowsTouched++;
        }
    }
    else
    {
        for (uint32_t row = 0; row < 64; row++)
        {
            if (!RowAllowed(row)) continue;
            A3bMtx3x4 m{}, out{};
            ReadXfMtx(row, m);
            if (!A3b_MtxFinite(m)) continue;
            A3b_ApplyCameraDelta(s_delta, m, out);
            WriteXfMtx(row, out);
            s_stats.rowsTouched++;
        }
    }
    s_stats.applyCount++;
    s_stats.exercised = true;
    Render_ForceXfDirty();
}

}  // namespace

bool A3b_CameraInterpEnabled() { return EnvOn(); }

float A3b_CameraAlpha() { return ParseAlpha(); }

void A3b_SetProbeAlpha(float alphaOrNan) { s_probeAlpha = alphaOrNan; }

void A3b_SuspendCamera(bool suspend) { s_suspended = suspend; }
bool A3b_CameraSuspended() { return s_suspended; }

void A3b_OnSimFrameEnd()
{
    if (!EnvOn()) return;
    // Prefer A1b when enabled; otherwise leave building for XFB fill.
    if (!A1b_PoseCaptureEnabled()) return;
    A1bPoseSnap s{};
    if (!A1b_GetCurrent(s)) return;
    std::lock_guard lk(s_mu);
    FillFromA1b(s_building, s);
}

void A3b_OnXfbCopy()
{
    if (!EnvOn()) return;
    A1bPoseSnap s{};
    const bool haveA1b = A1b_PoseCaptureEnabled() && A1b_GetCurrent(s);

    std::lock_guard lk(s_mu);
    if (haveA1b)
        FillFromA1b(s_building, s);
    // Ensure proj from live XF if A1b xf not ready.
    if (!s_building.projValid)
    {
        for (int i = 0; i < 7; i++)
            s_building.proj[i] = XFFloat(0x1000u + XF_PROJECTION + (uint32_t)i);
        for (int i = 0; i < 6; i++)
            s_building.viewport[i] = XFFloat(0x1000u + XF_VIEWPORT + (uint32_t)i);
        s_building.projValid = true;
        for (int i = 0; i < 7; i++)
            if (!std::isfinite(s_building.proj[i])) s_building.projValid = false;
        s_building.projHash = A3b_HashProj7(s_building.proj);
    }
    if (!s_building.viewValid)
    {
        ReadXfMtx(0, s_building.view);
        s_building.viewValid = A3b_MtxFinite(s_building.view);
        s_building.viewHash = A3b_HashMtx(s_building.view);
    }
    s_building.xfbIndex = Perf_XfbFrameIndex();
    if (!s_building.simFrameId)
        s_building.simFrameId = Perf_GuestFrameIndex();

    if (s_haveCurr)
    {
        s_prev = s_curr;
        s_havePrev = true;
    }
    s_curr = s_building;
    s_haveCurr = true;
    s_building = A3bCameraPose{};

    static int dumps = 0;
    static float lastTx = 1e30f, lastTy = 1e30f, lastTz = 1e30f;
    if (s_havePrev && dumps < 64)
    {
        const float tx = s_curr.view.m[3], ty = s_curr.view.m[7], tz = s_curr.view.m[11];
        const float dT = std::fabs(tx - lastTx) + std::fabs(ty - lastTy) + std::fabs(tz - lastTz);
        // First few + any ≥0.5 unit view-T move (ignore float chatter).
        if (dumps < 6 || dT >= 0.5f)
        {
            const uint32_t fail = DetectFails(s_prev, s_curr);
            LOG_INFO("A3b cam: xfb={}←{} seq={}←{} viewOk={}/{} proj={} fail={:03x} "
                     "viewT=({:.2f},{:.2f},{:.2f}) camPos=({:.2f},{:.2f},{:.2f})",
                     s_curr.xfbIndex, s_prev.xfbIndex, s_curr.seq, s_prev.seq,
                     s_curr.viewValid ? 1 : 0, s_prev.viewValid ? 1 : 0,
                     s_curr.projHash == s_prev.projHash ? 1 : 0, fail, tx, ty, tz,
                     s_curr.cameraPos[0], s_curr.cameraPos[1], s_curr.cameraPos[2]);
            lastTx = tx;
            lastTy = ty;
            lastTz = tz;
            dumps++;
        }
    }
}

bool A3b_GetPair(A3bCameraPair& out)
{
    if (s_haveFrozen)
    {
        out = s_frozen;
        return out.havePair;
    }
    std::lock_guard lk(s_mu);
    out = A3bCameraPair{};
    if (!s_havePrev || !s_haveCurr) return false;
    out.prev = s_prev;
    out.curr = s_curr;
    out.havePair = true;
    out.failBits = DetectFails(s_prev, s_curr);
    out.interpolable = A3b_PairInterpolable(out);
    return true;
}

void A3b_FreezePair(const A3bCameraPair& p)
{
    s_frozen = p;
    s_haveFrozen = p.havePair;
}

void A3b_ClearFrozenPair()
{
    s_haveFrozen = false;
    s_frozen = A3bCameraPair{};
}

bool A3b_HasFrozenPair() { return s_haveFrozen; }

bool A3b_PairInterpolable(const A3bCameraPair& p)
{
    if (!p.havePair) return false;
    if (p.failBits & (A3bFail_Missing | A3bFail_NonFinite | A3bFail_Cut | A3bFail_ProjChange |
                      A3bFail_CamChange | A3bFail_Singular | A3bFail_Shear |
                      A3bFail_Reflection))
        return false;
    return p.prev.viewValid && p.curr.viewValid;
}

bool A3b_BeginScratchCamera(float alpha, bool baseIsPrev)
{
    s_deltaActive = false;
    if (!EnvOn() || s_suspended || !Render_ReplayIsolationActive()) return false;

    A3bCameraPair pair{};
    if (!A3b_GetPair(pair) || !pair.interpolable)
    {
        s_stats.failBits = pair.failBits | A3bFail_Missing;
        return false;
    }

    A3bMtx3x4 view0 = pair.prev.view;
    A3bMtx3x4 view1 = pair.curr.view;
    const A3bMtx3x4& viewBase = baseIsPrev ? view0 : view1;
    // Diagnostic: when live views are identical (static UI), synthesize a camera
    // world +X translation so midpoint application can be proven on a MATCH archive.
    // SCR_A3B_FORCE_VIEW_DX=<metres> (default 0 = off). Scratch-only.
    {
        const char* fdx = std::getenv("SCR_A3B_FORCE_VIEW_DX");
        const float dx = (fdx && fdx[0]) ? (float)std::atof(fdx) : 0.f;
        if (std::fabs(dx) > 1e-4f && A3b_MtxNearEqual(view0, view1, 1e-4f))
        {
            A3bMtx3x4 cam{};
            if (A3b_MtxInvertRigid(view0, cam))
            {
                cam.m[3] += dx;
                A3bMtx3x4 syn{};
                if (A3b_MtxInvertRigid(cam, syn))
                    view1 = syn;
            }
        }
    }

    A3bMtx3x4 viewA{};
    uint32_t fail = 0;
    if (!A3b_InterpolateView(view0, view1, alpha, viewA, &fail))
    {
        s_stats.failBits = fail;
        return false;
    }
    A3bMtx3x4 delta{};
    // D = Vα · inv(Vbase). Identity when Vα ≈ Vbase (α=0+basePrev or α=1+baseCurr).
    if (A3b_MtxNearEqual(viewBase, viewA, 1e-5f))
        A3b_MtxIdentity(delta);
    else if (!A3b_CameraDelta(viewBase, viewA, delta))
    {
        s_stats.failBits = A3bFail_Singular;
        return false;
    }

    s_delta = delta;
    s_deltaActive = true;
    s_stats.alpha = alpha;
    s_stats.view0Hash = A3b_HashMtx(view0);
    s_stats.view1Hash = A3b_HashMtx(view1);
    s_stats.viewAHash = A3b_HashMtx(viewA);
    s_stats.deltaHash = A3b_HashMtx(delta);
    s_stats.failBits = 0;
    s_stats.rowsTouched = 0;

    A3bMtx3x4 id{};
    A3b_MtxIdentity(id);
    if (A3b_MtxNearEqual(delta, id, 1e-3f))
        s_stats.endpointMatch = true;

    ApplyDeltaToSelectedPosMtx();
    return true;
}

void A3b_SetRowMask(uint64_t mask) { s_rowMask = mask; }
void A3b_ClearRowMask() { s_rowMask = 0; }
uint64_t A3b_GetRowMask() { return s_rowMask; }

void A3b_OnXfLoadIsolation(uint32_t addr, uint32_t count)
{
    if (!s_deltaActive || !EnvOn() || s_suspended || !Render_ReplayIsolationActive()) return;
    // Full pos-mtx writes only (12 floats, 12-aligned → PNMTXIDX = addr/4).
    if (count < 12 || (addr % 12) != 0 || addr >= 256) return;
    const uint32_t row = addr / 4u;
    if (!RowAllowed(row)) return;
    A3bMtx3x4 m{}, out{};
    ReadXfMtx(row, m);
    if (!A3b_MtxFinite(m)) return;
    A3b_ApplyCameraDelta(s_delta, m, out);
    WriteXfMtx(row, out);
    s_stats.xfHookCount++;
    s_stats.rowsTouched++;
    s_stats.exercised = true;
    Render_ForceXfDirty();
}

void A3b_EndScratchCamera() { s_deltaActive = false; }

bool A3b_ScratchCameraActive() { return s_deltaActive; }

void A3b_ResetCameraStats()
{
    const float a = s_stats.alpha;
    s_stats = A3bCameraStats{};
    s_stats.alpha = a;
}

A3bCameraStats A3b_GetCameraStats() { return s_stats; }

void A3b_NoteCameraHashes(uint32_t baseline, uint32_t result)
{
    s_stats.baselineHash = baseline;
    s_stats.resultHash = result;
    if (s_stats.endpointMatch || s_stats.alpha <= 1e-4f)
        s_stats.endpointMatch = (baseline != 0 && baseline == result);
    if (s_stats.alpha > 0.2f && s_stats.alpha < 0.8f)
        s_stats.midpointDistinct = (baseline != 0 && result != 0 && baseline != result);
    LOG_INFO("A3b camera result: α={:.3f} applies={} rows={} xfHooks={} "
             "v0={:08x} v1={:08x} vA={:08x} d={:08x} base={:08x} res={:08x} "
             "endMatch={} midDiff={} fail={:03x} exercised={}",
             s_stats.alpha, s_stats.applyCount, s_stats.rowsTouched, s_stats.xfHookCount,
             s_stats.view0Hash, s_stats.view1Hash, s_stats.viewAHash, s_stats.deltaHash,
             baseline, result, s_stats.endpointMatch ? 1 : 0, s_stats.midpointDistinct ? 1 : 0,
             s_stats.failBits, s_stats.exercised ? 1 : 0);
}

void A3b_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!EnvOn()) return;
    A3bCameraPair p{};
    A3b_GetPair(p);
    std::snprintf(buf, bufSize, "A3b cam pair=%d ok=%d α=%.2f fail=%03x ovr=%u",
                  p.havePair ? 1 : 0, p.interpolable ? 1 : 0, ParseAlpha(), p.failBits,
                  s_stats.applyCount);
}
