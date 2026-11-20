// Controllers (XInput), keyboard and mouse on Windows.
#include "../input.h"
#include "../../ui/overlay.h"

#include <cstdio>
#include <windows.h>
#include <xinput.h>   // lowercase: Linux MinGW filesystem is case-sensitive

bool Pad_Read(int index, PadState& out)
{
    XINPUT_STATE st{};
    if (XInputGetState(index, &st) != ERROR_SUCCESS) return false;
    out.buttons = st.Gamepad.wButtons;
    out.lx = st.Gamepad.sThumbLX;
    out.ly = st.Gamepad.sThumbLY;
    out.rx = st.Gamepad.sThumbRX;
    out.ry = st.Gamepad.sThumbRY;
    out.lt = st.Gamepad.bLeftTrigger;
    out.rt = st.Gamepad.bRightTrigger;
    return true;
}

void Input_PollHotplug()
{
    static bool primed = false;
    static bool was[4] = {};
    for (int i = 0; i < 4; i++)
    {
        XINPUT_STATE st{};
        bool on = XInputGetState(i, &st) == ERROR_SUCCESS;
        if (primed)
        {
            if (on && !was[i])
            {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "Controller %d connected", i + 1);
                Overlay_Toast(buf);
            }
            else if (!on && was[i])
            {
                char buf[96];
                if (i == 0)
                    std::snprintf(buf, sizeof(buf), "Controller 1 disconnected — keyboard/mouse active");
                else
                    std::snprintf(buf, sizeof(buf), "Controller %d disconnected", i + 1);
                Overlay_Toast(buf);
            }
        }
        was[i] = on;
    }
    primed = true;
}

void Pad_SetRumble(int index, uint16_t low, uint16_t high)
{
    XINPUT_VIBRATION v{};
    v.wLeftMotorSpeed = low;
    v.wRightMotorSpeed = high;
    XInputSetState(index, &v);
}

int Pad_Count()
{
    int n = 0;
    for (int i = 0; i < 4; i++)
    {
        XINPUT_STATE st{};
        if (XInputGetState(i, &st) == ERROR_SUCCESS) n++;
    }
    return n;
}

const char* Pad_Name(int index)
{
    if (index < 0 || index > 3) return nullptr;
    XINPUT_STATE st{};
    if (XInputGetState(index, &st) != ERROR_SUCCESS) return nullptr;
    return "XInput controller";
}

bool Key_Down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
