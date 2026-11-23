#include "overlay.h"
#include "settings_ui.h"
#include "ui_image.h"
#include "ui_shot.h"
#include "../config.h"
#include "../env.h"
#include "../gx/video.h"
#include "../log.h"
#include "../perf.h"
#include "../debug/a1_xf_capture.h"
#include "../debug/a1b_pose_capture.h"
#include "../debug/a1c_gx_capture.h"
#include "../debug/a2b_fifo_archive.h"
#include "../debug/a2c_scratch_replay.h"
#include "../debug/a2g_natural_trace.h"
#include "../debug/a3a_draw_attr.h"
#include "../debug/a3b_camera_interp.h"
#include "../debug/a3c_env_capture.h"
#include "../debug/a3d_scene_coverage.h"
#include "../debug/a3e_budgets.h"
#include "../debug/a3e_player_palette.h"
#include "../debug/a3f_player_provenance.h"
#include "../debug/a3g_pose_aggregate.h"
#include "../debug/a3h_skeleton_palette.h"
#include "../debug/a3i_model_ownership.h"
#include "../debug/a3j_model_palette.h"
#include "../debug/a3k_owner_transform.h"
#include "../debug/pose_capture.h"
#include "../debug/pose_qualify.h"
#include "../debug/pose_interp.h"
#include "../debug/decoupled_present.h"
#include "../debug/a3u_player_composite.h"
#include "../input/input_latency.h"
#include "../platform/platform.h"
#include "../platform/ui.h"
#include "../tas/tas.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
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

// Compact pad / key chips for the on-screen input hints (Xbox-style face colours).
ImU32 GlyphPadColor(const char* name)
{
    if (!_stricmp(name, "A")) return IM_COL32(60, 180, 75, 255);
    if (!_stricmp(name, "B")) return IM_COL32(220, 60, 60, 255);
    if (!_stricmp(name, "X")) return IM_COL32(60, 120, 220, 255);
    if (!_stricmp(name, "Y")) return IM_COL32(230, 200, 50, 255);
    if (!_stricmp(name, "LB") || !_stricmp(name, "RB") || !_stricmp(name, "LT") || !_stricmp(name, "RT"))
        return IM_COL32(90, 95, 110, 255);
    if (!_stricmp(name, "START") || !_stricmp(name, "BACK")) return IM_COL32(70, 75, 90, 255);
    return IM_COL32(100, 105, 120, 255);
}

bool GlyphIsFace(const char* name)
{
    return !_stricmp(name, "A") || !_stricmp(name, "B") || !_stricmp(name, "X") || !_stricmp(name, "Y");
}

void DrawPadGlyph(const char* name)
{
    if (!name || !name[0] || !_stricmp(name, "NONE"))
    {
        ImGui::TextDisabled("—");
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ImGui::GetTextLineHeight();
    const ImVec2 label = ImGui::CalcTextSize(name);
    const bool face = GlyphIsFace(name);
    const float padX = face ? h * 0.35f : h * 0.28f;
    const float w = face ? h : (label.x + padX * 2.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + w, p0.y + h);
    const ImU32 fill = GlyphPadColor(name);
    if (face)
        dl->AddCircleFilled(ImVec2((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f), h * 0.48f, fill, 24);
    else
        dl->AddRectFilled(p0, p1, fill, h * 0.25f);
    const ImVec2 tp(p0.x + (w - label.x) * 0.5f, p0.y + (h - label.y) * 0.5f);
    dl->AddText(tp, IM_COL32(255, 255, 255, 255), name);
    ImGui::Dummy(ImVec2(w, h));
}

void DrawKeyGlyph(const char* keysCsv)
{
    if (!keysCsv || !keysCsv[0] || !_stricmp(keysCsv, "NONE"))
    {
        ImGui::TextDisabled("—");
        return;
    }
    // First binding only when comma-separated (keep the chip compact).
    char buf[48];
    size_t n = 0;
    for (const char* p = keysCsv; *p && *p != ',' && n + 1 < sizeof(buf); p++)
    {
        if (*p == ' ' && n == 0) continue;
        buf[n++] = (char)std::toupper((unsigned char)*p);
    }
    while (n > 0 && buf[n - 1] == ' ') n--;
    buf[n] = 0;
    if (!buf[0])
    {
        ImGui::TextDisabled("—");
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ImGui::GetTextLineHeight();
    const ImVec2 label = ImGui::CalcTextSize(buf);
    const float padX = h * 0.30f;
    const float w = label.x + padX * 2.0f;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + w, p0.y + h);
    dl->AddRectFilled(p0, p1, IM_COL32(55, 58, 70, 255), h * 0.22f);
    dl->AddRect(p0, p1, IM_COL32(160, 165, 180, 200), h * 0.22f);
    dl->AddText(ImVec2(p0.x + padX, p0.y + (h - label.y) * 0.5f), IM_COL32(235, 235, 240, 255), buf);
    ImGui::Dummy(ImVec2(w, h));
}

void HintRow(const char* action, const char* pad, const char* keys)
{
    ImGui::TextUnformatted(action);
    ImGui::SameLine(ImGui::GetFontSize() * 5.5f);
    DrawPadGlyph(pad);
    ImGui::SameLine(0.0f, ImGui::GetFontSize() * 0.35f);
    ImGui::TextDisabled("/");
    ImGui::SameLine(0.0f, ImGui::GetFontSize() * 0.35f);
    DrawKeyGlyph(keys);
}
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
    // The settings overlay is the only ImGui surface that navigates with a gamepad.
    // The FPS/hints/toast HUD is read-only; feeding SDL pad state every Present would
    // add needless per-frame driver calls while gameplay is running.
    if (s_open) Ui_FeedGamepadFromPads();
    ImGui::GetIO().DisplaySize = ImVec2(w, h);   // the bound target (an off-screen one for screenshots)
    // scale with the window: designed for 720p, then user ui_scale
    const float userScale = std::clamp(g_config.uiScale, 0.75f, 1.50f);
    float scale = std::max(0.75f, ImGui::GetIO().DisplaySize.y / 720.0f) * userScale;
    ImGui::GetStyle().FontScaleMain = scale;
    ImGui::NewFrame();

    if (showFps)
    {
        static uint64_t s_lastMs = 0;
        static uint64_t s_lastSim = 0, s_lastXfb = 0, s_lastUniquePres = 0, s_lastPresent = 0, s_lastVi = 0;
        static float s_simHz = 0.0f, s_xfbHz = 0.0f, s_uniquePresHz = 0.0f, s_presentHz = 0.0f, s_viHz = 0.0f;
        static uint64_t s_lastRssMs = 0, s_rssMb = 0;
        extern uint32_t g_viFieldCount;
        extern int g_audioUnderruns;
        const uint64_t now = Platform_TickMs();
        if (now - s_lastMs >= 500)
        {
            const uint64_t sim = Perf_GuestFrameIndex();
            const uint64_t xfb = Perf_XfbFrameIndex();
            const uint64_t uniquePres = Perf_UniqueFramesPresentedIndex();
            const uint64_t present = Perf_PresentFrameIndex();
            const uint64_t vi = g_viFieldCount;
            if (s_lastMs)
            {
                const double secs = (now - s_lastMs) / 1000.0;
                if (secs > 0.0)
                {
                    s_simHz = (float)((sim - s_lastSim) / secs);
                    s_xfbHz = (float)((xfb - s_lastXfb) / secs);
                    s_uniquePresHz = (float)((uniquePres - s_lastUniquePres) / secs);
                    s_presentHz = (float)((present - s_lastPresent) / secs);
                    s_viHz = (float)((vi - s_lastVi) / secs);
                }
            }
            s_lastSim = sim;
            s_lastXfb = xfb;
            s_lastUniquePres = uniquePres;
            s_lastPresent = present;
            s_lastVi = vi;
            s_lastMs = now;
        }
        if (now - s_lastRssMs >= 1000)
        {
            s_rssMb = Platform_ProcessRssMb();
            s_lastRssMs = now;
        }
        const float dispHz = Video_DisplayRefreshHz();
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12.0f * scale, vp->WorkPos.y + 12.0f * scale), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("##scr_fps", nullptr, flags))
        {
            // unique_frames_presented = first Present per XFB; present_calls may re-show the same XFB.
            ImGui::Text("sim %.0f  ·  unique_xfb %.0f  ·  unique_pres %.0f  ·  present_calls %.0f",
                        s_simHz, s_xfbHz, s_uniquePresHz, s_presentHz);
            if (dispHz > 0.0f)
                ImGui::Text("display_refresh %.0f Hz  ·  VSync %s%s", dispHz,
                            Video_VsyncEnabled() ? "on" : "off",
                            Video_AllowTearing() ? " (tear)" : "");
            else
                ImGui::Text("display_refresh ?  ·  VSync %s%s",
                            Video_VsyncEnabled() ? "on" : "off",
                            Video_AllowTearing() ? " (tear)" : "");
            ImGui::Text("guest_vi %.0f Hz  ·  Present %.1f ms  ·  draws/xfb %u",
                        s_viHz, Perf_PresentMsSmooth(), Perf_LastXfbDraws());
            {
                const float gpuMs = Perf_GpuMsSmooth();
                if (gpuMs > 0.0f)
                    ImGui::Text("GPU blit ~%.2f ms", gpuMs);
            }
            ImGui::Text("%llu MB  ·  aud underrun %d", (unsigned long long)s_rssMb, g_audioUnderruns);
            if (A1_XfCaptureEnabled())
            {
                char a1[128];
                A1_OverlayLine(a1, sizeof(a1));
                if (a1[0])
                    ImGui::TextUnformatted(a1);
            }
            if (A1b_PoseCaptureEnabled())
            {
                char a1b[160];
                A1b_OverlayLine(a1b, sizeof(a1b));
                if (a1b[0])
                    ImGui::TextUnformatted(a1b);
            }
            if (InputLatency_Enabled())
            {
                char inLat[160];
                InputLatency_OverlayLine(inLat, sizeof(inLat));
                if (inLat[0])
                    ImGui::TextUnformatted(inLat);
            }
            if (A1c_GxCaptureEnabled())
            {
                char a1c[160];
                A1c_OverlayLine(a1c, sizeof(a1c));
                if (a1c[0])
                    ImGui::TextUnformatted(a1c);
            }
            if (A2b_FifoCaptureEnabled())
            {
                char a2b[160];
                A2b_OverlayLine(a2b, sizeof(a2b));
                if (a2b[0])
                    ImGui::TextUnformatted(a2b);
            }
            if (A3a_DrawAttrEnabled() || A3a_MatrixOverrideEnabled())
            {
                char a3a[160];
                A3a_OverlayLine(a3a, sizeof(a3a));
                if (a3a[0])
                    ImGui::TextUnformatted(a3a);
            }
            if (A3b_CameraInterpEnabled())
            {
                char a3b[160];
                A3b_OverlayLine(a3b, sizeof(a3b));
                if (a3b[0])
                    ImGui::TextUnformatted(a3b);
            }
            if (A3c_EnvCaptureEnabled() || A3c_EnvInterpEnabled())
            {
                char a3c[160];
                A3c_OverlayLine(a3c, sizeof(a3c));
                if (a3c[0])
                    ImGui::TextUnformatted(a3c);
            }
            if (A3d_SceneCoverageEnabled())
            {
                char a3d[192];
                A3d_OverlayLine(a3d, sizeof(a3d));
                if (a3d[0])
                    ImGui::TextUnformatted(a3d);
            }
            if (A3e_DenseEnabled())
            {
                char a3e[128];
                A3e_OverlayLine(a3e, sizeof(a3e));
                if (a3e[0])
                    ImGui::TextUnformatted(a3e);
            }
            if (A3e_PlayerPaletteEnabled())
            {
                char pal[160];
                A3e_PlayerPalette_OverlayLine(pal, sizeof(pal));
                if (pal[0])
                    ImGui::TextUnformatted(pal);
            }
            if (A3f_PlayerTraceEnabled() || A3f_PaletteCaptureEnabled())
            {
                char a3f[160];
                A3f_OverlayLine(a3f, sizeof(a3f));
                if (a3f[0])
                    ImGui::TextUnformatted(a3f);
            }
            if (A3g_PoseAggregateEnabled() || A3g_HierarchyTraceEnabled())
            {
                char a3g[160];
                A3g_OverlayLine(a3g, sizeof(a3g));
                if (a3g[0])
                    ImGui::TextUnformatted(a3g);
            }
            if (A3h_SkeletonTraceEnabled() || A3h_PaletteMapEnabled())
            {
                char a3h[160];
                A3h_OverlayLine(a3h, sizeof(a3h));
                if (a3h[0])
                    ImGui::TextUnformatted(a3h);
            }
            if (A3i_AnyEnabled())
            {
                char a3i[192];
                A3i_OverlayLine(a3i, sizeof(a3i));
                if (a3i[0])
                    ImGui::TextUnformatted(a3i);
            }
            if (A3j_AnyEnabled())
            {
                char a3j[192];
                A3j_OverlayLine(a3j, sizeof(a3j));
                if (a3j[0])
                    ImGui::TextUnformatted(a3j);
            }
            if (A3k_AnyEnabled())
            {
                char a3k[192];
                A3k_OverlayLine(a3k, sizeof(a3k));
                if (a3k[0])
                    ImGui::TextUnformatted(a3k);
            }
            if (PoseCapture_Enabled())
            {
                char pose[192];
                PoseCapture_OverlayLine(pose, sizeof(pose));
                if (pose[0])
                    ImGui::TextUnformatted(pose);
            }
            if (PoseQualify_Enabled())
            {
                char pq[192];
                PoseQualify_OverlayLine(pq, sizeof(pq));
                if (pq[0])
                    ImGui::TextUnformatted(pq);
            }
            if (PoseInterp_Enabled())
            {
                char pi[192];
                PoseInterp_OverlayLine(pi, sizeof(pi));
                if (pi[0])
                    ImGui::TextUnformatted(pi);
            }
            if (DecoupledPresent_Enabled())
            {
                char dp[192];
                DecoupledPresent_OverlayLine(dp, sizeof(dp));
                if (dp[0])
                    ImGui::TextUnformatted(dp);
            }
            if (A3u_Enabled() || A3u_AuditOn())
            {
                char a3u[192];
                A3u_OverlayLine(a3u, sizeof(a3u));
                if (a3u[0])
                    ImGui::TextUnformatted(a3u);
            }
            if (A2b_FifoCaptureEnabled() || A2c_ScratchReplayEnabled())
            {
                char a2c[160];
                A2c_OverlayLine(a2c, sizeof(a2c));
                if (a2c[0])
                    ImGui::TextUnformatted(a2c);
            }
            if (A2b_ProfileEnabled())
            {
                char ap[220];
                A2b_OverlayProfile(ap, sizeof(ap));
                if (ap[0])
                    ImGui::TextUnformatted(ap);
            }
            if (A2gNat_Enabled())
            {
                char nat[160];
                A2gNat_OverlayLine(nat, sizeof(nat));
                if (nat[0])
                    ImGui::TextUnformatted(nat);
            }
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
            HintRow("Jump", g_config.bind[0].c_str(), g_config.keys[0].c_str());
            HintRow("Slash", g_config.bind[1].c_str(), g_config.keys[1].c_str());
            HintRow("Pause", g_config.bind[6].c_str(), g_config.keys[6].c_str());
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
