// Approach A1 — XF projection/viewport capture at each unique XFB (opt-in).
//
// SCR_A1_XF_CAPTURE=1 enables snapshots from g_gx at Perf_OnXfbCopy time.
// Zero effect on simulation, audio, or Present path when disabled.
// Does NOT produce additional unique XFBs (investigation groundwork only).
#include "a1_xf_capture.h"
#include "../gx/gx_state.h"
#include "../log.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {

bool EnvOn()
{
    static int c = -1;
    if (c < 0)
    {
        const char* v = std::getenv("SCR_A1_XF_CAPTURE");
        c = (v && v[0] && v[0] != '0') ? 1 : 0;
        if (c)
            LOG_INFO("A1: XF capture enabled (SCR_A1_XF_CAPTURE); groundwork only — no extra unique XFB");
    }
    return c != 0;
}

struct Snap
{
    float proj[7];   // XF_PROJECTION: 6 floats + orthographic type
    float view[6];   // XF_VIEWPORT
    uint64_t xfbIndex;
};

std::mutex s_mu;
Snap s_prev{}, s_curr{};
bool s_havePrev = false, s_haveCurr = false;
std::atomic<uint64_t> s_count{ 0 };
std::atomic<uint64_t> s_changed{ 0 };

void ReadProj(float out[7])
{
    for (int i = 0; i < 7; i++)
        out[i] = XFFloat(0x1000u + XF_PROJECTION + (uint32_t)i);
}

void ReadView(float out[6])
{
    for (int i = 0; i < 6; i++)
        out[i] = XFFloat(0x1000u + XF_VIEWPORT + (uint32_t)i);
}

bool Differs(const float* a, const float* b, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (std::fabs(a[i] - b[i]) > 1e-6f)
            return true;
    }
    return false;
}

}  // namespace

bool A1_XfCaptureEnabled() { return EnvOn(); }

void A1_OnXfbCopy()
{
    if (!EnvOn()) return;
    Snap s{};
    ReadProj(s.proj);
    ReadView(s.view);
    s.xfbIndex = s_count.load(std::memory_order_relaxed) + 1;

    std::lock_guard lk(s_mu);
    if (s_haveCurr)
    {
        s_prev = s_curr;
        s_havePrev = true;
        if (Differs(s_prev.proj, s.proj, 7) || Differs(s_prev.view, s.view, 6))
            s_changed.fetch_add(1, std::memory_order_relaxed);
    }
    s_curr = s;
    s_haveCurr = true;
    s_count.store(s.xfbIndex, std::memory_order_relaxed);
}

uint64_t A1_SnapCount() { return s_count.load(std::memory_order_relaxed); }

bool A1_HasPair()
{
    std::lock_guard lk(s_mu);
    return s_havePrev && s_haveCurr;
}

bool A1_GetProjectionPair(float prevOut[7], float currOut[7])
{
    std::lock_guard lk(s_mu);
    if (!s_havePrev || !s_haveCurr) return false;
    std::memcpy(prevOut, s_prev.proj, 7 * sizeof(float));
    std::memcpy(currOut, s_curr.proj, 7 * sizeof(float));
    return true;
}

bool A1_ProjectionChanged()
{
    std::lock_guard lk(s_mu);
    if (!s_havePrev || !s_haveCurr) return false;
    return Differs(s_prev.proj, s_curr.proj, 7) || Differs(s_prev.view, s_curr.view, 6);
}

void A1_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || bufSize == 0) return;
    buf[0] = 0;
    if (!EnvOn()) return;
    std::lock_guard lk(s_mu);
    if (!s_haveCurr)
    {
        std::snprintf(buf, bufSize, "A1 XF: waiting for first XFB");
        return;
    }
    const bool pair = s_havePrev;
    const bool chg = pair && (Differs(s_prev.proj, s_curr.proj, 7) || Differs(s_prev.view, s_curr.view, 6));
    std::snprintf(buf, bufSize,
                  "A1 XF snaps %llu  proj%c  vp(%.0fx%.0f)  (capture only)",
                  (unsigned long long)s_count.load(std::memory_order_relaxed),
                  chg ? '*' : (pair ? '=' : '-'),
                  s_curr.view[0] * 2.0f, s_curr.view[1] * 2.0f);
}
