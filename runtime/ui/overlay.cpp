#include "overlay.h"
#include "settings_ui.h"
#include "ui_image.h"
#include "ui_shot.h"
#include "../config.h"
#include "../env.h"
#include "../log.h"
#include "../perf.h"
#include "../platform/platform.h"
#include "../platform/ui.h"
#include "../tas/tas.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>

void App_Restart();
void Gamepad_StopRumble();

namespace
{
ID3D11Device* s_dev = nullptr;
ID3D11DeviceContext* s_ctx = nullptr;
std::atomic<bool> s_ready{ false };
std::atomic<bool> s_open{ false };
std::atomic<bool> s_homePress{ false };
// test aid: SCR_UI_SHOT_OVERLAY=<file.bmp> (legacy BKR_*) opens the overlay
// SCR_UI_SHOT_AFTER seconds (default 30) into the game, saves after SCR_UI_SHOT_HOLD (default 1)
const char* s_shotPath = nullptr;
uint64_t s_shotOpenAt = 0;
uint64_t s_shotHold = 1000;
std::mutex s_toastMu;
std::string s_toast;
uint64_t s_toastUntil = 0;
}   // namespace

void Overlay_Init(void* window, ID3D11Device* device, ID3D11DeviceContext* ctx)
{
    s_shotPath = EnvGet("SCR_UI_SHOT_OVERLAY", "BKR_UI_SHOT_OVERLAY");
    if (Tas_Active() && !s_shotPath) return;   // scripted runs drive the game; no overlay
    if (s_shotPath)
    {
        const char* after = EnvGet("SCR_UI_SHOT_AFTER", "BKR_UI_SHOT_AFTER");
        s_shotOpenAt = Platform_TickMs() + (after ? atoi(after) : 30) * 1000ull;
        if (const char* hold = EnvGet("SCR_UI_SHOT_HOLD", "BKR_UI_SHOT_HOLD"))
            s_shotHold = atoi(hold) * 1000ull;
    }
    s_dev = device;
    s_ctx = ctx;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // nothing to remember between runs
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    std::string font = Platform_UiFont();
    if (!font.empty()) io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f);
    SettingsUI_AddFallbackFonts();
    SettingsUI_ApplyStyle();
    UiBackend_Init(window);
    ImGui_ImplDX11_Init(device, ctx);
    UiImage_SetDevice(device);
    s_ready = true;
}

bool Overlay_Ready() { return s_ready; }

void Overlay_Toggle()
{
    if (!s_ready) return;
    s_open = !s_open;
    LOG_INFO("settings overlay {}", s_open ? "opened" : "closed");
}

bool Overlay_IsOpen() { return s_open; }

bool Overlay_TakeHomePress() { return s_homePress.exchange(false); }

void Overlay_Toast(const char* msg, uint32_t durationMs)
{
    if (!msg || !msg[0] || !s_ready) return;
    std::lock_guard lk(s_toastMu);
    s_toast = msg;
    s_toastUntil = Platform_TickMs() + std::max<uint32_t>(500, durationMs);
}

bool Overlay_ShotPending() { return s_shotPath && s_shotOpenAt && Platform_TickMs() >= s_shotOpenAt; }

void Overlay_Render(float w, float h)
{
    if (!s_ready) return;
    UiBackend_ReplayInput();
    if (s_shotPath && s_shotOpenAt && Platform_TickMs() >= s_shotOpenAt && !s_open) s_open = true;

    std::string toastMsg;
    {
        std::lock_guard lk(s_toastMu);
        if (!s_toast.empty() && Platform_TickMs() < s_toastUntil)
            toastMsg = s_toast;
        else if (!s_toast.empty())
            s_toast.clear();
    }
    const bool showFps = g_config.showFpsOverlay;
    const bool showHints = g_config.showInputHints;
    if (!s_open && toastMsg.empty() && !showFps && !showHints) return;

    ImGui_ImplDX11_NewFrame();
    UiBackend_NewFrame();
    ImGui::GetIO().DisplaySize = ImVec2(w, h);   // the bound target (an off-screen one for screenshots)
    // scale with the window: designed for 720p, then user ui_scale
    const float userScale = std::clamp(g_config.uiScale, 0.75f, 1.50f);
    float scale = std::max(0.75f, ImGui::GetIO().DisplaySize.y / 720.0f) * userScale;
    ImGui::GetStyle().FontScaleMain = scale;
    ImGui::NewFrame();

    if (showFps)
    {
        static float s_fpsSmooth = 0.0f;
        static float s_ftMs[120]{};
        static int s_ftN = 0, s_ftI = 0;
        static uint64_t s_lastGuest = 0, s_lastGuestMs = 0;
        static float s_guestHz = 0.0f;
        static uint64_t s_lastRssMs = 0, s_rssMb = 0;
        const float dt = ImGui::GetIO().DeltaTime;
        const float inst = (dt > 0.0001f) ? (1.0f / dt) : 0.0f;
        s_fpsSmooth = (s_fpsSmooth <= 0.0f) ? inst : (s_fpsSmooth * 0.9f + inst * 0.1f);
        const float ftMs = dt * 1000.0f;
        s_ftMs[s_ftI] = ftMs;
        s_ftI = (s_ftI + 1) % 120;
        if (s_ftN < 120) s_ftN++;
        float worst = 0.0f;
        for (int i = 0; i < s_ftN; i++) worst = std::max(worst, s_ftMs[i]);
        // Approximate 1% low: reciprocal of the slowest recent frame in the window.
        const float onePct = (worst > 0.0001f) ? (1000.0f / worst) : 0.0f;
        const uint64_t now = Platform_TickMs();
        if (now - s_lastGuestMs >= 500)
        {
            const uint64_t g = Perf_GuestFrameIndex();
            if (s_lastGuestMs && g >= s_lastGuest)
            {
                const double secs = (now - s_lastGuestMs) / 1000.0;
                if (secs > 0.0) s_guestHz = (float)((g - s_lastGuest) / secs);
            }
            s_lastGuest = g;
            s_lastGuestMs = now;
        }
        if (now - s_lastRssMs >= 1000)
        {
            s_rssMb = Platform_ProcessRssMb();
            s_lastRssMs = now;
        }
        extern int g_audioUnderruns;
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12.0f * scale, vp->WorkPos.y + 12.0f * scale), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("##scr_fps", nullptr, flags))
        {
            ImGui::Text("%.0f FPS  ·  %.1f ms  ·  1%% ~%.0f", s_fpsSmooth, ftMs, onePct);
            ImGui::Text("present %.1f ms  ·  draws/xfb %u", Perf_PresentMsSmooth(), Perf_LastXfbDraws());
            ImGui::Text("guest ~%.0f Hz  ·  %llu MB  ·  aud underrun %d", s_guestHz,
                        (unsigned long long)s_rssMb, g_audioUnderruns);
        }
        ImGui::End();
    }

    if (showHints && !s_open)
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12.0f * scale, vp->WorkPos.y + vp->WorkSize.y - 12.0f * scale),
                                ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.50f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("##scr_hints", nullptr, flags))
        {
            ImGui::Text("Jump  %s / %s", g_config.bind[0].c_str(), g_config.keys[0].c_str());
            ImGui::Text("Slash %s / %s", g_config.bind[1].c_str(), g_config.keys[1].c_str());
            ImGui::Text("Pause %s / %s", g_config.bind[6].c_str(), g_config.keys[6].c_str());
            ImGui::TextDisabled("Settings %s · View/Back", Config_HotkeyName(g_config.hotkeySettings, "F1").c_str());
        }
        ImGui::End();
    }

    if (s_open)
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 size(std::min(vp->Size.x * 0.9f, 900.0f * scale), std::min(vp->Size.y * 0.88f, 630.0f * scale));
        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        bool open = true;
        if (ImGui::Begin("Sonic Colors###overlay", &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                              ImGuiWindowFlags_NoSavedSettings))
        {
            float bottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
            ImGui::BeginChild("panel", ImVec2(0, -bottom), ImGuiChildFlags_NavFlattened);
            if (ImGui::BeginTabBar("overlayTabs"))
            {
                static int shotFrames = 0;
                bool shotMods = SettingsUI_ShotMods() && shotFrames++ < 2;
                static int selected = 0;
                int pick = SettingsUI_PadTab(selected, 3, true);
                if (SettingsUI_TabItem("Settings", 0, pick))
                {
                    selected = 0;
                    SettingsUI_Draw(SettingsHost::InGame);
                    ImGui::EndTabItem();
                }
                if (SettingsUI_TabItem("Mods", 1, pick, shotMods))
                {
                    selected = 1;
                    SettingsUI_DrawMods(SettingsHost::InGame);
                    ImGui::EndTabItem();
                }
                if (SettingsUI_TabItem("Music", 2, pick))
                {
                    selected = 2;
                    SettingsUI_DrawMusic(SettingsHost::InGame);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndChild();
            if (ImGui::Button("Close")) open = false;
            ImGui::SameLine();
            if (ImGui::Button("Wii HOME Menu"))
            {
                s_homePress = true;   // the game opens it on the next controller read
                open = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Restart game"))
                ImGui::OpenPopup("Restart game?");
            ImGui::SameLine();
            if (ImGui::Button("Quit game"))
                ImGui::OpenPopup("Quit game?");
            ImGui::SameLine();
            ImGui::TextDisabled("View / %s / Esc closes", Config_HotkeyName(g_config.hotkeySettings, "F1").c_str());
            SettingsUI_DrawVersion();

            if (ImGui::BeginPopupModal("Restart game?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped("Restart the game now? Unsaved progress in the current stage may be lost.");
                if (ImGui::Button("Restart", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                    App_Restart();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopupModal("Quit game?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped("Quit Sonic Colors Recomp? Unsaved progress in the current stage may be lost.");
                if (ImGui::Button("Quit", ImVec2(120, 0)))
                {
                    LOG_INFO("quit from the settings overlay");
                    Gamepad_StopRumble();
                    Platform_Terminate(0);
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
        if (!open) s_open = false;
        if (SettingsUI_TakeRestartRequest()) App_Restart();
    }

    if (!toastMsg.empty())
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y - 48.0f * scale),
                                ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.82f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("##scr_toast", nullptr, flags))
            ImGui::TextUnformatted(toastMsg.c_str());
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    if (s_shotPath && s_shotOpenAt && Platform_TickMs() >= s_shotOpenAt + s_shotHold)
    {
        UI_SaveBoundTarget(s_dev, s_ctx, s_shotPath);
        LOG_INFO("settings overlay screenshot saved to {}", s_shotPath);
        s_shotOpenAt = 0;
        s_open = false;
    }
}
