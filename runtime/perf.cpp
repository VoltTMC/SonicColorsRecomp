#include "perf.h"
#include "log.h"
#include "platform/platform.h"
#include "game_addrs.h"
#include "kernel/memory.h"
#include "audio/music_mod.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdlib>

extern "C" uint32_t g_cfgFrameRate;
// Recompiled SDK helper: publishes VI wait interval + derived delta (SDA globals).
// Must match PPC_GEN_FUNC (__restrict) — MSVC/clang-cl ABI mangles restrict into the symbol.
namespace ppc { PPC_GEN_FUNC(fn_803E19B0); }

// Set while FrameLoopWait runs under --frame-rate 0 so VIGetRetraceCount can
// satisfy the EQ spin immediately (see hle_os.cpp).
static thread_local bool s_uncappedFrameWait = false;
bool Perf_UncappedFrameWaitActive() { return s_uncappedFrameWait; }

static std::atomic<int> s_hostFpsCap{ 0 };
static uint64_t s_lastUncappedPace = 0;

void Perf_SetHostFpsCap(int cap) { s_hostFpsCap.store(std::clamp(cap, 0, 1000), std::memory_order_relaxed); }
int Perf_HostFpsCap() { return s_hostFpsCap.load(std::memory_order_relaxed); }

static void PaceUncappedHost()
{
    const int cap = s_hostFpsCap.load(std::memory_order_relaxed);
    if (cap <= 0 || g_cfgFrameRate != 0) return;
    const uint64_t freq = Platform_PerfFrequency();
    if (!freq) return;
    const uint64_t minDelta = freq / (uint64_t)cap;
    const uint64_t now = Platform_PerfCounter();
    if (s_lastUncappedPace && now - s_lastUncappedPace < minDelta)
    {
        const uint64_t wait = minDelta - (now - s_lastUncappedPace);
        const uint32_t ms = (uint32_t)(wait * 1000ull / freq);
        if (ms > 0) Platform_SleepMs(ms);
        while (Platform_PerfCounter() - s_lastUncappedPace < minDelta) {}
    }
    s_lastUncappedPace = Platform_PerfCounter();
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
// Uncapped (--frame-rate 0): keep a 1-field float dt via E19B0(1), and mark the
// VI EQ spin so VIGetRetraceCount returns a value that ends it immediately.
// Guest then runs as fast as the host allows (fast-forward sim @ fixed dt);
// present stays coupled ~1:1 with guest updates.
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
    PaceUncappedHost();
    Perf_OnGuestFrameEnd();
}

static bool s_on = false;

static std::atomic<uint64_t> s_guestFrameIndex{ 0 };
static std::atomic<uint64_t> s_guestN{ 0 }, s_presentN{ 0 }, s_xfbN{ 0 };
static std::atomic<uint64_t> s_guestNs{ 0 }, s_presentNs{ 0 }, s_gxSyncNs{ 0 }, s_viWaitNs{ 0 };
static std::atomic<uint64_t> s_gapNs{ 0 };   // wall time between FrameLoopWait exit → next enter
static std::atomic<uint64_t> s_gxSyncCount{ 0 }, s_viSpinIters{ 0 };

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
    if (!s_on) return;
    t_present0 = Now();
}

void Perf_OnPresentEnd()
{
    if (!s_on || !t_present0) return;
    s_presentNs.fetch_add(Now() - t_present0, std::memory_order_relaxed);
    s_presentN.fetch_add(1, std::memory_order_relaxed);
    t_present0 = 0;
}

void Perf_OnXfbCopy()
{
    if (!s_on) return;
    s_xfbN.fetch_add(1, std::memory_order_relaxed);
}

void Perf_LogStatus(double secs)
{
    if (!s_on || secs <= 0.0) return;
    const double inv = 1.0 / secs;
    const double nsToMs = 1000.0 / (double)Freq();

    uint64_t guestN = s_guestN.exchange(0);
    uint64_t presentN = s_presentN.exchange(0);
    uint64_t xfbN = s_xfbN.exchange(0);
    uint64_t guestNs = s_guestNs.exchange(0);
    uint64_t presentNs = s_presentNs.exchange(0);
    uint64_t gxNs = s_gxSyncNs.exchange(0);
    uint64_t viNs = s_viWaitNs.exchange(0);
    uint64_t gapNs = s_gapNs.exchange(0);
    uint64_t gxCnt = s_gxSyncCount.exchange(0);
    uint64_t viIters = s_viSpinIters.exchange(0);

    double guestHz = guestN * inv;
    double presentHz = presentN * inv;
    double xfbHz = xfbN * inv;
    // guestN begins counted this window; gap samples are N-1 ≈ N for large N
    double guestMs = guestN ? (guestNs * nsToMs) / guestN : 0;
    double gapMs = guestN ? (gapNs * nsToMs) / guestN : 0;
    double presentMs = presentN ? (presentNs * nsToMs) / presentN : 0;
    double gxMs = gxCnt ? (gxNs * nsToMs) / gxCnt : 0;
    double viMsTot = viNs * nsToMs;
    double viMsPerGuest = guestN ? viMsTot / guestN : 0;

    LOG_INFO("perf: guest {:.1f} Hz ({:.2f} ms in-wait + {:.2f} ms gap), xfb {:.1f} Hz, present {:.1f} Hz ({:.2f} ms), "
             "VI-wait {:.2f} ms/guest ({} spins), GX-sync {:.2f} ms x{}",
             guestHz, guestMs, gapMs, xfbHz, presentHz, presentMs, viMsPerGuest, viIters, gxMs, gxCnt);
}
