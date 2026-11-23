// Approach A3o — decoupled presentation scheduler (SCR_DECOUPLED_PRESENT).
#include "decoupled_present.h"
#include "a2b_fifo_archive.h"
#include "a2c_scratch_replay.h"
#include "a3b_camera_interp.h"
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
std::atomic<int> s_midPoseTrace{ -1 };
std::mutex s_mu;
DecoupledPresentStats s_st{};
float s_rate = 2.f;
float s_latencyMs = -1.f;  // <0 → auto = 0.5 * sim period
uint64_t s_lastSimTicks = 0;
uint64_t s_lastPresentTicks = 0;
uint32_t s_midsThisSim = 0;
uint32_t s_logN = 0;
char s_lastDetail[192]{};
std::atomic<uint64_t> s_corrId{ 1 };
std::atomic<uint32_t> s_diagForceMid{ 0 };  // set on GPU/XFB thread; consumed on guest pace
constexpr uint32_t kMidTraceCap = 32;
MidPoseTrace s_midTrace[kMidTraceCap]{};
uint32_t s_midTraceN = 0;

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

bool MidPoseTraceOn()
{
    int v = s_midPoseTrace.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = (EnvFlag("SCR_MID_POSE_TRACE") || EnvFlag("SCR_MID_CAMERA_TRACE")) ? 1 : 0;
        s_midPoseTrace.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("DecoupledPresent: mid pose/camera trace on (SCR_MID_POSE_TRACE); "
                     "pose= in logs is poseOk boolean (0/1), not an override count");
    }
    return v != 0;
}

void RecordMidTrace(const MidPoseTrace& t)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (s_midTraceN < kMidTraceCap)
        s_midTrace[s_midTraceN++] = t;
    else
        s_midTrace[s_midTraceN - 1] = t;  // keep latest when full
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
    const uint64_t corr = s_corrId.fetch_add(1, std::memory_order_relaxed);
    MidPoseTrace tr{};
    tr.corrId = corr;
    tr.targetTicks = targetTicks;
    tr.alpha = alpha;

    // Publish time-correct pose before replay so LoadIndexedXF sees ViewPos'.
    float builtAlpha = alpha;
    bool poseOk = false;
    uint64_t pinPrev = 0, pinCurr = 0;
    if (PoseInterp_Enabled())
    {
        poseOk = PoseInterp_BuildForHostTime(targetTicks, &builtAlpha);
        if (!poseOk)
            poseOk = PoseInterp_BuildAtAlpha(alpha);
        tr.poseFb = (uint8_t)PoseInterp_LastFallback();
        if (poseOk)
        {
            s_st.timeAlphaOk++;
            NoteAlpha(builtAlpha);
            alpha = builtAlpha;
            tr.alpha = alpha;
            tr.poseOk = 1;
            PoseInterp_GetPublished(&tr.viewBaseEa, &tr.viewActiveEa, &tr.nodesInterp, &tr.nView,
                                    &tr.seqPrev, &tr.seqCurr, nullptr);
            pinPrev = tr.seqPrev;
            pinCurr = tr.seqCurr;
            if (pinPrev) PoseCapture_PinSeq(pinPrev);
            if (pinCurr) PoseCapture_PinSeq(pinCurr);
        }
        else
        {
            s_st.timeAlphaFail++;
        }
    }
    else
        tr.poseFb = (uint8_t)PoseInterpFb_Disabled;

    uint32_t hash = 0, draws = 0;
    uint64_t gxConsumed = 0;
    char detail[192]{};
    if (!A2b_FifoCaptureEnabled())
    {
        std::snprintf(detail, sizeof(detail), "no A2b archive (set SCR_A2B_FIFO_CAPTURE=1)");
        std::lock_guard<std::mutex> lk(s_mu);
        std::snprintf(s_lastDetail, sizeof(s_lastDetail), "%s", detail);
        s_st.midFail++;
        std::snprintf(tr.detail, sizeof(tr.detail), "%s", detail);
        if (MidPoseTraceOn() || (TraceOn() && (s_logN < 12 || (s_logN % 120) == 0)))
            LOG_INFO("DecoupledPresent mid fail id={} α={:.3f} poseOk={} fb={} {}", corr, alpha,
                     poseOk ? 1 : 0, PoseInterp_FallbackName((PoseInterpFallback)tr.poseFb),
                     detail);
        s_logN++;
        if (pinPrev) PoseCapture_UnpinSeq(pinPrev);
        if (pinCurr) PoseCapture_UnpinSeq(pinCurr);
        if (MidPoseTraceOn()) RecordMidTrace(tr);
        return false;
    }

    A2bFrameSummary sum{};
    if (A2b_GetCurrentSummary(sum)) tr.archiveXfb = (uint32_t)sum.xfbIndex;

    // Hold GPU idle for scratch replay + present.
    auto gpuLock = GX_SyncGPU();
    const bool ok =
        A2c_DecoupledMidPresent(alpha, &hash, &draws, detail, sizeof(detail), &gxConsumed);
    (void)gpuLock;

    tr.uniqueOk = ok ? 1 : 0;
    tr.draws = draws;
    tr.colorHash = hash;
    tr.gxConsumed = gxConsumed;
    std::snprintf(tr.detail, sizeof(tr.detail), "%s", detail);

    {
        std::lock_guard<std::mutex> lk(s_mu);
        std::snprintf(s_lastDetail, sizeof(s_lastDetail), "%s", detail);
        s_st.lastGxConsumed = (uint32_t)gxConsumed;
        s_st.lastNodesInterp = tr.nodesInterp;
        if (ok)
        {
            s_st.uniqueMid++;
            s_st.lastMidHash = hash;
            if (poseOk) s_st.uniqueMidPoseOk++;
            if (gxConsumed > 0) s_st.uniqueMidPoseGx++;
            if (s_st.lastGuestHash && hash != s_st.lastGuestHash)
                s_st.distinctMidVsGuest++;
        }
        else
            s_st.midFail++;
    }

    // A3q: do not leave mid-α published for the next guest frame's early draws.
    PoseInterp_ClearPublished();
    if (pinPrev) PoseCapture_UnpinSeq(pinPrev);
    if (pinCurr) PoseCapture_UnpinSeq(pinCurr);

    const bool logIt = MidPoseTraceOn() || (TraceOn() && (s_logN < 12 || (s_logN % 120) == 0));
    if (logIt)
    {
        LOG_INFO("DecoupledPresent mid {} id={} α={:.3f} poseOk={} fb={} nodes={} "
                 "view={:08x}/{:08x} seq={}/{} gx={} archXfb={} {}",
                 ok ? "UNIQUE" : "fail", corr, alpha, poseOk ? 1 : 0,
                 PoseInterp_FallbackName((PoseInterpFallback)tr.poseFb), tr.nodesInterp,
                 tr.viewBaseEa, tr.viewActiveEa, (unsigned long long)tr.seqPrev,
                 (unsigned long long)tr.seqCurr, (unsigned long long)gxConsumed, tr.archiveXfb,
                 detail);
        if (ok && MidPoseTraceOn())
            LOG_INFO("MidCamera id={} sonic=ViewPos'(PoseInterp V̂'·World') "
                     "env=archived_sample_held a3bXF={}",
                     corr, A3b_CameraInterpEnabled() ? 1 : 0);
    }
    s_logN++;
    if (MidPoseTraceOn()) RecordMidTrace(tr);
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

    // A3q: request a sparse correctness mid on the *guest* pace path. Never call
    // TryUniqueMid/GX_SyncGPU from the XFB/GPU thread (deadlock).
    if (MidPoseTraceOn() && PoseInterp_Enabled() && A2b_FifoCaptureEnabled())
    {
        static uint32_t s_forceN = 0;
        if ((++s_forceN % 30u) == 0u)
            s_diagForceMid.store(1, std::memory_order_release);
    }
}

// Guest-thread only. Consumes SCR_MID_POSE_TRACE force-mid requests after XFB.
bool ConsumeDiagForceMid(uint64_t nowTicks, uint64_t freq)
{
    if (!s_diagForceMid.exchange(0, std::memory_order_acq_rel)) return false;
    PoseSnap prev{}, curr{};
    if (!PoseCapture_GetLatest(curr) || !PoseCapture_GetPrevious(prev))
        return false;
    const uint64_t lat = LatencyTicks(freq ? freq : 1);
    const uint64_t target = (nowTicks > lat) ? (nowTicks - lat) : 0;
    const bool ok = TryUniqueMid(0.5f, target);
    // TryUniqueMid clears publish; restore mid-α for the next guest frame's draws.
    (void)PoseInterp_BuildAtAlpha(0.5f);
    if (ok)
    {
        s_midsThisSim++;
        s_lastPresentTicks = nowTicks;
    }
    return ok;
}

void DecoupledPresent_OnGuestPaceEnter(uint64_t nowTicks, uint64_t freq)
{
    if (!On() || !freq) return;
    // When the sim overran and the wait loop never runs, still honor force-mid.
    (void)ConsumeDiagForceMid(nowTicks, freq);
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
    std::snprintf(buf, bufSize, "Decoup mid=%llu poseOk=%llu gx=%llu dup=%llu fail=%llu α=%.2f",
                  (unsigned long long)st.uniqueMid, (unsigned long long)st.uniqueMidPoseOk,
                  (unsigned long long)st.uniqueMidPoseGx, (unsigned long long)st.duplicatePace,
                  (unsigned long long)st.midFail, st.alphaLast);
}

bool DecoupledPresent_MidPoseTraceEnabled() { return MidPoseTraceOn(); }

uint32_t DecoupledPresent_MidPoseTraceCount()
{
    std::lock_guard<std::mutex> lk(s_mu);
    return s_midTraceN;
}

bool DecoupledPresent_GetMidPoseTrace(uint32_t index, MidPoseTrace& out)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (index >= s_midTraceN) return false;
    out = s_midTrace[index];
    return true;
}
