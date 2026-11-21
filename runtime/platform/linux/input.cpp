// Controllers (SDL3 via sdl_gamepad), keyboard and mouse on Linux.
#include "../input.h"
#include "sdl.h"
#include "../sdl_gamepad.h"

#include <SDL3/SDL.h>

void Input_HandleEvent(const SDL_Event& e) { SdlGamepad_HandleEvent(e); }

void Input_ProbeConnectedPads() { SdlGamepad_ProbeConnected(false); }

void Input_EnsurePadsProbed() { SdlGamepad_ProbeConnected(false); }

void Input_PollHotplug() {}  // SDL_EVENT_GAMEPAD_* from the window loop

int Pad_Count() { return SdlGamepad_Count(); }

const char* Pad_Name(int index) { return SdlGamepad_Name(index); }

bool Pad_Read(int index, PadState& out) { return SdlGamepad_Read(index, out); }

void Pad_SetRumble(int index, uint16_t low, uint16_t high)
{
    SdlGamepad_SetRumble(index, low, high);
}

void Ui_FeedGamepadFromPads()
{
    // SDL3 ImGui backend already publishes gamepad state; avoid duplicate events.
}

bool Key_Down(int vk)
{
    if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON || vk == VK_XBUTTON1 || vk == VK_XBUTTON2)
    {
        SDL_MouseButtonFlags b = SDL_GetMouseState(nullptr, nullptr);
        switch (vk)
        {
        case VK_LBUTTON: return b & SDL_BUTTON_LMASK;
        case VK_RBUTTON: return b & SDL_BUTTON_RMASK;
        case VK_MBUTTON: return b & SDL_BUTTON_MMASK;
        case VK_XBUTTON1: return b & SDL_BUTTON_X1MASK;
        default: return b & SDL_BUTTON_X2MASK;
        }
    }
    SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
    if (vk >= 'A' && vk <= 'Z') sc = (SDL_Scancode)(SDL_SCANCODE_A + (vk - 'A'));
    else if (vk >= '1' && vk <= '9') sc = (SDL_Scancode)(SDL_SCANCODE_1 + (vk - '1'));
    else if (vk == '0') sc = SDL_SCANCODE_0;
    else if (vk >= VK_F1 && vk <= VK_F12) sc = (SDL_Scancode)(SDL_SCANCODE_F1 + (vk - VK_F1));
    else if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9) sc = (SDL_Scancode)(SDL_SCANCODE_KP_1 + (vk - VK_NUMPAD1));
    else
        switch (vk)
        {
        case VK_NUMPAD0: sc = SDL_SCANCODE_KP_0; break;
        case VK_SPACE: sc = SDL_SCANCODE_SPACE; break;
        case VK_RETURN: sc = SDL_SCANCODE_RETURN; break;
        case VK_BACK: sc = SDL_SCANCODE_BACKSPACE; break;
        case VK_TAB: sc = SDL_SCANCODE_TAB; break;
        case VK_ESCAPE: sc = SDL_SCANCODE_ESCAPE; break;
        case VK_LSHIFT: sc = SDL_SCANCODE_LSHIFT; break;
        case VK_RSHIFT: sc = SDL_SCANCODE_RSHIFT; break;
        case VK_LCONTROL: sc = SDL_SCANCODE_LCTRL; break;
        case VK_RCONTROL: sc = SDL_SCANCODE_RCTRL; break;
        case VK_LMENU: sc = SDL_SCANCODE_LALT; break;
        case VK_RMENU: sc = SDL_SCANCODE_RALT; break;
        case VK_UP: sc = SDL_SCANCODE_UP; break;
        case VK_DOWN: sc = SDL_SCANCODE_DOWN; break;
        case VK_LEFT: sc = SDL_SCANCODE_LEFT; break;
        case VK_RIGHT: sc = SDL_SCANCODE_RIGHT; break;
        case VK_INSERT: sc = SDL_SCANCODE_INSERT; break;
        case VK_DELETE: sc = SDL_SCANCODE_DELETE; break;
        case VK_HOME: sc = SDL_SCANCODE_HOME; break;
        case VK_END: sc = SDL_SCANCODE_END; break;
        case VK_PRIOR: sc = SDL_SCANCODE_PAGEUP; break;
        case VK_NEXT: sc = SDL_SCANCODE_PAGEDOWN; break;
        }
    if (sc == SDL_SCANCODE_UNKNOWN) return false;
    int n = 0;
    const bool* keys = SDL_GetKeyboardState(&n);
    return keys && sc < n && keys[sc];
}
