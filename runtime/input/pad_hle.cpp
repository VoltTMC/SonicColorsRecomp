// GameCube PAD HLE: Sonic Colors' pad manager vtable calls PADRead (80357660) for the
// GC-controller path. SI currently reports no devices, so without this override the
// game never sees GC A/D-pad while the "Current Setting" UI shows a GameCube pad.
#include "gamepad.h"
#include "input_debug.h"
#include "../game_addrs.h"
#include "../kernel/memory.h"
#include "../log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
enum : int8_t
{
    PAD_ERR_NONE = 0,
    PAD_ERR_NO_CONTROLLER = -1,
};
enum : uint16_t
{
    GC_LEFT = 0x0001, GC_RIGHT = 0x0002, GC_DOWN = 0x0004, GC_UP = 0x0008,
    GC_Z = 0x0010, GC_R = 0x0020, GC_L = 0x0040,
    GC_A = 0x0100, GC_B = 0x0200, GC_X = 0x0400, GC_Y = 0x0800, GC_START = 0x1000,
};
constexpr uint32_t SI_GC_CONTROLLER = 0x09000000;
constexpr uint32_t SI_ERROR_NO_RESPONSE = 0x08;

int8_t Stick8(float v)
{
    return (int8_t)std::clamp((int)std::lround(v * 100.0f), -100, 100);
}

uint16_t MapButtons(const GamepadState& p)
{
    uint16_t b = 0;
    if (p.buttons & PAD_DPAD_LEFT) b |= GC_LEFT;
    if (p.buttons & PAD_DPAD_RIGHT) b |= GC_RIGHT;
    if (p.buttons & PAD_DPAD_DOWN) b |= GC_DOWN;
    if (p.buttons & PAD_DPAD_UP) b |= GC_UP;
    if (p.buttons & PAD_A) b |= GC_A;
    if (p.buttons & PAD_B) b |= GC_B;
    if (p.buttons & PAD_X) b |= GC_X;
    if (p.buttons & PAD_Y) b |= GC_Y;
    if (p.buttons & PAD_START) b |= GC_START;
    if (p.buttons & PAD_RB) b |= GC_Z;
    if (p.lt > 0.5f) b |= GC_L;
    if (p.rt > 0.5f) b |= GC_R;
    return b;
}

void FillStatus(int chan, uint32_t st)
{
    // Channel 0 always present (keyboard fallback, same as WPAD HLE); 1-3 follow host pads.
    if (chan != 0 && !Gamepad_IsConnected(chan))
    {
        memset(GuestPtr(st), 0, 12);
        Write8(st + 0xA, (uint8_t)PAD_ERR_NO_CONTROLLER);
        InputDebug_Pad(chan, 0, PAD_ERR_NO_CONTROLLER, "PADRead");
        return;
    }
    GamepadState pad;
    Gamepad_Poll(chan, pad);
    uint16_t buttons = MapButtons(pad);
    Write16(st + 0x0, buttons);
    Write8(st + 0x2, (uint8_t)Stick8(pad.lx));
    Write8(st + 0x3, (uint8_t)Stick8(pad.ly));
    Write8(st + 0x4, (uint8_t)Stick8(pad.rx));
    Write8(st + 0x5, (uint8_t)Stick8(pad.ry));
    Write8(st + 0x6, (uint8_t)std::clamp((int)(pad.lt * 255.0f), 0, 255));
    Write8(st + 0x7, (uint8_t)std::clamp((int)(pad.rt * 255.0f), 0, 255));
    Write8(st + 0x8, 0);
    Write8(st + 0x9, 0);
    Write8(st + 0xA, (uint8_t)PAD_ERR_NONE);
    Write8(st + 0xB, 0);
    InputDebug_Host(chan, pad.buttons, true, "PADRead");
    InputDebug_Pad(chan, buttons, PAD_ERR_NONE, "PADRead");
}
}   // namespace

// u32 PADRead(PADStatus* status) — status is PADStatus[4], 12 bytes each
PPC_OVERRIDE(PADRead)
{
    uint32_t st = ctx.r[3];
    uint32_t mask = 0;
    if (!st) { ctx.r[3] = 0; return; }
    static bool once = false;
    if (!once) { once = true; LOG_INFO("PAD: HLE read active (GameCube controller path)"); InputDebug_Note("PADRead first call"); }
    for (int c = 0; c < 4; c++)
    {
        FillStatus(c, st + (uint32_t)c * 12u);
        if (Read8(st + (uint32_t)c * 12u + 0xA) == (uint8_t)PAD_ERR_NONE)
            mask |= 1u << (31 - c);   // SI-style channel bits (chan0 = bit31)
    }
    ctx.r[3] = mask ? mask : 0xF0000000u;   // non-zero: some pads reported
}

// u32 SIGetType(s32 chan) — report a standard GC controller on channel 0
PPC_OVERRIDE(SIGetType)
{
    int chan = (int)ctx.r[3];
    if (chan == 0)
    {
        ctx.r[3] = SI_GC_CONTROLLER;
        return;
    }
    if (chan >= 1 && chan <= 3 && Gamepad_IsConnected(chan))
    {
        ctx.r[3] = SI_GC_CONTROLLER;
        return;
    }
    ctx.r[3] = SI_ERROR_NO_RESPONSE;
}
