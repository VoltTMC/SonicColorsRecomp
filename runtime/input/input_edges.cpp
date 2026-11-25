// I1: button transition preservation between host samples and guest polls.
#include "input_edges.h"
#include "gamepad.h"
#include "../log.h"
#include "../perf.h"
#include "../platform/input.h"
#include "../platform/platform.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

static int Popcount16(uint16_t v)
{
    int n = 0;
    for (; v; v = (uint16_t)(v & (v - 1))) n++;
    return n;
}

namespace
{

constexpr int kSlots = 4;
constexpr int kVkMax = 256;

std::mutex s_mu;
std::atomic<int> s_diag{ -1 };

uint16_t s_padLive[kSlots]{};
uint16_t s_padPrev[kSlots]{};
uint16_t s_padSticky[kSlots]{};
uint16_t s_padPublished[kSlots]{};
uint64_t s_padPubFrame[kSlots]{};
bool s_padHavePrev[kSlots]{};

// Sticky keys: must appear down for at least one Gamepad_Poll after KeyDown,
// even if released before the next GetAsyncKeyState/SDL sample.
uint8_t s_keySticky[kVkMax]{};
uint8_t s_keySeenDown[kVkMax]{}; // KeyDown observed (for diag)
uint64_t s_keyPubFrame = ~0ull;

InputEdgesStats s_stats{};

bool DiagOn()
{
    int v = s_diag.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_INPUT_I1");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_diag.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("input-i1: transition latch diagnostics enabled (SCR_INPUT_I1 / --input-i1)");
    }
    return v != 0;
}

}  // namespace

bool InputEdges_DiagEnabled() { return DiagOn(); }

void InputEdges_SetDiagEnabled(bool on)
{
    s_diag.store(on ? 1 : 0, std::memory_order_relaxed);
    if (on)
        LOG_INFO("input-i1: transition latch diagnostics enabled (--input-i1)");
}

void InputEdges_ResetAll()
{
    std::lock_guard lk(s_mu);
    for (int i = 0; i < kSlots; i++)
    {
        s_padLive[i] = s_padPrev[i] = s_padSticky[i] = s_padPublished[i] = 0;
        s_padPubFrame[i] = 0;
        s_padHavePrev[i] = false;
    }
    std::memset(s_keySticky, 0, sizeof(s_keySticky));
    std::memset(s_keySeenDown, 0, sizeof(s_keySeenDown));
    s_keyPubFrame = ~0ull;
}

void InputEdges_OnFocusChange(bool focused)
{
    if (focused) return;
    std::lock_guard lk(s_mu);
    for (int i = 0; i < kSlots; i++)
    {
        s_padLive[i] = 0;
        s_padSticky[i] = 0;
        s_padPrev[i] = 0;
        s_padHavePrev[i] = true; // next rise is a real press after focus return
        s_padPublished[i] = 0;
    }
    std::memset(s_keySticky, 0, sizeof(s_keySticky));
    s_stats.focusClears++;
    if (DiagOn())
        LOG_INFO("input-i1: focus lost — cleared sticky/held button state");
}

void InputEdges_NotePadButtons(int slot, uint16_t buttons)
{
    if (slot < 0 || slot >= kSlots) return;
    std::lock_guard lk(s_mu);
    s_stats.hostPadSamples++;
    s_padLive[slot] = buttons;
    if (s_padHavePrev[slot])
    {
        const uint16_t rising = (uint16_t)(buttons & ~s_padPrev[slot]);
        if (rising)
        {
            s_padSticky[slot] |= rising;
            s_stats.risingLatched++;
            if (DiagOn())
                LOG_INFO("input-i1: pad{} rising latched={:04x} live={:04x}", slot, rising, buttons);
        }
    }
    s_padPrev[slot] = buttons;
    s_padHavePrev[slot] = true;
}

void InputEdges_KeyDown(int vk)
{
    if (vk <= 0 || vk >= kVkMax) return;
    std::lock_guard lk(s_mu);
    s_keySticky[vk] = 1;
    s_keySeenDown[vk] = 1;
    s_stats.hostKeyDown++;
    if (DiagOn())
        LOG_INFO("input-i1: key down vk={:#x} (sticky)", vk);
}

void InputEdges_KeyUp(int vk)
{
    if (vk <= 0 || vk >= kVkMax) return;
    std::lock_guard lk(s_mu);
    // Keep sticky until publish so a down+up between polls still shows one down sample.
    s_stats.hostKeyUp++;
    if (DiagOn())
        LOG_INFO("input-i1: key up vk={:#x} sticky={}", vk, (int)s_keySticky[vk]);
}

bool InputEdges_KeyEffectiveDown(int vk)
{
    if (vk <= 0) return false;
    if (Key_Down(vk)) return true;
    if (vk >= kVkMax) return false;
    std::lock_guard lk(s_mu);
    return s_keySticky[vk] != 0;
}

void InputEdges_SamplePads()
{
    for (int i = 0; i < kSlots; i++)
    {
        PadState st{};
        if (!Pad_Read(i, st))
        {
            InputEdges_NotePadButtons(i, 0);
            continue;
        }
        InputEdges_NotePadButtons(i, st.buttons);
    }
}

uint16_t InputEdges_PublishPad(int slot, uint16_t live, uint64_t guestFrame)
{
    if (slot < 0 || slot >= kSlots) return live;
    std::lock_guard lk(s_mu);
    const uint16_t sticky = s_padSticky[slot];
    if (s_padPubFrame[slot] != guestFrame)
    {
        s_padPublished[slot] = (uint16_t)(live | sticky);
        s_padPubFrame[slot] = guestFrame;
        s_stats.publishes++;
    }
    else
    {
        // Same guest frame (WPAD then PAD, etc.): keep prior snapshot, fold new rises + live.
        s_padPublished[slot] = (uint16_t)(s_padPublished[slot] | live | sticky);
    }
    if (sticky)
    {
        s_stats.stickyConsumed += (uint64_t)Popcount16(sticky);
        if (DiagOn())
            LOG_INFO("input-i1: pad{} publish frame={} live={:04x} stickyWas={:04x} -> {:04x}",
                     slot, guestFrame, live, sticky, s_padPublished[slot]);
        s_padSticky[slot] = 0;
    }
    return s_padPublished[slot];
}

void InputEdges_PublishKeys(uint64_t guestFrame)
{
    std::lock_guard lk(s_mu);
    if (s_keyPubFrame == guestFrame) return;
    s_keyPubFrame = guestFrame;
    // Clear stickies for keys that were either still sticky (observed this frame via
    // KeyEffectiveDown) or already released — one guest sample is enough.
    for (int vk = 0; vk < kVkMax; vk++)
    {
        if (!s_keySticky[vk]) continue;
        s_keySticky[vk] = 0;
        s_stats.stickyConsumed++;
    }
}

InputEdgesStats InputEdges_Stats()
{
    std::lock_guard lk(s_mu);
    return s_stats;
}

void InputEdges_DiagOverlay(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!DiagOn()) return;
    InputEdgesStats st = InputEdges_Stats();
    std::snprintf(buf, bufSize, "i1 rise=%llu sticky=%llu pub=%llu key↓%llu key↑%llu",
                  (unsigned long long)st.risingLatched, (unsigned long long)st.stickyConsumed,
                  (unsigned long long)st.publishes, (unsigned long long)st.hostKeyDown,
                  (unsigned long long)st.hostKeyUp);
}
