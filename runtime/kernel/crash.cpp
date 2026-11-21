// Crash reporting: maps host faults back to guest functions and dumps guest state. The
// fault handler, host stack walks and the sampling profilers are in platform/*/crash.cpp.
#include "crash.h"
#include "threads.h"
#include "memory.h"
#include "../env.h"
#include "../log.h"
#include "../cpu/cpu.h"
#include "../platform/platform.h"
#include "../perf.h"
#include "../gx/video.h"
#include "../region.h"

#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

static std::vector<std::pair<uintptr_t, uint32_t>> s_hostToGuest;

void Crash_BuildMap()
{
    for (const PPCFuncMapping* m = Region_Code().funcs; m->host; m++)
        s_hostToGuest.push_back({ (uintptr_t)m->host, m->guest });
    std::sort(s_hostToGuest.begin(), s_hostToGuest.end());
}

uint32_t Crash_GuestFunctionForHost(uintptr_t rip)
{
    auto it = std::upper_bound(s_hostToGuest.begin(), s_hostToGuest.end(), std::make_pair(rip, 0xFFFFFFFFu));
    if (it == s_hostToGuest.begin()) return 0;
    --it;
    return it->second;
}

uintptr_t Crash_FirstHostFunction() { return s_hostToGuest.empty() ? 0 : s_hostToGuest[0].first; }

void Crash_DumpContext(const PPCContext& c)
{
    for (int i = 0; i < 32; i += 4)
        LOG_ERROR("  r{:<2}={:08x} r{:<2}={:08x} r{:<2}={:08x} r{:<2}={:08x}", i, c.r[i], i + 1, c.r[i + 1], i + 2, c.r[i + 2], i + 3, c.r[i + 3]);
    LOG_ERROR("  lr={:08x} ctr={:08x} msr={:08x}", c.lr, c.ctr, c.msr);
}

// ---------------------------------------------------------------- watchdog
extern uint32_t g_framesPresented;
extern "C" uint32_t g_cfgFrameRate;

static void WatchdogThread()
{
    uint32_t lastFrames = 0;
    for (;;)
    {
        Platform_SleepMs(3000);
        if (!Crash_GuestThreadKnown()) continue;
        // sample a few times and report the most frequent guest functions
        uint32_t samples[16];
        for (int i = 0; i < 16; i++)
        {
            samples[i] = Crash_GuestFunctionForHost(Crash_SampleGuestPC());
            Platform_SleepMs(5);
        }
        std::string s;
        for (int i = 0; i < 16; i++) s += std::format("{:08X} ", samples[i]);
        extern uint32_t g_excCount[16];
        extern uint32_t PI_Read32(uint32_t off);
        extern uint32_t g_viFieldCount;
        {
            extern int g_audioPeak;
            static uint32_t lastFields = 0;
            static uint64_t lastQpc = 0;
            uint64_t q = Platform_PerfCounter();
            double secs = lastQpc ? (double)(q - lastQpc) / Platform_PerfFrequency() : 3.0;
            lastQpc = q;
            extern int g_audioUnderruns, g_audioDrops, g_audioRepeats;
            extern uint64_t g_audioFrames;
            static uint64_t lastAudio = 0;
            static uint64_t lastSim = 0, lastXfb = 0, lastUniquePres = 0, lastPresent = 0;
            const char* tgt = g_cfgFrameRate == 0 ? "uncapped"
                              : (g_cfgFrameRate >= 60 || Perf_Guest60Forced() ? "60/guest" : "30");
            const uint64_t sim = Perf_GuestFrameIndex();
            const uint64_t xfb = Perf_XfbFrameIndex();
            const uint64_t uniquePres = Perf_UniqueFramesPresentedIndex();
            const uint64_t present = Perf_PresentFrameIndex();
            const float dispHz = Video_DisplayRefreshHz();
            // present_calls = swapchain Present() (may re-show the same XFB).
            // unique_xfb = guest EFB→XFB copies. unique_frames_presented = first Present per XFB.
            // display_refresh_hz = physical monitor mode — not equal to present_calls.
            LOG_INFO("status: target {} | simulation_hz {:.1f} | unique_xfb {:.1f} | "
                     "unique_frames_presented {:.1f} | present_calls {:.1f} | "
                     "guest_vi_hz {:.2f} | display_refresh_hz {:.0f} | vsync {}{} | "
                     "audio peak {} underruns {} drops {} repeats {} rate {:.0f} Hz",
                     tgt,
                     (sim - lastSim) / secs,
                     (xfb - lastXfb) / secs,
                     (uniquePres - lastUniquePres) / secs,
                     (present - lastPresent) / secs,
                     (g_viFieldCount - lastFields) / secs,
                     dispHz,
                     g_videoConfig.vsync ? "on" : "off",
                     Video_AllowTearing() ? "+tear" : "",
                     g_audioPeak, g_audioUnderruns, g_audioDrops, g_audioRepeats,
                     (g_audioFrames - lastAudio) / secs);
            extern void Perf_LogStatus(double);
            Perf_LogStatus(secs);
            g_audioRepeats = 0;
            lastAudio = g_audioFrames;
            g_audioUnderruns = g_audioDrops = 0;
            lastFields = g_viFieldCount;
            lastSim = sim;
            lastXfb = xfb;
            lastUniquePres = uniquePres;
            lastPresent = present;
            g_audioPeak = 0;
        }
        if (g_framesPresented == lastFrames)
        {
            LOG_INFO("watchdog: no new frames; guest at: {}", s);
            LOG_INFO("  exceptions: ext={} dec={}  PI cause={:08x} mask={:08x}  VI fields={}  curThread={:08x}", g_excCount[4], g_excCount[8],
                     PI_Read32(0), PI_Read32(4), g_viFieldCount, Read32(0x800000E4));
            extern uint16_t VI_Read16(uint32_t off);
            LOG_INFO("  VI: DI0={:04x}{:04x} DI1={:04x}{:04x} DI2={:04x}{:04x} DI3={:04x}{:04x} vtr={:04x} dcr={:04x} beam={}", VI_Read16(0x30), VI_Read16(0x32),
                     VI_Read16(0x34), VI_Read16(0x36), VI_Read16(0x38), VI_Read16(0x3A), VI_Read16(0x3C), VI_Read16(0x3E), VI_Read16(0), VI_Read16(2), VI_Read16(0x2C));
            bool armed; extern uint64_t Dec_Deadline(bool*); uint64_t dl = Dec_Deadline(&armed);
            LOG_INFO("  DEC armed={} remaining={}", armed, (int64_t)(dl - Timebase_Now()));
            if (g_curThread)
            {
                const PPCContext& c = g_curThread->ctx;
                std::string bt;
                uint32_t fp = c.r[1];
                for (int i = 0; i < 12 && fp >= 0x80000000 && fp < 0x93600000; i++) { uint32_t nx = Read32(fp); if (nx < 0x80000000 || nx >= 0x93600000) break; bt += std::format("{:08x} ", Read32(nx + 4)); fp = nx; }
                LOG_INFO("  current fiber: lr={:08x} msr={:08x} r1={:08x} r3={:08x} chain: {}", c.lr, c.msr, c.r[1], c.r[3], bt);
            }
            extern void Crash_DumpThreads();
            Crash_DumpThreads();
        }
        lastFrames = g_framesPresented;
    }
}

// BKR_IRQ_STALLS=1: sample where the guest thread is when it has not polled interrupts

void Crash_StartWatchdog()
{
    Crash_StartSamplers();
    std::thread(WatchdogThread).detach();
    // SCR_TEST_PAUSE=start,end (seconds) (legacy BKR_*): focus-loss pause without touching focus
    if (const char* tp = EnvGet("SCR_TEST_PAUSE", "BKR_TEST_PAUSE"))
    {
        int a = 0, b = 0;
        if (sscanf(tp, "%d,%d", &a, &b) == 2 && b > a)
            std::thread([a, b] {
                extern std::atomic<bool> g_pauseRequested;
                Platform_SleepMs(a * 1000);
                g_pauseRequested = true;
                Platform_SleepMs((b - a) * 1000);
                g_pauseRequested = false;
            }).detach();
    }
}

// Walk the OS active thread queue and print each thread with a back-chain trace.
void Crash_DumpThreads()
{
    uint32_t t = Read32(0x800000DC);
    for (int n = 0; t && n < 32; n++)
    {
        uint16_t state = Read16(t + 0x2C8);
        int32_t prio = (int32_t)Read32(t + 0x2D0);
        uint32_t queue = Read32(t + 0x2DC);
        uint32_t sp = Read32(t + 4);
        std::string bt;
        uint32_t fp = sp;
        for (int i = 0; i < 10 && fp >= 0x80000000 && fp < 0x81800000; i++)
        {
            uint32_t next = Read32(fp);
            if (next < 0x80000000 || next >= 0x81800000) break;
            bt += std::format("{:08x} ", Read32(next + 4));
            fp = next;
        }
        LOG_INFO("  thread {:08x} state={} prio={} queue={:08x} sp={:08x} lr-chain: {}", t, state, prio, queue, sp, bt);
        t = Read32(t + 0x2FC);
    }
}
