// Controllers (dynamic XInput + packed player slots), keyboard and mouse on Windows.
//
// XInput user indices are not player slots: Windows may assign the only pad to user 1–3.
// We pack connected users into slots 0..n-1 so player 1 always receives the first pad.
// Link XInput by LoadLibrary (xinput1_4 → 1_3 → 9_1_0) — never hard-depend on XINPUT1_3.dll.
#include "../input.h"
#include "../../input/gamepad.h"
#include "../../log.h"
#include "../../ui/overlay.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <windows.h>
#include <xinput.h>  // types only; DLL is LoadLibrary'd (not linked)

#ifndef XINPUT_GAMEPAD_TRIGGER_THRESHOLD
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD 30
#endif

namespace
{

using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
using XInputSetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);
using XInputGetCapabilitiesFn = DWORD(WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*);

HMODULE s_xinput = nullptr;
XInputGetStateFn s_getState = nullptr;
XInputSetStateFn s_setState = nullptr;
XInputGetCapabilitiesFn s_getCaps = nullptr;
const char* s_xinputDllName = nullptr;

std::mutex s_mu;
// Player slot → XInput user index (0–3), or -1 if empty.
int s_slotUser[4] = { -1, -1, -1, -1 };
bool s_slotOn[4] = {};
uint64_t s_lastMapMs = 0;
std::atomic<int> s_diag{ -1 };

bool DiagOn()
{
    int v = s_diag.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_INPUT_DIAG");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_diag.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("input: SCR_INPUT_DIAG=1 (throttled Windows controller diagnostics)");
    }
    return v != 0;
}

bool EnsureXInput()
{
    if (s_getState) return true;
    static const char* kDlls[] = {
        "xinput1_4.dll",   // Windows 8+
        "xinput1_3.dll",   // DirectX End-User Runtime
        "xinput9_1_0.dll", // Vista/7 inbox
        "xinput1_2.dll",
        "xinput1_1.dll",
    };
    for (const char* name : kDlls)
    {
        HMODULE dll = LoadLibraryA(name);
        if (!dll) continue;
        auto getState = (XInputGetStateFn)(void*)GetProcAddress(dll, "XInputGetState");
        auto setState = (XInputSetStateFn)(void*)GetProcAddress(dll, "XInputSetState");
        auto getCaps = (XInputGetCapabilitiesFn)(void*)GetProcAddress(dll, "XInputGetCapabilities");
        if (!getState)
        {
            FreeLibrary(dll);
            continue;
        }
        s_xinput = dll;
        s_getState = getState;
        s_setState = setState;
        s_getCaps = getCaps;
        s_xinputDllName = name;
        LOG_INFO("input: XInput loaded from {} (GetState OK, SetState={}, Caps={})", name,
                 s_setState ? "yes" : "no", s_getCaps ? "yes" : "no");
        return true;
    }
    static bool once = false;
    if (!once)
    {
        once = true;
        LOG_WARN("input: no XInput DLL found (tried xinput1_4/1_3/9_1_0/…); controllers unavailable");
    }
    return false;
}

DWORD XiGetState(DWORD user, XINPUT_STATE& st)
{
    if (!EnsureXInput() || !s_getState) return ERROR_DEVICE_NOT_CONNECTED;
    return s_getState(user, &st);
}

void RemapSlotsLocked(bool toast)
{
    bool on[4] = {};
    for (DWORD u = 0; u < 4; u++)
    {
        XINPUT_STATE st{};
        on[u] = (XiGetState(u, st) == ERROR_SUCCESS);
    }

    int prevUser[4];
    bool prevOn[4];
    for (int i = 0; i < 4; i++)
    {
        prevUser[i] = s_slotUser[i];
        prevOn[i] = s_slotOn[i];
    }

    // Pack connected XInput users into player slots 0..n-1 (stable by ascending user index).
    int w = 0;
    for (DWORD u = 0; u < 4; u++)
    {
        if (!on[u]) continue;
        if (w < 4)
        {
            s_slotUser[w] = (int)u;
            s_slotOn[w] = true;
            w++;
        }
    }
    while (w < 4)
    {
        s_slotUser[w] = -1;
        s_slotOn[w] = false;
        w++;
    }

    if (!toast) return;

    for (int i = 0; i < 4; i++)
    {
        if (s_slotOn[i] && !prevOn[i])
        {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "Controller %d connected (XInput user %d)", i + 1,
                          s_slotUser[i]);
            Overlay_Toast(buf);
            LOG_INFO("input: player {} ← XInput user {} ({})", i + 1, s_slotUser[i],
                     s_xinputDllName ? s_xinputDllName : "?");
        }
        else if (!s_slotOn[i] && prevOn[i])
        {
            char buf[96];
            if (i == 0)
                std::snprintf(buf, sizeof(buf),
                              "Controller 1 disconnected — keyboard/mouse active");
            else
                std::snprintf(buf, sizeof(buf), "Controller %d disconnected", i + 1);
            Overlay_Toast(buf);
            LOG_INFO("input: player {} disconnected (was XInput user {})", i + 1, prevUser[i]);
        }
        else if (s_slotOn[i] && prevOn[i] && s_slotUser[i] != prevUser[i])
        {
            LOG_INFO("input: player {} remapped XInput user {} → {}", i + 1, prevUser[i],
                     s_slotUser[i]);
        }
    }
}

void RefreshMap(bool force, bool toast)
{
    // XInputGetState on empty slots is cheap enough at 4 Hz for hotplug; avoid every frame.
    const uint64_t now = GetTickCount64();
    std::lock_guard lk(s_mu);
    if (!force && s_lastMapMs && (now - s_lastMapMs) < 250) return;
    s_lastMapMs = now;
    RemapSlotsLocked(toast);
}

int SlotUserLocked(int index)
{
    if (index < 0 || index > 3) return -1;
    return s_slotUser[index];
}

void DiagSample(int index, const PadState& st, bool ok)
{
    if (!DiagOn()) return;
    static uint64_t last[4]{};
    static uint16_t lastButtons[4]{};
    const uint64_t now = GetTickCount64();
    const bool edge = ok && st.buttons != lastButtons[index];
    if (!edge && last[index] && (now - last[index]) < 2000) return;
    last[index] = now;
    if (ok) lastButtons[index] = st.buttons;
    int user = -1;
    {
        std::lock_guard lk(s_mu);
        user = SlotUserLocked(index);
    }
    LOG_INFO("input diag: slot={} user={} ok={} dll={} buttons={:04x} lx={} ly={} rx={} ry={} "
             "lt={} rt={}",
             index, user, ok ? 1 : 0, s_xinputDllName ? s_xinputDllName : "none",
             ok ? st.buttons : 0, ok ? st.lx : 0, ok ? st.ly : 0, ok ? st.rx : 0, ok ? st.ry : 0,
             ok ? st.lt : 0, ok ? st.rt : 0);
}

}  // namespace

bool Pad_Read(int index, PadState& out)
{
    out = PadState();
    RefreshMap(false, false);
    int user = -1;
    {
        std::lock_guard lk(s_mu);
        user = SlotUserLocked(index);
    }
    if (user < 0) return false;
    XINPUT_STATE st{};
    if (XiGetState((DWORD)user, st) != ERROR_SUCCESS)
    {
        RefreshMap(true, true);
        return false;
    }
    out.buttons = st.Gamepad.wButtons;
    out.lx = st.Gamepad.sThumbLX;
    out.ly = st.Gamepad.sThumbLY;
    out.rx = st.Gamepad.sThumbRX;
    out.ry = st.Gamepad.sThumbRY;
    out.lt = st.Gamepad.bLeftTrigger;
    out.rt = st.Gamepad.bRightTrigger;
    DiagSample(index, out, true);
    return true;
}

void Input_PollHotplug()
{
    static bool primed = false;
    RefreshMap(false, primed);
    primed = true;
}

void Pad_SetRumble(int index, uint16_t low, uint16_t high)
{
    RefreshMap(false, false);
    int user = -1;
    {
        std::lock_guard lk(s_mu);
        user = SlotUserLocked(index);
    }
    if (user < 0 || !EnsureXInput() || !s_setState) return;
    XINPUT_VIBRATION v{};
    v.wLeftMotorSpeed = low;
    v.wRightMotorSpeed = high;
    s_setState((DWORD)user, &v);
}

int Pad_Count()
{
    RefreshMap(false, false);
    std::lock_guard lk(s_mu);
    int n = 0;
    for (bool on : s_slotOn)
        if (on) n++;
    return n;
}

const char* Pad_Name(int index)
{
    RefreshMap(false, false);
    std::lock_guard lk(s_mu);
    if (index < 0 || index > 3 || !s_slotOn[index]) return nullptr;
    // XInput does not expose product names without WGI/RawInput; keep stable label + user.
    static thread_local char buf[48];
    std::snprintf(buf, sizeof(buf), "XInput controller (user %d)", s_slotUser[index]);
    return buf;
}

void Input_EnsurePadsProbed()
{
    EnsureXInput();
    RefreshMap(true, false);
}

bool Key_Down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

// ---------------------------------------------------------------- ImGui gamepad feed (launcher / overlay)
// ImGui_ImplWin32 only polls XInput user 0. Feed packed slot 0 so launcher nav matches gameplay.
#include <imgui.h>

void Ui_FeedGamepadFromPads()
{
    ImGuiIO& io = ImGui::GetIO();
    PadState st{};
    const bool ok = Pad_Read(0, st);
    io.BackendFlags = ok ? (io.BackendFlags | ImGuiBackendFlags_HasGamepad)
                         : (io.BackendFlags & ~ImGuiBackendFlags_HasGamepad);
    if (!ok) return;

    auto btn = [&](ImGuiKey key, uint16_t bit) { io.AddKeyEvent(key, (st.buttons & bit) != 0); };
    btn(ImGuiKey_GamepadStart, PAD_START);
    btn(ImGuiKey_GamepadBack, PAD_BACK);
    btn(ImGuiKey_GamepadFaceDown, PAD_A);
    btn(ImGuiKey_GamepadFaceRight, PAD_B);
    btn(ImGuiKey_GamepadFaceLeft, PAD_X);
    btn(ImGuiKey_GamepadFaceUp, PAD_Y);
    btn(ImGuiKey_GamepadDpadUp, PAD_DPAD_UP);
    btn(ImGuiKey_GamepadDpadDown, PAD_DPAD_DOWN);
    btn(ImGuiKey_GamepadDpadLeft, PAD_DPAD_LEFT);
    btn(ImGuiKey_GamepadDpadRight, PAD_DPAD_RIGHT);
    btn(ImGuiKey_GamepadL1, PAD_LB);
    btn(ImGuiKey_GamepadR1, PAD_RB);
    btn(ImGuiKey_GamepadL3, PAD_LTHUMB);
    btn(ImGuiKey_GamepadR3, PAD_RTHUMB);

    auto analog = [&](ImGuiKey key, float v) {
        v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
        io.AddKeyAnalogEvent(key, v > 0.1f, v);
    };
    analog(ImGuiKey_GamepadL2, st.lt / 255.f);
    analog(ImGuiKey_GamepadR2, st.rt / 255.f);

    constexpr float dead = 7849.f / 32767.f;
    auto stick = [&](ImGuiKey neg, ImGuiKey pos, float v) {
        float a = 0.f, b = 0.f;
        if (v < -dead) a = (-v - dead) / (1.f - dead);
        if (v > dead) b = (v - dead) / (1.f - dead);
        analog(neg, a);
        analog(pos, b);
    };
    const float lx = st.lx / 32767.f;
    const float ly = st.ly / 32767.f;
    const float rx = st.rx / 32767.f;
    const float ry = st.ry / 32767.f;
    stick(ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, lx);
    stick(ImGuiKey_GamepadLStickDown, ImGuiKey_GamepadLStickUp, ly);
    stick(ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight, rx);
    stick(ImGuiKey_GamepadRStickDown, ImGuiKey_GamepadRStickUp, ry);
}
