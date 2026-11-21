// Controllers (SDL3 gamepads — Xbox via XInput backend, DualShock/DualSense via HIDAPI),
// keyboard and mouse on Windows / Wine.
#include "../input.h"
#include "../sdl_gamepad.h"
#include "../../input/gamepad.h"
#include "../../log.h"

#include <imgui.h>
#include <windows.h>
#include <SDL3/SDL.h>

namespace
{
bool s_sdlReady = false;
}

static void EnsureSdlGamepad()
{
    if (s_sdlReady) return;
    // Gamepad-only: Win32 owns the window; do not init SDL video.
    SdlGamepad_ApplyHints();
    if (!SDL_Init(SDL_INIT_GAMEPAD))
    {
        LOG_WARN("input: SDL_Init(GAMEPAD) failed: {}", SDL_GetError());
        return;
    }
    SdlGamepad_Init();
    s_sdlReady = true;
    LOG_INFO("input: SDL3 gamepad backend ready (HIDAPI PS4/PS5 + XInput Xbox)");
}

bool Pad_Read(int index, PadState& out)
{
    EnsureSdlGamepad();
    return s_sdlReady && SdlGamepad_Read(index, out);
}

void Input_PollHotplug()
{
    EnsureSdlGamepad();
    if (s_sdlReady) SdlGamepad_PumpHotplug();
}

void Pad_SetRumble(int index, uint16_t low, uint16_t high)
{
    EnsureSdlGamepad();
    if (s_sdlReady) SdlGamepad_SetRumble(index, low, high);
}

int Pad_Count()
{
    EnsureSdlGamepad();
    return s_sdlReady ? SdlGamepad_Count() : 0;
}

const char* Pad_Name(int index)
{
    EnsureSdlGamepad();
    return s_sdlReady ? SdlGamepad_Name(index) : nullptr;
}

void Input_EnsurePadsProbed()
{
    EnsureSdlGamepad();
    if (s_sdlReady) SdlGamepad_ProbeConnected(false);
}

bool Key_Down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

void Ui_FeedGamepadFromPads()
{
    EnsureSdlGamepad();
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
    stick(ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, st.lx / 32767.f);
    stick(ImGuiKey_GamepadLStickDown, ImGuiKey_GamepadLStickUp, st.ly / 32767.f);
    stick(ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight, st.rx / 32767.f);
    stick(ImGuiKey_GamepadRStickDown, ImGuiKey_GamepadRStickUp, st.ry / 32767.f);
}
