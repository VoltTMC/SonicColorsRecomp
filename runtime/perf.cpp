#include "perf.h"
#include "log.h"
#include "platform/platform.h"
#include "game_addrs.h"
#include "kernel/memory.h"
#include "kernel/scheduler.h"
#include "kernel/threads.h"
#include "cpu/ppc_context.h"
#include "gx/video.h"
#include "audio/music_mod.h"
#include "debug/a1_xf_capture.h"
#include "debug/a1b_pose_capture.h"
#include "debug/a1c_gx_capture.h"
#include "debug/a2b_fifo_archive.h"
#include "debug/a3a_draw_attr.h"
#include "debug/a3b_camera_interp.h"
#include "debug/a3c_env_capture.h"
#include "debug/a3d_scene_coverage.h"
#include "debug/a3i_model_ownership.h"
#include "debug/a3j_model_palette.h"
#include "debug/a3k_owner_transform.h"
#include "debug/pose_capture.h"
#include "debug/pose_qualify.h"
#include "debug/pose_interp.h"
#include "debug/decoupled_present.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdlib>

extern "C" uint32_t g_cfgFrameRate;
extern "C" volatile uint32_t g_ppcIrqPending;
// Recompiled SDK helper: publishes VI wait interval + derived delta (SDA globals).
// Must match PPC_GEN_FUNC (__restrict) — MSVC/clang-cl ABI mangles restrict into the symbol.
namespace ppc { PPC_GEN_FUNC(fn_803E19B0); }

// Set while FrameLoopWait runs under --frame-rate 0 so VIGetRetraceCount can
// satisfy the EQ spin immediately (see hle_os.cpp).
static thread_local bool s_uncappedFrameWait = false;
bool Perf_UncappedFrameWaitActive() { return s_uncappedFrameWait; }

static std::atomic<int> s_hostFpsCap{ 0 };
static uint64_t s_nextSimDeadline = 0;
static uint64_t s_lastRepeatPresent = 0;
static std::atomic<uint64_t> s_repeatN{ 0 };

void Perf_SetHostFpsCap(int cap) { s_hostFpsCap.store(std::clamp(cap, 0, 1000), std::memory_order_relaxed); }
int Perf_HostFpsCap() { return s_hostFpsCap.load(std::memory_order_relaxed); }

// Uncapped: fixed-timestep sim at 60 Hz (same as wait=1 / --60fps).
//
// VSync off: latest-frame Present at display_refresh_hz (or host_fps_cap) — not a 1000 Hz
// spam of an unchanged XFB. Guest still produces ~60 unique XFBs/s.
//
// VSync on: do NOT re-Present here. The unique-XFB path already called Present(1), which
// blocks to the *host* refresh edge. Re-Present(1) would burn another full refresh on the
// guest thread and starve AI/AX. The swapchain holds the image across subsequent refreshes
// (e.g. one 60 Hz XFB occupies two 120 Hz refreshes). Pace this window with IRQ pumps only.
//
// Audio: pump Sched_RunDue / ppc_check_irq like VI spins so AI DMA/AX stay fed mid-wait.
static void PaceUncappedHost(PPCContext& ctx, uint8_t* base)
{
    if (g_cfgFrameRate != 0) return;
    const uint64_t freq = Platform_PerfFrequency();
    if (!freq) return;

    constexpr uint32_t kSimHz = 60;
    const uint64_t simPeriod = freq / kSimHz;
    const bool vsync = Video_VsyncEnabled();
    const int presentCap = s_hostFpsCap.load(std::memory_order_relaxed);

    // Target Present cadence when VSync is off (never a hidden guest-VI 60 Hz cap):
    //   host_fps_cap > 0  → that rate
    //   else              → host display refresh (fallback 120 if unknown)
    float targetHz = 0.0f;
    if (presentCap > 0)
        targetHz = (float)presentCap;
    else
    {
        targetHz = Video_DisplayRefreshHz();
        if (targetHz < 30.0f) targetHz = 120.0f;
    }
    const uint64_t presentPeriod = freq / (uint64_t)(targetHz + 0.5f);

    uint64_t now = Platform_PerfCounter();
    if (!s_nextSimDeadline)
    {
        s_nextSimDeadline = now + simPeriod;
        return;
    }

    // A3q: guest-thread force-mid probe (SCR_MID_POSE_TRACE) even when wait is empty.
    if (DecoupledPresent_Enabled())
        DecoupledPresent_OnGuestPaceEnter(now, freq);

    while (now < s_nextSimDeadline)
    {
        Sched_RunDue();
        Irq_Update();
        if (g_ppcIrqPending)
            ppc_check_irq(ctx, base);

        if (vsync)
        {
            // Hold until next sim; Present(1) already happened for this XFB.
            uint32_t ms = (uint32_t)((s_nextSimDeadline - now) * 1000ull / freq);
            if (ms > 2) ms = 2;
            if (ms > 0) Platform_SleepMs(ms);
            else Platform_SleepMs(0);
        }
        else
        {
            const bool canPresent = !s_lastRepeatPresent
                || (now - s_lastRepeatPresent) >= presentPeriod;
            if (canPresent)
            {
                if (DecoupledPresent_Enabled())
                {
                    // A3o: attempt unique mid-render; falls back to duplicate pace.
                    DecoupledPresent_OnPaceWait(now, s_nextSimDeadline, freq);
                }
                else
                {
                    // Latest completed XFB only — no new guest render between sim steps.
                    Renderer_RepeatLastXfb();
                }
                s_lastRepeatPresent = Platform_PerfCounter();
                s_repeatN.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                const uint64_t waitTicks = presentPeriod - (now - s_lastRepeatPresent);
                uint32_t ms = (uint32_t)(waitTicks * 1000ull / freq);
                if (ms > 2) ms = 2;
                if (ms > 0) Platform_SleepMs(ms);
            }
        }
        now = Platform_PerfCounter();
    }

    s_nextSimDeadline += simPeriod;
    if (now > s_nextSimDeadline + simPeriod * 2)
        s_nextSimDeadline = now + simPeriod;
}

// True guest-60 (wait=1). Frame rate 60 uses wait=1 with spring CD gate
// (runtime/debug/spring_60.cpp). SCR_GUEST_60 / --guest-60 still forces wait=1
// even if frame_rate is left at 30.
static bool Guest60Forced()
{
    static int cached = -1;
    if (cached < 0) cached = std::getenv("SCR_GUEST_60") ? 1 : 0;
    return cached != 0;
}

static uint32_t WaitFields()
{
    if (g_cfgFrameRate == 0) return 0u;
    if (g_cfgFrameRate >= 60 || Guest60Forced()) return 1u;
    return 2u;
}

extern "C" uint32_t Perf_WaitFields() { return WaitFields(); }
extern "C" uint32_t Perf_DtFields()
{
    const uint32_t w = WaitFields();
    return w == 0 ? 1u : w;
}
extern "C" bool Perf_Guest60Forced() { return Guest60Forced(); }

// Guest frame-loop helper (fn_800126EC): one call ≈ one paced game update.
// Republish wait every frame so obj+0xC / PAL force-1 stay in sync with WaitFields().
//
// Uncapped (--frame-rate 0):
//   - Skip VI EQ spin + flush/retrace waits (no 60 Hz presentation lock).
//   - Publish fixed wait=1 / dt=1/60 via E19B0 (same sim step as --60fps).
//   - PaceUncappedHost holds the guest to 60 sim Hz and re-presents between steps.
PPC_OVERRIDE(FrameLoopWait)
{
    const uint32_t w = WaitFields();
    const uint32_t dt = Perf_DtFields();
    const uint32_t obj = ctx.r[3];
    if (obj) Write32(obj + 8, w == 0 ? 1u : w);
    {
        const uint32_t s0 = ctx.r[0], s3 = ctx.r[3], s4 = ctx.r[4], s5 = ctx.r[5], s6 = ctx.r[6];
        ctx.r[3] = dt == 0 ? 1u : dt;
        ppc::fn_803E19B0(ctx, base);
        ctx.r[0] = s0; ctx.r[3] = s3; ctx.r[4] = s4; ctx.r[5] = s5; ctx.r[6] = s6;
    }
    Perf_OnGuestFrameBegin();
    s_uncappedFrameWait = (w == 0);
    PPC_ORIGINAL(FrameLoopWait);
    s_uncappedFrameWait = false;
    MusicMod_Tick();
    if (w == 0)
        PaceUncappedHost(ctx, base);
    Perf_OnGuestFrameEnd();
}

static bool s_on = false;

static std::atomic<uint64_t> s_guestFrameIndex{ 0 };
static std::atomic<uint64_t> s_xfbTotal{ 0 }, s_presentTotal{ 0 };  // always-on
static std::atomic<uint64_t> s_uniquePresentedTotal{ 0 };           // XFBs presented ≥ once
static std::atomic<uint64_t> s_lastUniquePresentedXfb{ 0 };         // watermark vs s_xfbTotal
static std::atomic<uint64_t> s_guestN{ 0 }, s_presentN{ 0 }, s_xfbN{ 0 }, s_uniquePresentedN{ 0 };
static std::atomic<uint64_t> s_guestNs{ 0 }, s_presentNs{ 0 }, s_gxSyncNs{ 0 }, s_viWaitNs{ 0 };
static std::atomic<uint64_t> s_gapNs{ 0 };   // wall time between FrameLoopWait exit → next enter
static std::atomic<uint64_t> s_gxSyncCount{ 0 }, s_viSpinIters{ 0 };
static std::atomic<uint64_t> s_viPostSkip{ 0 }, s_viPostRun{ 0 };  // post-EQ VI barriers
static std::atomic<uint64_t> s_lastPresentNs{ 0 };
static std::atomic<uint64_t> s_presentSmoothNs{ 0 };
static std::atomic<uint32_t> s_lastXfbDraws{ 0 };
static std::atomic<uint64_t> s_gpuSmoothNs{ 0 };

// Per-thread timing (guest CPU thread / GX present path).
static thread_local uint64_t t_guest0 = 0, t_present0 = 0, t_gx0 = 0;
static thread_local uint64_t t_viSpin0 = 0;
static thread_local uint64_t t_guestEnd = 0;   // last FrameLoopWait exit (for gap)
static thread_local bool t_viSpinning = false;

static uint64_t Now() { return Platform_PerfCounter(); }
static uint64_t Freq() { return Platform_PerfFrequency(); }

void Perf_SetEnabled(bool on) { s_on = on; }
bool Perf_Enabled() { return s_on; }

void Perf_OnGuestFrameBegin()
{
    if (!s_on) return;
    const uint64_t now = Now();
    if (t_guestEnd)
        s_gapNs.fetch_add(now - t_guestEnd, std::memory_order_relaxed);
    t_guest0 = now;
}

void Perf_OnGuestFrameEnd()
{
    s_guestFrameIndex.fetch_add(1, std::memory_order_relaxed);
    A1b_OnSimFrameEnd();  // no-op unless SCR_A1B_POSE_CAPTURE=1
    A1c_OnSimFrameEnd();  // no-op unless SCR_A1C_GX_CAPTURE=1
    A2b_OnSimFrameEnd();  // no-op unless SCR_A2B_FIFO_CAPTURE=1
    A3a_OnSimFrameEnd();  // no-op unless SCR_A3A_DRAW_ATTR=1
    A3b_OnSimFrameEnd();  // no-op unless SCR_A3B_CAMERA_INTERP=1
    if (!s_on || !t_guest0) return;
    const uint64_t now = Now();
    s_guestNs.fetch_add(now - t_guest0, std::memory_order_relaxed);
    s_guestN.fetch_add(1, std::memory_order_relaxed);
    t_guest0 = 0;
    t_guestEnd = now;
}

uint64_t Perf_GuestFrameIndex()
{
    return s_guestFrameIndex.load(std::memory_order_relaxed);
}

uint64_t Perf_XfbFrameIndex()
{
    return s_xfbTotal.load(std::memory_order_relaxed);
}

uint64_t Perf_PresentFrameIndex()
{
    return s_presentTotal.load(std::memory_order_relaxed);
}

uint64_t Perf_UniqueFramesPresentedIndex()
{
    return s_uniquePresentedTotal.load(std::memory_order_relaxed);
}

void Perf_OnViSpinSample(bool spinning)
{
    if (!s_on) return;
    if (spinning)
    {
        s_viSpinIters.fetch_add(1, std::memory_order_relaxed);
        if (!t_viSpinning)
        {
            t_viSpinning = true;
            t_viSpin0 = Now();
        }
    }
    else if (t_viSpinning)
    {
        s_viWaitNs.fetch_add(Now() - t_viSpin0, std::memory_order_relaxed);
        t_viSpinning = false;
        t_viSpin0 = 0;
    }
}

void Perf_OnViPostWaitSample(bool skipped)
{
    if (!s_on) return;
    if (skipped) s_viPostSkip.fetch_add(1, std::memory_order_relaxed);
    else s_viPostRun.fetch_add(1, std::memory_order_relaxed);
}

void Perf_OnGxSyncBegin()
{
    if (!s_on) return;
    t_gx0 = Now();
}

void Perf_OnGxSyncEnd()
{
    if (!s_on || !t_gx0) return;
    s_gxSyncNs.fetch_add(Now() - t_gx0, std::memory_order_relaxed);
    s_gxSyncCount.fetch_add(1, std::memory_order_relaxed);
    t_gx0 = 0;
}

void Perf_OnPresentBegin()
{
    // Always sample Present duration for the FPS overlay (cheap wall clock).
    t_present0 = Now();
}

void Perf_OnPresentEnd()
{
    if (!t_present0) return;
    const uint64_t dt = Now() - t_present0;
    t_present0 = 0;
    // Coordinate uncapped latest-frame pacing with guest XFB Presents so total
    // present_calls track display_refresh_hz (not display_hz + sim_hz).
    s_lastRepeatPresent = Now();
    s_lastPresentNs.store(dt, std::memory_order_relaxed);
    uint64_t prev = s_presentSmoothNs.load(std::memory_order_relaxed);
    const uint64_t next = prev ? (prev * 7 + dt) / 8 : dt;
    s_presentSmoothNs.store(next, std::memory_order_relaxed);
    s_presentTotal.fetch_add(1, std::memory_order_relaxed);
    // Count each new XFB the first time any Present shows it (not every re-Present).
    {
        const uint64_t xfb = s_xfbTotal.load(std::memory_order_acquire);
        uint64_t prev = s_lastUniquePresentedXfb.load(std::memory_order_relaxed);
        while (xfb > prev)
        {
            if (s_lastUniquePresentedXfb.compare_exchange_weak(prev, xfb, std::memory_order_acq_rel))
            {
                const uint64_t n = xfb - prev;
                s_uniquePresentedTotal.fetch_add(n, std::memory_order_relaxed);
                if (s_on)
                    s_uniquePresentedN.fetch_add(n, std::memory_order_relaxed);
                break;
            }
        }
    }
    if (!s_on) return;
    s_presentNs.fetch_add(dt, std::memory_order_relaxed);
    s_presentN.fetch_add(1, std::memory_order_relaxed);
}

void Perf_OnXfbCopy()
{
    s_xfbTotal.fetch_add(1, std::memory_order_relaxed);
    A1_OnXfbCopy();   // no-op unless SCR_A1_XF_CAPTURE=1
    A1b_OnXfbCopy();  // no-op unless SCR_A1B_POSE_CAPTURE=1
    A1c_OnXfbCopy();  // no-op unless SCR_A1C_GX_CAPTURE=1
    A2b_OnXfbCopy();  // no-op unless SCR_A2B_FIFO_CAPTURE=1
    A3a_OnXfbCopy();  // no-op unless SCR_A3A_DRAW_ATTR=1 (also drains A3i when A3a on)
    A3i_OnXfbCopyStandalone();  // drains A3i when enabled without A3a
    A3j_OnXfbCopyStandalone();  // drains A3j when enabled without A3a
    A3k_OnXfbCopyStandalone();  // drains A3k when enabled without A3a
    PoseCapture_OnXfb();  // no-op unless SCR_POSE_CAPTURE/QUALIFY/INTERP
    PoseQualify_OnXfb();  // no-op unless SCR_POSE_QUALIFY=1
    PoseInterp_OnXfb();   // no-op unless SCR_POSE_INTERP=1
    DecoupledPresent_OnXfb();  // no-op unless SCR_DECOUPLED_PRESENT=1
    A3b_OnXfbCopy();  // no-op unless SCR_A3B_CAMERA_INTERP=1
    A3c_OnXfbCopy();  // no-op unless SCR_A3C_ENV_CAPTURE/INTERP=1
    A3d_OnXfbCopy();  // no-op unless SCR_A3D_SCENE_COVERAGE=1
    if (!s_on) return;
    s_xfbN.fetch_add(1, std::memory_order_relaxed);
}

void Perf_NoteXfbDraws(uint32_t draws)
{
    s_lastXfbDraws.store(draws, std::memory_order_relaxed);
}

float Perf_LastPresentMs()
{
    const uint64_t freq = Freq();
    if (!freq) return 0.0f;
    return (float)(s_lastPresentNs.load(std::memory_order_relaxed) * 1000.0 / (double)freq);
}

float Perf_PresentMsSmooth()
{
    const uint64_t freq = Freq();
    if (!freq) return 0.0f;
    return (float)(s_presentSmoothNs.load(std::memory_order_relaxed) * 1000.0 / (double)freq);
}

uint32_t Perf_LastXfbDraws()
{
    return s_lastXfbDraws.load(std::memory_order_relaxed);
}

void Perf_NoteGpuNs(uint64_t gpuNs)
{
    uint64_t prev = s_gpuSmoothNs.load(std::memory_order_relaxed);
    const uint64_t next = prev ? (prev * 7 + gpuNs) / 8 : gpuNs;
    s_gpuSmoothNs.store(next, std::memory_order_relaxed);
}

float Perf_GpuMsSmooth()
{
    return (float)(s_gpuSmoothNs.load(std::memory_order_relaxed) / 1.0e6);
}

void Perf_LogStatus(double secs)
{
    if (!s_on || secs <= 0.0) return;
    const double inv = 1.0 / secs;
    const double nsToMs = 1000.0 / (double)Freq();

    uint64_t guestN = s_guestN.exchange(0);
    uint64_t presentN = s_presentN.exchange(0);
    uint64_t xfbN = s_xfbN.exchange(0);
    uint64_t uniquePresN = s_uniquePresentedN.exchange(0);
    uint64_t guestNs = s_guestNs.exchange(0);
    uint64_t presentNs = s_presentNs.exchange(0);
    uint64_t gxNs = s_gxSyncNs.exchange(0);
    uint64_t viNs = s_viWaitNs.exchange(0);
    uint64_t gapNs = s_gapNs.exchange(0);
    uint64_t gxCnt = s_gxSyncCount.exchange(0);
    uint64_t viIters = s_viSpinIters.exchange(0);
    uint64_t viPostSkip = s_viPostSkip.exchange(0);
    uint64_t viPostRun = s_viPostRun.exchange(0);
    uint64_t repeatN = s_repeatN.exchange(0);

    double guestHz = guestN * inv;       // simulation steps (FrameLoopWait)
    double presentHz = presentN * inv;   // all Present() including re-presents
    double xfbHz = xfbN * inv;           // guest XFB copies
    double uniquePresHz = uniquePresN * inv; // first Present per XFB only
    double repeatHz = repeatN * inv;
    // guestN begins counted this window; gap samples are N-1 ≈ N for large N
    double guestMs = guestN ? (guestNs * nsToMs) / guestN : 0;
    double gapMs = guestN ? (gapNs * nsToMs) / guestN : 0;
    double presentMs = presentN ? (presentNs * nsToMs) / presentN : 0;
    double gxMs = gxCnt ? (gxNs * nsToMs) / gxCnt : 0;
    double viMsTot = viNs * nsToMs;
    double viMsPerGuest = guestN ? viMsTot / guestN : 0;

    LOG_INFO("perf: simulation_hz {:.1f} ({:.2f} ms in-wait + {:.2f} ms gap), unique_xfb {:.1f}, "
             "unique_frames_presented {:.1f}, present_calls {:.1f} (repeat {:.1f}, {:.2f} ms), "
             "display_refresh_hz {:.0f}, vsync={}, VI-wait {:.2f} ms/sim ({} spins/interval), "
             "VI-post skip/run {}/{}, GX-sync {:.2f} ms x{}, GPU blit {:.2f} ms, "
             "draws/xfb {}, RSS {} MiB",
             guestHz, guestMs, gapMs, xfbHz, uniquePresHz, presentHz, repeatHz, presentMs,
             Video_DisplayRefreshHz(), g_videoConfig.vsync ? "on" : "off",
             viMsPerGuest, viIters, viPostSkip, viPostRun, gxMs, gxCnt,
             Perf_GpuMsSmooth(), Perf_LastXfbDraws(), Platform_ProcessRssMb());
}
