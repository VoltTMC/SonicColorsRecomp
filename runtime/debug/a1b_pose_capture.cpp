// Approach A1b — capture Sonic world pose + camera node + XF / view state.
//
// Sources (decomp-verified, see docs/FRAME_RATE.md A1b):
//   Player shell: cached from PlayerGetWorldMtxPtr (fn_80033C78) r3, and/or
//                 Read32(PlayerSingletonPtr / 0x808F3378) when non-null.
//   World matrix: shell → +0x28 → +0x84 (body) → +0x0C  (fn_80033C78 / fn_80053718)
//   Position:     body + 0x168 (fn_80033C34)
//   Camera mgr:   CameraManagerTick (fn_80571720) r3=this
//   Camera node:  mgr+0xBC → +0x20 → +0xC068 (same path Tick uses for r27), else
//                 first list entry at mgr+0xC0 (fn_805C1540)
//   Cam pos:      node+0x90 (fn_80570FF0 dest); alt node+0xA8 (Tick reads)
//   Cam local:    node+0x60 (12 floats sourced in fn_80570FF0)
//   View mtx:     first XF LoadXF into pos mtx 0 each GPU frame (Render_OnXF)
//   Projection:   g_gx XF_PROJECTION / XF_VIEWPORT at XFB (same as A1)
//
// SCR_A1B_POSE_CAPTURE=1 — disabled by default. Read-only; no guest writes.
#include "a1b_pose_capture.h"
#include "a1_xf_capture.h"
#include "a3f_player_provenance.h"
#include "../game_addrs.h"
#include "../gx/gx_state.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../perf.h"

#include "../platform/platform.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {

constexpr uint32_t kBodyMtxOff = 0x0C;
constexpr uint32_t kBodyPosOff = 0x168;
constexpr uint32_t kBodyDirtyOff = 0x2DD8;

constexpr uint32_t kCamMgrListOff = 0xC0;   // fn_80570C40 / fn_805C1320
constexpr uint32_t kCamMgrBcOff = 0xBC;     // CameraManagerTick r27 path
constexpr uint32_t kCamNodeLocalOff = 0x60;
constexpr uint32_t kCamNodePosOff = 0x90;
constexpr uint32_t kCamNodePosAltOff = 0xA8;

bool EnvOn()
{
    static int c = -1;
    if (c < 0)
    {
        const char* v = std::getenv("SCR_A1B_POSE_CAPTURE");
        c = (v && v[0] && v[0] != '0') ? 1 : 0;
        // A3f/A3g ownership/palette need the same shell→body identity sources.
        if (!c)
        {
            const char* t = std::getenv("SCR_A3F_PLAYER_TRACE");
            const char* p = std::getenv("SCR_A3F_PALETTE_CAPTURE");
            const char* g = std::getenv("SCR_A3G_POSE_AGGREGATE");
            const char* h = std::getenv("SCR_A3G_HIERARCHY_TRACE");
            const char* h1 = std::getenv("SCR_A3H_SKELETON_TRACE");
            const char* h2 = std::getenv("SCR_A3H_PALETTE_MAP");
            const char* i1 = std::getenv("SCR_A3I_MODEL_OWNERSHIP");
            const char* i2 = std::getenv("SCR_A3I_CALC_WORLD");
            const char* i3 = std::getenv("SCR_A3I_CALC_VIEW");
            const char* i4 = std::getenv("SCR_A3I_PALETTE_PROVENANCE");
            const char* pc = std::getenv("SCR_POSE_CAPTURE");
            if ((t && t[0] && t[0] != '0') || (p && p[0] && p[0] != '0') ||
                (g && g[0] && g[0] != '0') || (h && h[0] && h[0] != '0') ||
                (h1 && h1[0] && h1[0] != '0') || (h2 && h2[0] && h2[0] != '0') ||
                (i1 && i1[0] && i1[0] != '0') || (i2 && i2[0] && i2[0] != '0') ||
                (i3 && i3[0] && i3[0] != '0') || (i4 && i4[0] && i4[0] != '0') ||
                (pc && pc[0] && pc[0] != '0'))
                c = 1;
        }
        if (c)
            LOG_INFO("A1b: pose capture enabled (SCR_A1B_POSE_CAPTURE or A3f/A3g/A3h/A3i/"
                     "SCR_POSE_CAPTURE identity); no extra unique XFB");
    }
    return c != 0;
}

bool GuestPtrOk(uint32_t ea)
{
    // Wii cached MEM1 (0x8…) and MEM2 (0x9…); player singleton lives in MEM2.
    if (!ea) return false;
    const uint32_t hi = ea >> 28;
    return hi == 0x8 || hi == 0x9;
}

float ReadF32(uint32_t addr)
{
    uint32_t u = Read32(addr);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

void ReadMtx3x4(uint32_t ea, A1bMtx3x4& out)
{
    for (int i = 0; i < 12; i++)
        out.m[i] = ReadF32(ea + (uint32_t)(i * 4));
}

void ReadXfProj(float out[7])
{
    for (int i = 0; i < 7; i++)
        out[i] = XFFloat(0x1000u + XF_PROJECTION + (uint32_t)i);
}

void ReadXfViewport(float out[6])
{
    for (int i = 0; i < 6; i++)
        out[i] = XFFloat(0x1000u + XF_VIEWPORT + (uint32_t)i);
}

void ReadGxPosMtx0(A1bMtx3x4& out)
{
    for (int i = 0; i < 12; i++)
    {
        union { uint32_t u; float f; } c;
        c.u = g_gx.xfmem[i];
        out.m[i] = c.f;
    }
}

bool FiniteMtx(const A1bMtx3x4& m)
{
    for (int i = 0; i < 12; i++)
        if (!std::isfinite(m.m[i])) return false;
    return true;
}

bool Finite3(const float* v)
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

std::atomic<uint32_t> s_cachedPlayerShell{ 0 };
std::atomic<uint32_t> s_cachedCameraMgr{ 0 };
std::atomic<uint64_t> s_hookPlayer{ 0 };
std::atomic<uint64_t> s_hookCamera{ 0 };

std::mutex s_mu;
A1bPoseSnap s_prev{}, s_curr{};
bool s_havePrev = false, s_haveCurr = false;
std::atomic<uint64_t> s_seq{ 0 };

// GPU-thread: first pos-mtx-0 of the current XFB frame
std::mutex s_viewMu;
A1bMtx3x4 s_viewMtx{};
bool s_viewValid = false;
std::atomic<bool> s_viewCapturedThisXfb{ false };

bool BodyFromShell(uint32_t shell, uint32_t& bodyOut)
{
    if (!GuestPtrOk(shell)) return false;
    const uint32_t inner = Read32(shell + 0x28);
    if (!GuestPtrOk(inner)) return false;
    const uint32_t body = Read32(inner + 0x84);
    if (!GuestPtrOk(body)) return false;
    bodyOut = body;
    return true;
}

bool ResolvePlayer(uint32_t& shellOut, uint32_t& bodyOut)
{
    // Prefer shells observed from GetPos / GetWorldMtxPtr / BoostPlugin (verified layouts).
    // Do NOT treat PlayerSingletonPtr (type 0x88C0) as a shell — +0x28 is not that chain.
    uint32_t shell = s_cachedPlayerShell.load(std::memory_order_relaxed);
    uint32_t body = 0;
    if (BodyFromShell(shell, body))
    {
        shellOut = shell;
        bodyOut = body;
        return true;
    }
    return false;
}

void FillPlayer(A1bPoseSnap& s)
{
    s.playerValid = false;
    uint32_t shell = 0, body = 0;
    if (!ResolvePlayer(shell, body)) return;
    s.playerShellEa = shell;
    s.playerBodyEa = body;
    s.playerDirtyFlags = Read32(body + kBodyDirtyOff);
    ReadMtx3x4(body + kBodyMtxOff, s.playerWorld);
    s.playerPos[0] = ReadF32(body + kBodyPosOff + 0);
    s.playerPos[1] = ReadF32(body + kBodyPosOff + 4);
    s.playerPos[2] = ReadF32(body + kBodyPosOff + 8);
    s.playerValid = FiniteMtx(s.playerWorld) && Finite3(s.playerPos);
}

uint32_t ResolveCameraNode(uint32_t mgr)
{
    // Path A — same as CameraManagerTick r27 setup:
    //   node = *(*(mgr+0xBC)+0x20 + 0xC068)
    const uint32_t bc = Read32(mgr + kCamMgrBcOff);
    if (GuestPtrOk(bc))
    {
        const uint32_t mid = Read32(bc + 0x20);
        if (GuestPtrOk(mid))
        {
            const uint32_t node = Read32(mid + 0xC068);
            if (GuestPtrOk(node)) return node;
        }
    }
    // Path B — first NW4R list entry at mgr+0xC0 (fn_805C1540 with r4=0)
    const uint32_t first = Read32(mgr + kCamMgrListOff);
    if (GuestPtrOk(first)) return first;
    return 0;
}

void FillCamera(A1bPoseSnap& s)
{
    const uint32_t mgr = s_cachedCameraMgr.load(std::memory_order_relaxed);
    s.cameraMgrValid = GuestPtrOk(mgr);
    s.cameraMgrEa = s.cameraMgrValid ? mgr : 0;
    s.cameraNodeValid = false;
    if (!s.cameraMgrValid) return;

    const uint32_t node = ResolveCameraNode(mgr);
    if (!GuestPtrOk(node)) return;
    s.cameraNodeEa = node;
    s.cameraPos[0] = ReadF32(node + kCamNodePosOff + 0);
    s.cameraPos[1] = ReadF32(node + kCamNodePosOff + 4);
    s.cameraPos[2] = ReadF32(node + kCamNodePosOff + 8);
    s.cameraPosAlt[0] = ReadF32(node + kCamNodePosAltOff + 0);
    s.cameraPosAlt[1] = ReadF32(node + kCamNodePosAltOff + 4);
    s.cameraPosAlt[2] = ReadF32(node + kCamNodePosAltOff + 8);
    A1bMtx3x4 local{};
    ReadMtx3x4(node + kCamNodeLocalOff, local);
    if (FiniteMtx(local))
        s.cameraLocal = local;
    // Pointer + finite position fields are enough; +0x60 may be uninitialized off-stage.
    s.cameraNodeValid = GuestPtrOk(node) &&
                        (Finite3(s.cameraPos) || Finite3(s.cameraPosAlt));
}

void FillViewFromGpu(A1bPoseSnap& s)
{
    std::lock_guard lk(s_viewMu);
    s.viewMtxValid = s_viewValid && FiniteMtx(s_viewMtx);
    if (s.viewMtxValid)
        s.viewMtx = s_viewMtx;
}

void MaybeDump(const A1bPoseSnap& snap, bool force)
{
    static int dumpOn = -1;
    if (dumpOn < 0)
    {
        const char* d = std::getenv("SCR_A1B_POSE_DUMP");
        dumpOn = (d && d[0] && d[0] != '0') ? 1 : 0;
    }
    if (!dumpOn) return;

    static uint64_t lastDumpMs = 0;
    static bool everPlayer = false;
    const uint64_t now = Platform_TickMs();
    const bool playerEdge = snap.playerValid && !everPlayer;
    if (snap.playerValid) everPlayer = true;
    if (!force && !playerEdge && now - lastDumpMs < 2000) return;
    lastDumpMs = now;

    LOG_INFO("A1b dump: seq={} sim={} player={} shell={:08x} body={:08x} "
             "pos=({:.2f},{:.2f},{:.2f}) mtxT=({:.2f},{:.2f},{:.2f}) dirty={:08x} "
             "hooks ply={} cam={} camMgr={} ({:08x}) camNode={} ({:08x}) "
             "camPos=({:.2f},{:.2f},{:.2f}) view={} viewT=({:.2f},{:.2f},{:.2f}) xf={}",
             snap.seq, snap.simFrameId,
             snap.playerValid ? "ok" : "no",
             snap.playerShellEa, snap.playerBodyEa,
             snap.playerPos[0], snap.playerPos[1], snap.playerPos[2],
             snap.playerWorld.m[3], snap.playerWorld.m[7], snap.playerWorld.m[11],
             snap.playerDirtyFlags,
             s_hookPlayer.load(std::memory_order_relaxed),
             s_hookCamera.load(std::memory_order_relaxed),
             snap.cameraMgrValid ? "ok" : "no", snap.cameraMgrEa,
             snap.cameraNodeValid ? "ok" : "no", snap.cameraNodeEa,
             snap.cameraPos[0], snap.cameraPos[1], snap.cameraPos[2],
             snap.viewMtxValid ? "ok" : "no",
             snap.viewMtx.m[3], snap.viewMtx.m[7], snap.viewMtx.m[11],
             snap.xfValid ? "ok" : "pending");
}

}  // namespace

bool A1b_PoseCaptureEnabled() { return EnvOn(); }

void A1b_OnSimFrameEnd()
{
    if (!EnvOn()) return;

    A1bPoseSnap snap{};
    snap.seq = s_seq.load(std::memory_order_relaxed) + 1;
    snap.simFrameId = Perf_GuestFrameIndex();
    FillPlayer(snap);
    FillCamera(snap);
    FillViewFromGpu(snap);
    // XF filled later on XFB thread for this same curr slot.

    std::lock_guard lk(s_mu);
    if (s_haveCurr)
    {
        s_prev = s_curr;
        s_havePrev = true;
    }
    s_curr = snap;
    s_haveCurr = true;
    s_seq.store(snap.seq, std::memory_order_relaxed);
    MaybeDump(snap, false);
}

void A1b_OnXfbCopy()
{
    if (!EnvOn()) return;

    // Next GPU frame may capture a fresh first pos-mtx-0.
    s_viewCapturedThisXfb.store(false, std::memory_order_relaxed);

    float proj[7], view[6];
    A1bMtx3x4 pos0{};
    ReadXfProj(proj);
    ReadXfViewport(view);
    ReadGxPosMtx0(pos0);
    const uint32_t matIdx = g_gx.cp[CP_MATINDEX_A];
    const uint64_t xfb = Perf_XfbFrameIndex();

    std::lock_guard lk(s_mu);
    if (!s_haveCurr) return;
    std::memcpy(s_curr.xfProj, proj, sizeof(proj));
    std::memcpy(s_curr.xfViewport, view, sizeof(view));
    s_curr.gxPosMtx0 = pos0;
    s_curr.gxMatIndexA = matIdx;
    s_curr.xfbIndex = xfb;
    s_curr.xfValid = FiniteMtx(pos0);
    for (int i = 0; i < 7 && s_curr.xfValid; i++)
        if (!std::isfinite(proj[i])) s_curr.xfValid = false;

    // Refresh view from GPU buffer into curr (may have arrived mid-frame).
    {
        std::lock_guard vlk(s_viewMu);
        if (s_viewValid && FiniteMtx(s_viewMtx))
        {
            s_curr.viewMtx = s_viewMtx;
            s_curr.viewMtxValid = true;
        }
    }

    static int dumpOn = -1;
    if (dumpOn < 0)
    {
        const char* d = std::getenv("SCR_A1B_POSE_DUMP");
        dumpOn = (d && d[0] && d[0] != '0') ? 1 : 0;
    }
    if (dumpOn && s_curr.playerValid)
    {
        static uint64_t lastXfDumpMs = 0;
        const uint64_t now = Platform_TickMs();
        if (now - lastXfDumpMs >= 4000)
        {
            lastXfDumpMs = now;
            const bool pair = s_havePrev;
            float dpos = 0.f;
            if (pair)
            {
                for (int i = 0; i < 3; i++)
                {
                    const float d = s_curr.playerPos[i] - s_prev.playerPos[i];
                    dpos += d * d;
                }
            }
            LOG_INFO("A1b xfb: seq={} prevSeq={} pair={} dpos2={:.4f} xf=ok proj0={:.3f} "
                     "viewT=({:.2f},{:.2f},{:.2f}) ply=({:.2f},{:.2f},{:.2f})",
                     s_curr.seq, pair ? s_prev.seq : 0ull, pair ? "yes" : "no", dpos,
                     s_curr.xfProj[0],
                     s_curr.viewMtx.m[3], s_curr.viewMtx.m[7], s_curr.viewMtx.m[11],
                     s_curr.playerPos[0], s_curr.playerPos[1], s_curr.playerPos[2]);
        }
    }
}

void A1b_OnXfPosMtx0(const float m[12])
{
    if (!EnvOn() || !m) return;
    if (s_viewCapturedThisXfb.exchange(true, std::memory_order_relaxed))
        return;
    A1bMtx3x4 tmp{};
    std::memcpy(tmp.m, m, sizeof(tmp.m));
    if (!FiniteMtx(tmp)) return;
    std::lock_guard lk(s_viewMu);
    s_viewMtx = tmp;
    s_viewValid = true;
}

void A1b_CachePlayerShell(uint32_t shellEa)
{
    if (!EnvOn()) return;
    if (!GuestPtrOk(shellEa)) return;
    s_cachedPlayerShell.store(shellEa, std::memory_order_relaxed);
    s_hookPlayer.fetch_add(1, std::memory_order_relaxed);
    uint32_t body = 0;
    if (BodyFromShell(shellEa, body))
        A3f_NotePlayerBody(body);
}

uint64_t A1b_SnapCount() { return s_seq.load(std::memory_order_relaxed); }

bool A1b_HasPair()
{
    std::lock_guard lk(s_mu);
    return s_havePrev && s_haveCurr;
}

bool A1b_GetCurrent(A1bPoseSnap& out)
{
    std::lock_guard lk(s_mu);
    if (!s_haveCurr) return false;
    out = s_curr;
    return true;
}

bool A1b_GetPrevious(A1bPoseSnap& out)
{
    std::lock_guard lk(s_mu);
    if (!s_havePrev) return false;
    out = s_prev;
    return true;
}

void A1b_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || bufSize == 0) return;
    buf[0] = 0;
    if (!EnvOn()) return;
    A1bPoseSnap cur{};
    if (!A1b_GetCurrent(cur))
    {
        std::snprintf(buf, bufSize, "A1b: waiting for sim frame");
        return;
    }
    std::snprintf(buf, bufSize,
                  "A1b seq %llu sim %llu ply %c(%.0f,%.0f,%.0f) cam %c view %c xf %c",
                  (unsigned long long)cur.seq,
                  (unsigned long long)cur.simFrameId,
                  cur.playerValid ? 'Y' : 'n',
                  cur.playerPos[0], cur.playerPos[1], cur.playerPos[2],
                  cur.cameraNodeValid ? 'Y' : 'n',
                  cur.viewMtxValid ? 'Y' : 'n',
                  cur.xfValid ? 'Y' : 'n');
}

// --- Guest hooks: cache pointers only (call original unmodified) -------------

PPC_OVERRIDE(PlayerGetWorldMtxPtr)
{
    if (EnvOn())
    {
        const uint32_t shell = ctx.r[3];
        if (GuestPtrOk(shell))
            A1b_CachePlayerShell(shell);
    }
    PPC_ORIGINAL(PlayerGetWorldMtxPtr);
}

PPC_OVERRIDE(PlayerGetPos)
{
    if (EnvOn())
    {
        // fn_80033C34(dst, shell): r3 = float*, r4 = player shell
        const uint32_t shell = ctx.r[4];
        if (GuestPtrOk(shell))
            A1b_CachePlayerShell(shell);
    }
    PPC_ORIGINAL(PlayerGetPos);
}

PPC_OVERRIDE(CameraManagerTick)
{
    if (EnvOn())
    {
        const uint32_t mgr = ctx.r[3];
        if (GuestPtrOk(mgr))
        {
            s_cachedCameraMgr.store(mgr, std::memory_order_relaxed);
            s_hookCamera.fetch_add(1, std::memory_order_relaxed);
        }
    }
    PPC_ORIGINAL(CameraManagerTick);
}
