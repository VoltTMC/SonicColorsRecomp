// Shared SDL3 gamepad → XInput-layout PadState (Linux + Windows/Wine).
#include "sdl_gamepad.h"
#include "../input/gamepad.h"
#include "../input/input_latency.h"
#include "../log.h"
#include "../ui/overlay.h"

#include <atomic>
#include <format>
#include <mutex>
#include <string>

namespace
{

std::mutex s_mu;
SDL_Gamepad* s_pads[4]{};
bool s_inited = false;
std::atomic<uint64_t> s_acquireSeq{ 0 };
// Last acquire seq per packed slot (for InputLatency_OnPublish correlation).
uint64_t s_lastSeq[4]{};

void CompactPads()
{
    int w = 0;
    for (int r = 0; r < 4; r++)
        if (s_pads[r]) s_pads[w++] = s_pads[r];
    while (w < 4) s_pads[w++] = nullptr;
}

void AddPad(SDL_JoystickID id, bool toast)
{
    std::lock_guard lk(s_mu);
    for (SDL_Gamepad* p : s_pads)
        if (p && SDL_GetGamepadID(p) == id) return;
    for (int i = 0; i < 4; i++)
    {
        if (s_pads[i]) continue;
        s_pads[i] = SDL_OpenGamepad(id);
        if (!s_pads[i])
        {
            LOG_WARN("input: SDL_OpenGamepad({}) failed: {}", (int)id, SDL_GetError());
            return;
        }
        const char* name = SDL_GetGamepadName(s_pads[i]);
        LOG_INFO("controller {} connected: {}", i + 1, name ? name : "?");
        if (toast)
        {
            std::string msg = std::format("Controller {} connected{}", i + 1,
                                          name && name[0] ? std::format(" ({})", name) : "");
            Overlay_Toast(msg.c_str());
        }
        return;
    }
    LOG_WARN("controller ignored (4 already connected)");
}

void RemovePad(SDL_JoystickID id)
{
    std::lock_guard lk(s_mu);
    bool removed = false;
    int slot = -1;
    for (int i = 0; i < 4; i++)
    {
        if (!s_pads[i] || SDL_GetGamepadID(s_pads[i]) != id) continue;
        LOG_INFO("controller {} disconnected", i + 1);
        SDL_CloseGamepad(s_pads[i]);
        s_pads[i] = nullptr;
        removed = true;
        slot = i;
        break;
    }
    if (!removed) return;
    CompactPads();
    if (slot == 0)
    {
        LOG_INFO("controller: player 1 pad gone — keyboard/mouse still control player 1");
        Overlay_Toast("Controller 1 disconnected — keyboard/mouse active");
    }
    else if (slot > 0)
        Overlay_Toast(std::format("Controller {} disconnected", slot + 1).c_str());
}

int16_t Axis(SDL_Gamepad* p, SDL_GamepadAxis a, bool invert)
{
    int v = SDL_GetGamepadAxis(p, a);
    if (invert) v = -v - 1;  // SDL: down +, XInput: up +
    return (int16_t)v;
}

}  // namespace

void SdlGamepad_ApplyHints()
{
    // Must run before SDL_Init(GAMEPAD). Prefer HIDAPI for DualShock / DualSense;
    // Xbox pads still come through SDL's XInput backend.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
}

void SdlGamepad_Init()
{
    s_inited = true;
    SdlGamepad_ProbeConnected(false);
}

void SdlGamepad_Shutdown()
{
    std::lock_guard lk(s_mu);
    for (SDL_Gamepad*& p : s_pads)
    {
        if (p)
        {
            SDL_CloseGamepad(p);
            p = nullptr;
        }
    }
    s_inited = false;
}

void SdlGamepad_HandleEvent(const SDL_Event& e)
{
    if (e.type == SDL_EVENT_GAMEPAD_ADDED) AddPad(e.gdevice.which, true);
    else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) RemovePad(e.gdevice.which);
}

void SdlGamepad_ProbeConnected(bool toast)
{
    int n = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&n);
    if (!ids)
    {
        if (s_inited)
            LOG_INFO("input: SDL_GetGamepads → 0 ({})", SDL_GetError());
        return;
    }
    LOG_INFO("input: SDL reports {} gamepad(s)", n);
    for (int i = 0; i < n; i++)
        AddPad(ids[i], toast);
    SDL_free(ids);
}

void SdlGamepad_PumpHotplug()
{
    if (!s_inited) return;
    SDL_PumpEvents();
    SDL_Event e;
    while (SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_EVENT_GAMEPAD_ADDED, SDL_EVENT_GAMEPAD_REMOVED) > 0)
        SdlGamepad_HandleEvent(e);
    // Also accept joystick add that SDL promotes to gamepad.
    while (SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_EVENT_JOYSTICK_ADDED, SDL_EVENT_JOYSTICK_REMOVED) > 0)
    {
        if (e.type == SDL_EVENT_JOYSTICK_ADDED && SDL_IsGamepad(e.jdevice.which))
            AddPad(e.jdevice.which, true);
        else if (e.type == SDL_EVENT_JOYSTICK_REMOVED)
            RemovePad(e.jdevice.which);
    }
}

int SdlGamepad_Count()
{
    std::lock_guard lk(s_mu);
    int n = 0;
    for (SDL_Gamepad* p : s_pads)
        if (p) n++;
    return n;
}

const char* SdlGamepad_Name(int index)
{
    std::lock_guard lk(s_mu);
    SDL_Gamepad* p = index >= 0 && index < 4 ? s_pads[index] : nullptr;
    if (!p) return nullptr;
    const char* n = SDL_GetGamepadName(p);
    return n && n[0] ? n : "Gamepad";
}

bool SdlGamepad_Read(int index, PadState& out)
{
    // SDL caches joystick/gamepad state until SDL_UpdateJoysticks() (or PumpEvents).
    // Windows Input_PollHotplug throttles PumpEvents to ~4 Hz for hotplug only — without
    // an explicit UpdateJoysticks here, XInput/HIDAPI pads stay stale up to ~250 ms
    // (regression from 4205982). Linux window loop pumps every frame; this call is still
    // correct and cheap (XInputGetState per open pad).
    SDL_UpdateJoysticks();

    std::lock_guard lk(s_mu);
    SDL_Gamepad* p = index >= 0 && index < 4 ? s_pads[index] : nullptr;
    if (!p) return false;
    static const struct
    {
        SDL_GamepadButton b;
        uint16_t bit;
    } kButtons[] = {
        { SDL_GAMEPAD_BUTTON_DPAD_UP, PAD_DPAD_UP },
        { SDL_GAMEPAD_BUTTON_DPAD_DOWN, PAD_DPAD_DOWN },
        { SDL_GAMEPAD_BUTTON_DPAD_LEFT, PAD_DPAD_LEFT },
        { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, PAD_DPAD_RIGHT },
        { SDL_GAMEPAD_BUTTON_START, PAD_START },
        { SDL_GAMEPAD_BUTTON_BACK, PAD_BACK },
        { SDL_GAMEPAD_BUTTON_LEFT_STICK, PAD_LTHUMB },
        { SDL_GAMEPAD_BUTTON_RIGHT_STICK, PAD_RTHUMB },
        { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, PAD_LB },
        { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, PAD_RB },
        { SDL_GAMEPAD_BUTTON_SOUTH, PAD_A },
        { SDL_GAMEPAD_BUTTON_EAST, PAD_B },
        { SDL_GAMEPAD_BUTTON_WEST, PAD_X },
        { SDL_GAMEPAD_BUTTON_NORTH, PAD_Y },
    };
    out = PadState();
    for (auto& k : kButtons)
        if (SDL_GetGamepadButton(p, k.b)) out.buttons |= k.bit;
    out.lx = Axis(p, SDL_GAMEPAD_AXIS_LEFTX, false);
    out.ly = Axis(p, SDL_GAMEPAD_AXIS_LEFTY, true);
    out.rx = Axis(p, SDL_GAMEPAD_AXIS_RIGHTX, false);
    out.ry = Axis(p, SDL_GAMEPAD_AXIS_RIGHTY, true);
    out.lt = (uint8_t)(SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) * 255 / 32767);
    out.rt = (uint8_t)(SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) * 255 / 32767);

    const uint64_t seq = ++s_acquireSeq;
    if (index >= 0 && index < 4) s_lastSeq[index] = seq;
    InputLatency_OnAcquire(index, out.buttons, out.lx, out.ly, out.lt, out.rt, seq);
    return true;
}

uint64_t SdlGamepad_LastAcquireSeq(int index)
{
    if (index < 0 || index >= 4) return 0;
    std::lock_guard lk(s_mu);
    return s_lastSeq[index];
}

void SdlGamepad_SetRumble(int index, uint16_t low, uint16_t high)
{
    std::lock_guard lk(s_mu);
    SDL_Gamepad* p = index >= 0 && index < 4 ? s_pads[index] : nullptr;
    if (p) SDL_RumbleGamepad(p, low, high, low || high ? 0xFFFF : 0);
}
