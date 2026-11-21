// Phase 9: transition-based diagnostics for White Wisp boost (fn_800888CC / sn_boost).
#include "../game_addrs.h"
#include "../input/gamepad.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../platform/platform.h"
#include "a1b_pose_capture.h"

#include <cmath>
#include <cstring>

static bool s_boostDebug = false;

void BoostDebug_SetEnabled(bool on) { s_boostDebug = on; }
bool BoostDebug_Enabled() { return s_boostDebug; }

static float ReadF32(uint32_t addr)
{
    uint32_t u = Read32(addr);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

// Guest fn_800888CC (vtable @ 8075e0a4): enter boost; loads sn_boost @ 0x80714bbc via 8020E3CC.
// Layout: +0x08 type name; +0x14/+0x18 scratch floats rewritten on enter; +0x1C state (0/1).
// Persistent gauge lives on the player / component id 0x19 (fn_80033920), not on these scratch fields.
PPC_OVERRIDE(BoostPluginSnBoost)
{
    const uint32_t self = ctx.r[3];
    const uint32_t player = ctx.r[4];
    if (A1b_PoseCaptureEnabled())
        A1b_CachePlayerShell(player);
    float f14 = 0.f, f18 = 0.f;
    uint32_t u04 = 0, u08 = 0, u0c = 0, u10 = 0, st1c = 0;
    if (self && (self >> 28) == 0x8)
    {
        u04 = Read32(self + 0x04);
        u08 = Read32(self + 0x08);
        u0c = Read32(self + 0x0C);
        u10 = Read32(self + 0x10);
        f14 = ReadF32(self + 0x14);
        f18 = ReadF32(self + 0x18);
        st1c = Read32(self + 0x1C);
    }

    GamepadState pad{};
    Gamepad_Poll(0, pad);
    const bool bHeld = (pad.buttons & PAD_B) != 0;

    static bool lastB = false;
    static uint32_t lastSelf = 0;
    static uint64_t lastLogMs = 0;
    static int dumpLeft = 6;

    const bool bEdge = bHeld != lastB;
    const bool newObj = lastSelf != self;
    const uint64_t now = Platform_TickMs();
    const bool pulse = bHeld && (now - lastLogMs) > 500;

    if (s_boostDebug && (bEdge || newObj || pulse || dumpLeft > 0))
    {
        LOG_INFO("boost:enter self={:08x} player={:08x} type={:08x} u04={:08x} u0c={:08x} u10={:08x} "
                 "st1c={} scratch+14={:.4f} +18={:.4f} B={} LY={:.2f}{}",
                 self, player, u08, u04, u0c, u10, st1c, f14, f18, bHeld ? 1 : 0, pad.ly,
                 bEdge ? (bHeld ? " press" : " release") : (newObj ? " obj" : " hold"));
        lastLogMs = now;
        if (dumpLeft > 0) dumpLeft--;
    }

    lastB = bHeld;
    lastSelf = self;

    PPC_ORIGINAL(BoostPluginSnBoost);
}

// Guest fn_80088CB8 (vtable @ 8075e0a8): boost tick; dispatches on +0x1C then fn_8008926C.
PPC_OVERRIDE(BoostPluginTick)
{
    const uint32_t self = ctx.r[3];
    const uint32_t player = ctx.r[4];
    // A1b: BoostPluginTick r4 is the live player shell during stage (fn_80088CB8).
    if (A1b_PoseCaptureEnabled())
        A1b_CachePlayerShell(player);
    uint32_t st1c = 0;
    float f14 = 0.f, f18 = 0.f;
    if (self && (self >> 28) == 0x8)
    {
        st1c = Read32(self + 0x1C);
        f14 = ReadF32(self + 0x14);
        f18 = ReadF32(self + 0x18);
    }

    GamepadState pad{};
    Gamepad_Poll(0, pad);

    static uint32_t lastSt = 0xFFFFFFFFu;
    static float lastF14 = 0.f, lastF18 = 0.f;
    static uint64_t lastLogMs = 0;
    static bool sawTick = false;

    PPC_ORIGINAL(BoostPluginTick);

    float f14b = f14, f18b = f18;
    uint32_t st1cb = st1c;
    if (self && (self >> 28) == 0x8)
    {
        st1cb = Read32(self + 0x1C);
        f14b = ReadF32(self + 0x14);
        f18b = ReadF32(self + 0x18);
    }

    const bool stChanged = st1cb != lastSt;
    const bool fChanged =
        std::fabs(f14b - lastF14) > 0.001f || std::fabs(f18b - lastF18) > 0.001f;
    const uint64_t now = Platform_TickMs();
    const bool pulse = (pad.buttons & PAD_B) && (now - lastLogMs) > 500;

    if (s_boostDebug && (!sawTick || stChanged || fChanged || pulse))
    {
        LOG_INFO("boost:tick self={:08x} player={:08x} st1c={}->{} +14={:.4f}->{:.4f} "
                 "+18={:.4f}->{:.4f} B={} LY={:.2f}{}",
                 self, player, st1c, st1cb, f14, f14b, f18, f18b,
                 (pad.buttons & PAD_B) ? 1 : 0, pad.ly,
                 stChanged ? " state" : (fChanged ? " float" : " hold"));
        lastLogMs = now;
        sawTick = true;
    }

    lastSt = st1cb;
    lastF14 = f14b;
    lastF18 = f18b;
}
