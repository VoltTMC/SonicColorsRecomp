// Host acquisition → guest consumption timing for controller latency investigations.
#include "input_latency.h"
#include "../log.h"
#include "../perf.h"
#include "../platform/platform.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_on{ -1 };
std::mutex s_mu;

constexpr int kSlots = 4;
constexpr int kRing = 256;

struct Sample
{
    uint64_t seq = 0;
    uint64_t acquirePerf = 0;
    uint64_t publishPerf = 0;
    uint64_t simFrame = 0;
    uint64_t xfb = 0;
    uint16_t buttons = 0;
    int16_t lx = 0, ly = 0;
    uint8_t lt = 0, rt = 0;
    uint8_t published = 0;
};

struct SlotStats
{
    uint64_t seq = 0;
    uint64_t lastAcquirePerf = 0;
    uint64_t lastPublishPerf = 0;
    uint64_t lastButtons = 0;
    uint64_t acquireCount = 0;
    uint64_t publishCount = 0;
    uint64_t changeCount = 0;
    uint64_t stalePublish = 0;  // publish with same buttons as previous acquire window
    double sumAcquireIntervalMs = 0;
    double sumAcquireToPublishMs = 0;
    double maxAcquireIntervalMs = 0;
    double maxAcquireToPublishMs = 0;
    double lastAcquireIntervalMs = 0;
    double lastAcquireToPublishMs = 0;
    // Reservoir for percentiles (acquire intervals).
    float intervals[kRing]{};
    int intervalN = 0;
    int intervalW = 0;
    Sample ring[kRing]{};
    int ringW = 0;
};

SlotStats s_slot[kSlots];
uint64_t s_lastLogMs = 0;

bool EnvOn()
{
    const char* e = std::getenv("SCR_INPUT_LATENCY");
    return e && e[0] && e[0] != '0';
}

double PerfMs(uint64_t a, uint64_t b)
{
    const uint64_t f = Platform_PerfFrequency();
    if (!f || b < a) return 0.0;
    return (double)(b - a) * 1000.0 / (double)f;
}

float Percentile(float* v, int n, float p)
{
    if (n <= 0) return 0.f;
    std::sort(v, v + n);
    const float idx = p * (float)(n - 1);
    const int i = (int)idx;
    const float f = idx - (float)i;
    if (i + 1 >= n) return v[n - 1];
    return v[i] * (1.f - f) + v[i + 1] * f;
}

}  // namespace

bool InputLatency_Enabled()
{
    int v = s_on.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvOn() ? 1 : 0;
        s_on.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("input-latency: enabled (SCR_INPUT_LATENCY / --input-latency); "
                     "reports acquire→publish ms (not physical end-to-end)");
    }
    return v != 0;
}

void InputLatency_SetEnabled(bool on)
{
    s_on.store(on ? 1 : 0, std::memory_order_relaxed);
    if (on)
        LOG_INFO("input-latency: enabled (--input-latency); "
                 "reports acquire→publish ms (not physical end-to-end)");
}

void InputLatency_OnAcquire(int slot, uint16_t buttons, int16_t lx, int16_t ly, uint8_t lt,
                            uint8_t rt, uint64_t seq)
{
    if (!InputLatency_Enabled()) return;
    if (slot < 0 || slot >= kSlots) return;
    const uint64_t now = Platform_PerfCounter();
    std::lock_guard lk(s_mu);
    SlotStats& s = s_slot[slot];
    if (s.lastAcquirePerf)
    {
        const double dt = PerfMs(s.lastAcquirePerf, now);
        s.lastAcquireIntervalMs = dt;
        s.sumAcquireIntervalMs += dt;
        if (dt > s.maxAcquireIntervalMs) s.maxAcquireIntervalMs = dt;
        s.intervals[s.intervalW] = (float)dt;
        s.intervalW = (s.intervalW + 1) % kRing;
        if (s.intervalN < kRing) s.intervalN++;
    }
    if (buttons != (uint16_t)s.lastButtons || lx != 0 || ly != 0 || lt || rt)
    {
        // Count edge-ish activity (any non-neutral or button change).
        if (buttons != (uint16_t)s.lastButtons) s.changeCount++;
    }
    s.lastButtons = buttons;
    s.lastAcquirePerf = now;
    s.seq = seq;
    s.acquireCount++;

    Sample& sm = s.ring[s.ringW];
    sm = Sample{};
    sm.seq = seq;
    sm.acquirePerf = now;
    sm.buttons = buttons;
    sm.lx = lx;
    sm.ly = ly;
    sm.lt = lt;
    sm.rt = rt;
    sm.simFrame = Perf_GuestFrameIndex();
    sm.xfb = Perf_XfbFrameIndex();
    s.ringW = (s.ringW + 1) % kRing;
}

void InputLatency_OnPublish(int slot, uint64_t acquireSeq)
{
    if (!InputLatency_Enabled()) return;
    if (slot < 0 || slot >= kSlots) return;
    const uint64_t now = Platform_PerfCounter();
    std::lock_guard lk(s_mu);
    SlotStats& s = s_slot[slot];
    if (!s.lastAcquirePerf) return;
    const double age = PerfMs(s.lastAcquirePerf, now);
    s.lastAcquireToPublishMs = age;
    s.sumAcquireToPublishMs += age;
    if (age > s.maxAcquireToPublishMs) s.maxAcquireToPublishMs = age;
    s.lastPublishPerf = now;
    s.publishCount++;
    if (acquireSeq && acquireSeq == s.seq)
    {
        // Mark matching ring sample published.
        for (int i = 0; i < kRing; i++)
        {
            Sample& sm = s.ring[i];
            if (sm.seq == acquireSeq && !sm.published)
            {
                sm.publishPerf = now;
                sm.published = 1;
                break;
            }
        }
    }
}

void InputLatency_Tick()
{
    if (!InputLatency_Enabled()) return;
    const uint64_t nowMs = Platform_TickMs();
    if (s_lastLogMs && nowMs - s_lastLogMs < 2000) return;
    s_lastLogMs = nowMs;

    std::lock_guard lk(s_mu);
    for (int slot = 0; slot < kSlots; slot++)
    {
        SlotStats& s = s_slot[slot];
        if (s.acquireCount < 8) continue;
        float tmp[kRing];
        const int n = s.intervalN;
        std::memcpy(tmp, s.intervals, (size_t)n * sizeof(float));
        const float p50 = Percentile(tmp, n, 0.50f);
        std::memcpy(tmp, s.intervals, (size_t)n * sizeof(float));
        const float p95 = Percentile(tmp, n, 0.95f);
        std::memcpy(tmp, s.intervals, (size_t)n * sizeof(float));
        const float p99 = Percentile(tmp, n, 0.99f);
        const double avgInt =
            s.acquireCount > 1 ? s.sumAcquireIntervalMs / (double)(s.acquireCount - 1) : 0.0;
        const double avgAge =
            s.publishCount ? s.sumAcquireToPublishMs / (double)s.publishCount : 0.0;
        LOG_INFO("input-latency slot{}: acquire_interval_ms avg={:.2f} p50={:.2f} p95={:.2f} "
                 "p99={:.2f} max={:.2f} | acquire→publish_ms avg={:.3f} max={:.3f} last={:.3f} "
                 "| acquires={} publishes={} btn_changes={} (backend only; not physical E2E)",
                 slot, avgInt, p50, p95, p99, s.maxAcquireIntervalMs, avgAge,
                 s.maxAcquireToPublishMs, s.lastAcquireToPublishMs, s.acquireCount, s.publishCount,
                 s.changeCount);
    }
}

void InputLatency_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!InputLatency_Enabled()) return;
    std::lock_guard lk(s_mu);
    const SlotStats& s = s_slot[0];
    if (s.acquireCount == 0)
    {
        std::snprintf(buf, bufSize, "inLat: waiting");
        return;
    }
    std::snprintf(buf, bufSize, "inLat acqΔ=%.1fms age=%.2fms maxAge=%.1fms n=%llu",
                  s.lastAcquireIntervalMs, s.lastAcquireToPublishMs, s.maxAcquireToPublishMs,
                  (unsigned long long)s.acquireCount);
}
