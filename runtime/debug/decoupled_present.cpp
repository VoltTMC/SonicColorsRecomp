// Approach A3o — decoupled presentation scheduler (SCR_DECOUPLED_PRESENT).
#include "decoupled_present.h"
#include "a2b_fifo_archive.h"
#include "a2c_scratch_replay.h"
#include "pose_capture.h"
#include "pose_interp.h"
#include "../gx/gx_state.h"
#include "../log.h"
#include "../perf.h"
#include "../platform/platform.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_on{ -1 };
std::atomic<int> s_trace{ -1 };
std::mutex s_mu;
DecoupledPresentStats s_st{};
float s_rate = 2.f;
float s_latencyMs = -1.f;  // <0 → auto = 0.5 * sim period
uint64_t s_lastSimTicks = 0;
uint64_t s_lastPresentTicks = 0;
uint32_t s_midsThisSim = 0;
uint32_t s_logN = 0;
char s_lastDetail[192]{};

bool EnvFlag(const char* name)
{
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

float EnvFloat(const char* name, float def)
{
    const char* e = std::getenv(name);
    if (!e || !e[0]) return def;
    char* end = nullptr;
    const float v = std::strtof(e, &end);
    if (end == e) return def;
    return v;
}

bool On()
{
    int v = s_on.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_DECOUPLED_PRESENT") ? 1 : 0;
        s_on.store(v, std::memory_order_relaxed);
        s_rate = EnvFloat("SCR_DECOUPLED_PRESENT_RATE", 2.f);
        if (s_rate < 1.f) s_rate = 1.f;
        if (s_rate > 4.f) s_rate = 4.f;
        s_latencyMs = EnvFloat("SCR_PRESENT_LATENCY_MS", -1.f);
        s_st.targetRate = s_rate;
        s_st.latencyMs = s_latencyMs;
        if (v)
            LOG_INFO("DecoupledPresent: enabled rate={:.1f} latencyMs={} (unique mid needs "
                     "SCR_A2B_FIFO_CAPTURE + ready dense/env pass; PoseInterp via "
                     "SCR_POSE_INTERP)",
                     s_rate, s_latencyMs < 0.f ? "auto(~8.3)" : "env");
    }
    return v != 0;
}

bool TraceOn()
{
    int v = s_trace.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_PRESENT_TRACE") ? 1 : 0;
        s_trace.store(v, std::memory_order_relaxed);
    }
    return v != 0;
}

uint64_t LatencyTicks(uint64_t freq)
{
    if (s_latencyMs >= 0.f)
        return (uint64_t)(s_latencyMs * 0.001 * (double)freq);
    // Default: half a 60 Hz sim period → pair usually available.
    return freq / 120ull;
}

void NoteAlpha(float a)
{
    s_st.alphaLast = a;
    if (a < s_st.alphaMin) s_st.alphaMin = a;
    if (a > s_st.alphaMax) s_st.alphaMax = a;
}

bool TryUniqueMid(float alpha, uint64_t targetTicks)
{
    // Publish time-correct pose before replay so LoadIndexedXF sees ViewPos'.
    float builtAlpha = alpha;
    bool poseOk = false;
    if (PoseInterp_Enabled())
    {
        poseOk = PoseInterp_BuildForHostTime(targetTicks, &builtAlpha);
        if (!poseOk)
            poseOk = PoseInterp_BuildAtAlpha(alpha);
        if (poseOk)
        {
            s_st.timeAlphaOk++;
            NoteAlpha(builtAlpha);
            alpha = builtAlpha;
        }
        else
        {
            s_st.timeAlphaFail++;
        }
    }

    uint32_t hash = 0, draws = 0;
    char detail[192]{};
    if (!A2b_FifoCaptureEnabled())
    {
        std::snprintf(detail, sizeof(detail), "no A2b archive (set SCR_A2B_FIFO_CAPTURE=1)");
        std::lock_guard<std::mutex> lk(s_mu);
        std::snprintf(s_lastDetail, sizeof(s_lastDetail), "%s", detail);
        s_st.midFail++;
        if (TraceOn() && (s_logN < 12 || (s_logN % 120) == 0))
            LOG_INFO("DecoupledPresent mid fail α={:.3f} pose={} {}", alpha, poseOk ? 1 : 0,
                     detail);
        s_logN++;
        return false;
    }
    // Hold GPU idle for scratch replay + present.
    auto gpuLock = GX_SyncGPU();
    const bool ok = A2c_DecoupledMidPresent(alpha, &hash, &draws, detail, sizeof(detail));
    (void)gpuLock;
    {
        std::lock_guard<std::mutex> lk(s_mu);
        std::snprintf(s_lastDetail, sizeof(s_lastDetail), "%s", detail);
        if (ok)
        {
            s_st.uniqueMid++;
            s_st.lastMidHash = hash;
            if (s_st.lastGuestHash && hash != s_st.lastGuestHash)
                s_st.distinctMidVsGuest++;
        }
        else
            s_st.midFail++;
    }
    if (TraceOn() && (s_logN < 12 || (s_logN % 120) == 0))
        LOG_INFO("DecoupledPresent mid {} α={:.3f} pose={} {}", ok ? "UNIQUE" : "fail", alpha,
                 poseOk ? 1 : 0, detail);
    s_logN++;
    return ok;
}

}  // namespace

bool DecoupledPresent_Enabled() { return On(); }

float DecoupledPresent_TargetRate()
{
    On();
    return s_rate;
}

void DecoupledPresent_OnXfb()
{
    if (!On()) return;
    s_lastSimTicks = Platform_PerfCounter();
    s_midsThisSim = 0;
    s_st.simTicks++;
    // Guest unique XFB hash is not always available here; leave lastGuestHash for
    // mid path comparison when scratch hash differs.
}

void DecoupledPresent_OnPaceWait(uint64_t nowTicks, uint64_t simDeadlineTicks, uint64_t freq)
{
    if (!On() || !freq) return;
    s_st.scheduleAttempts++;

    // How many unique mids still owed this sim interval (rate-1 besides the guest XFB).
    const uint32_t midsWanted = (s_rate > 1.f) ? (uint32_t)(s_rate - 0.5f) : 0u;
    if (s_midsThisSim >= midsWanted)
    {
        // Pacing only: may re-present last XFB (counted as duplicate, not unique).
        Renderer_RepeatLastXfb();
        s_st.duplicatePace++;
        return;
    }

    const uint64_t lat = LatencyTicks(freq);
    const uint64_t target = (nowTicks > lat) ? (nowTicks - lat) : 0;
    // Spread mids evenly across the remaining wait window.
    const uint64_t period = (midsWanted > 0) ? ((freq / 60ull) / (midsWanted + 1ull)) : (freq / 120ull);
    if (s_lastPresentTicks && nowTicks < s_lastPresentTicks + period)
    {
        s_st.missedDeadline++;  // early — wait for spacing
        return;
    }

    float alpha = 0.5f;
    PoseSnap prev{}, curr{};
    if (PoseCapture_GetBracket(target, prev, curr, &alpha))
        NoteAlpha(alpha);
    else if (PoseCapture_GetLatest(curr) && PoseCapture_GetPrevious(prev) &&
             curr.hostTicks > prev.hostTicks)
    {
        // Unbracketed target: fail closed for time-alpha; still try mid α=0.5 debug.
        alpha = 0.5f;
        s_st.timeAlphaFail++;
    }

    if (TryUniqueMid(alpha, target))
    {
        s_midsThisSim++;
        s_lastPresentTicks = nowTicks;
        s_st.latencyMs = (float)(lat * 1000.0 / (double)freq);
    }
    else
    {
        // No unique mid available — pace with duplicate present (not counted unique).
        Renderer_RepeatLastXfb();
        s_st.duplicatePace++;
        s_lastPresentTicks = nowTicks;
        if (nowTicks + period > simDeadlineTicks)
            s_st.missedDeadline++;
    }
    (void)simDeadlineTicks;
}

void DecoupledPresent_GetStats(DecoupledPresentStats& out)
{
    std::lock_guard<std::mutex> lk(s_mu);
    out = s_st;
    out.targetRate = s_rate;
}

void DecoupledPresent_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!On()) return;
    DecoupledPresentStats st{};
    DecoupledPresent_GetStats(st);
    std::snprintf(buf, bufSize, "Decoup mid=%llu dup=%llu fail=%llu α=%.2f",
                  (unsigned long long)st.uniqueMid, (unsigned long long)st.duplicatePace,
                  (unsigned long long)st.midFail, st.alphaLast);
}
