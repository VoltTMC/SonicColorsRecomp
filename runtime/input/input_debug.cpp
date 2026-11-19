#include "input_debug.h"
#include "../log.h"

#include <array>
#include <string>

bool g_inputDebug = false;

namespace
{
struct HostSnap { uint16_t buttons = 0; bool connected = false; };
struct WiiSnap { uint16_t buttons = 0; uint8_t dev = 0xFF; };
struct PadSnap { uint16_t buttons = 0; int8_t err = -1; };

std::array<HostSnap, 4> s_host{};
std::array<WiiSnap, 4> s_wii{};
std::array<PadSnap, 4> s_pad{};

std::string ButtonsHost(uint16_t b)
{
    static const struct { uint16_t m; const char* n; } k[] = {
        { 0x0001, "UP" }, { 0x0002, "DOWN" }, { 0x0004, "LEFT" }, { 0x0008, "RIGHT" },
        { 0x0010, "START" }, { 0x0020, "BACK" }, { 0x0040, "LS" }, { 0x0080, "RS" },
        { 0x0100, "LB" }, { 0x0200, "RB" }, { 0x0400, "HOME" },
        { 0x1000, "A" }, { 0x2000, "B" }, { 0x4000, "X" }, { 0x8000, "Y" },
    };
    std::string s;
    for (auto& e : k)
        if (b & e.m) { if (!s.empty()) s += '+'; s += e.n; }
    return s.empty() ? "-" : s;
}

std::string ButtonsWpad(uint16_t b)
{
    static const struct { uint16_t m; const char* n; } k[] = {
        { 0x0001, "LEFT" }, { 0x0002, "RIGHT" }, { 0x0004, "DOWN" }, { 0x0008, "UP" },
        { 0x0010, "PLUS" }, { 0x0100, "TWO" }, { 0x0200, "ONE" }, { 0x0400, "B" },
        { 0x0800, "A" }, { 0x1000, "MINUS" }, { 0x2000, "Z" }, { 0x4000, "C" }, { 0x8000, "HOME" },
    };
    std::string s;
    for (auto& e : k)
        if (b & e.m) { if (!s.empty()) s += '+'; s += e.n; }
    return s.empty() ? "-" : s;
}

std::string ButtonsPad(uint16_t b)
{
    static const struct { uint16_t m; const char* n; } k[] = {
        { 0x0001, "LEFT" }, { 0x0002, "RIGHT" }, { 0x0004, "DOWN" }, { 0x0008, "UP" },
        { 0x0010, "Z" }, { 0x0020, "R" }, { 0x0040, "L" },
        { 0x0100, "A" }, { 0x0200, "B" }, { 0x0400, "X" }, { 0x0800, "Y" }, { 0x1000, "START" },
    };
    std::string s;
    for (auto& e : k)
        if (b & e.m) { if (!s.empty()) s += '+'; s += e.n; }
    return s.empty() ? "-" : s;
}
}   // namespace

void InputDebug_SetEnabled(bool on) { g_inputDebug = on; }

void InputDebug_Host(int chan, uint16_t buttons, bool connected, std::string_view src)
{
    if (!g_inputDebug || chan < 0 || chan > 3) return;
    auto& p = s_host[chan];
    if (p.buttons == buttons && p.connected == connected) return;
    p = { buttons, connected };
    LOG_INFO("input[{}] host {} conn={} btn={:#06x} ({})", chan, src, connected, buttons, ButtonsHost(buttons));
}

void InputDebug_Wii(int chan, uint16_t wpadButtons, uint8_t devType, std::string_view src)
{
    if (!g_inputDebug || chan < 0 || chan > 3) return;
    auto& p = s_wii[chan];
    if (p.buttons == wpadButtons && p.dev == devType) return;
    p = { wpadButtons, devType };
    LOG_INFO("input[{}] wii {} dev={} btn={:#06x} ({})", chan, src, devType, wpadButtons, ButtonsWpad(wpadButtons));
}

void InputDebug_Pad(int chan, uint16_t padButtons, int8_t err, std::string_view src)
{
    if (!g_inputDebug || chan < 0 || chan > 3) return;
    auto& p = s_pad[chan];
    if (p.buttons == padButtons && p.err == err) return;
    p = { padButtons, err };
    LOG_INFO("input[{}] pad {} err={} btn={:#06x} ({})", chan, src, err, padButtons, ButtonsPad(padButtons));
}

void InputDebug_Note(std::string_view msg)
{
    if (!g_inputDebug) return;
    LOG_INFO("input: {}", msg);
}
