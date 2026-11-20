// Config_Text / Config_Load roundtrip for Audio (+ a few Video) keys. No game / GUI.
#include "../../runtime/config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static int g_fail = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            g_fail++;                                                                                                  \
        }                                                                                                              \
    } while (0)

// Stubs pulled in by config.cpp
void Log_Write(int, const std::string&) {}
static fs::path g_userDir;
fs::path Platform_UserDir() { return g_userDir.empty() ? fs::temp_directory_path() / "scr_config_test_user" : g_userDir; }
bool Platform_ReplaceFile(const std::string& from, const std::string& to)
{
    std::error_code ec;
    fs::rename(from, to, ec);
    if (!ec) return true;
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) return false;
    fs::remove(from, ec);
    return true;
}
float g_audioVolume = 1.0f;
float g_speakerVolume = 0.6f;
void AudioOut_ApplyVolumes(float master, float speaker)
{
    g_audioVolume = master;
    g_speakerVolume = speaker;
}

int main()
{
    fs::path tmp = fs::temp_directory_path() / "scr_config_test.ini";
    std::error_code ec;
    fs::remove(tmp, ec);

    // Seed a file via Config_Text so Load does not rewrite defaults over our values.
    {
        Config c;
        c.masterVolume = 0.42f;
        c.speakerVolume = 0.33f;
        c.audioDevice = "Test Speakers";
        c.frameRate = 60;
        c.vsync = true;
        c.exclusiveFullscreen = true;
        c.stickDeadzone = 0.15f;
        c.stickDeadzoneRight = 0.35f;
        c.triggerDeadzone = 0.22f;
        std::ofstream f(tmp);
        f << Config_Text(c);
    }

    g_config = Config{};
    Config_Load(tmp.string());
    CHECK(std::fabs(g_config.masterVolume - 0.42f) < 0.001f);
    CHECK(std::fabs(g_config.speakerVolume - 0.33f) < 0.001f);
    CHECK(g_config.audioDevice == "Test Speakers");
    CHECK(g_config.frameRate == 60);
    CHECK(g_config.vsync);
    CHECK(g_config.exclusiveFullscreen);
    CHECK(std::fabs(g_config.stickDeadzone - 0.15f) < 0.001f);
    CHECK(std::fabs(g_config.stickDeadzoneRight - 0.35f) < 0.001f);
    CHECK(std::fabs(g_config.triggerDeadzone - 0.22f) < 0.001f);
    CHECK(!g_config.integerScale);   // default off in seeded file unless set
    CHECK(std::fabs(g_audioVolume - 0.42f) < 0.001f);
    CHECK(std::fabs(g_speakerVolume - 0.33f) < 0.001f);

    std::string text = Config_Text(g_config);
    CHECK(text.find("[Audio]") != std::string::npos);
    CHECK(text.find("master_volume") != std::string::npos);
    CHECK(text.find("audio_device") != std::string::npos);

    // Save roundtrip incl. integer_scale
    g_config.masterVolume = 0.75f;
    g_config.integerScale = true;
    CHECK(Config_Save());
    g_config = Config{};
    Config_Load(tmp.string());
    CHECK(std::fabs(g_config.masterVolume - 0.75f) < 0.001f);
    CHECK(g_config.integerScale);
    CHECK(Config_Text(g_config).find("integer_scale") != std::string::npos);

    g_config.hostFpsCap = 120;
    CHECK(Config_Save());
    g_config = Config{};
    Config_Load(tmp.string());
    CHECK(g_config.hostFpsCap == 120);
    CHECK(Config_Text(g_config).find("host_fps_cap") != std::string::npos);

    g_config.stickDeadzone = 0.10f;
    g_config.stickDeadzoneRight = 0.40f;
    g_config.triggerDeadzone = 0.05f;
    CHECK(Config_Save());
    g_config = Config{};
    Config_Load(tmp.string());
    CHECK(std::fabs(g_config.stickDeadzone - 0.10f) < 0.001f);
    CHECK(std::fabs(g_config.stickDeadzoneRight - 0.40f) < 0.001f);
    CHECK(std::fabs(g_config.triggerDeadzone - 0.05f) < 0.001f);
    CHECK(Config_Text(g_config).find("stick_deadzone") != std::string::npos);
    CHECK(Config_Text(g_config).find("trigger_deadzone") != std::string::npos);

    g_config.showFpsOverlay = true;
    g_config.showInputHints = true;
    g_config.autoBackupSavesOnLaunch = true;
    g_config.autoBackupSavesOnWrite = true;
    g_config.saveBackupKeep = 12;
    g_config.screenshotKeep = 40;
    g_config.uiScale = 1.25f;
    g_config.hotkeySettings = "F2";
    g_config.hotkeyScreenshot = "F8";
    g_config.hotkeyFullscreen = "F9";
    CHECK(Config_Save());
    g_config = Config{};
    Config_Load(tmp.string());
    CHECK(g_config.showFpsOverlay);
    CHECK(g_config.showInputHints);
    CHECK(g_config.autoBackupSavesOnLaunch);
    CHECK(g_config.autoBackupSavesOnWrite);
    CHECK(g_config.saveBackupKeep == 12);
    CHECK(g_config.screenshotKeep == 40);
    CHECK(Config_Text(g_config).find("show_input_hints") != std::string::npos);
    CHECK(std::fabs(g_config.uiScale - 1.25f) < 0.001f);
    CHECK(g_config.hotkeySettings == "F2");
    CHECK(g_config.hotkeyScreenshot == "F8");
    CHECK(g_config.hotkeyFullscreen == "F9");
    CHECK(Config_HotkeyVk("F2", 0) == 0x71);
    CHECK(Config_Text(g_config).find("show_fps_overlay") != std::string::npos);
    CHECK(Config_Text(g_config).find("auto_backup_saves_on_launch") != std::string::npos);
    CHECK(Config_Text(g_config).find("auto_backup_saves_on_write") != std::string::npos);
    CHECK(Config_Text(g_config).find("save_backup_keep") != std::string::npos);
    CHECK(Config_Text(g_config).find("screenshot_keep") != std::string::npos);
    CHECK(Config_Text(g_config).find("ui_scale") != std::string::npos);
    CHECK(Config_Text(g_config).find("hotkey_settings") != std::string::npos);

    g_config.launcherTab = 2;
    g_config.settingsTab = 3;
    g_config.launcherWidth = 1280;
    g_config.launcherHeight = 800;
    CHECK(Config_Save());
    g_config = Config{};
    Config_Load(tmp.string());
    CHECK(g_config.launcherTab == 2);
    CHECK(g_config.settingsTab == 3);
    CHECK(g_config.launcherWidth == 1280);
    CHECK(g_config.launcherHeight == 800);
    CHECK(Config_Text(g_config).find("launcher_tab") != std::string::npos);
    CHECK(Config_Text(g_config).find("settings_tab") != std::string::npos);
    CHECK(Config_Text(g_config).find("launcher_width") != std::string::npos);
    CHECK(Config_Text(g_config).find("last_seen_version") != std::string::npos);
    CHECK(Config_Text(g_config).find("window_maximized") != std::string::npos);

    g_config.windowMaximized = true;
    CHECK(Config_Save());
    g_config = Config{};
    Config_Load(tmp.string());
    CHECK(g_config.windowMaximized);

    g_config.lastSeenVersion = "v0.1.0";
    CHECK(Config_Save());
    g_config = Config{};
    Config_Load(tmp.string());
    CHECK(g_config.lastSeenVersion == "v0.1.0");
    CHECK(Config_Text(g_config).find("last_played_at") != std::string::npos);

    g_config.lastPlayedAt = "2026-11-20 12:00";
    CHECK(Config_Save());
    g_config = Config{};
    Config_Load(tmp.string());
    CHECK(g_config.lastPlayedAt == "2026-11-20 12:00");

    g_config.musicFilterCat = 2;
    g_config.musicFilterVar = 1;
    g_config.musicFilterRepl = 2;
    g_config.musicFilterWorld = 3;
    CHECK(Config_Save());
    g_config = Config{};
    Config_Load(tmp.string());
    CHECK(g_config.musicFilterCat == 2);
    CHECK(g_config.musicFilterVar == 1);
    CHECK(g_config.musicFilterRepl == 2);
    CHECK(g_config.musicFilterWorld == 3);
    CHECK(Config_Text(g_config).find("filter_cat") != std::string::npos);
    CHECK(Config_Text(g_config).find("filter_world") != std::string::npos);

    g_userDir = fs::temp_directory_path() / "scr_config_test_user";
    fs::remove_all(g_userDir, ec);
    std::string backupPath;
    CHECK(Config_BackupCurrent(&backupPath));
    CHECK(fs::is_regular_file(backupPath, ec));
    CHECK(backupPath.find("config_backups") != std::string::npos);

    g_config.bind[0] = "B";
    g_config.keys[0] = "Z";
    g_config.stickDeadzone = 0.33f;
    CHECK(Config_SaveInputProfile("unit_test", nullptr));
    auto profiles = Config_ListInputProfiles();
    CHECK(std::find(profiles.begin(), profiles.end(), "unit_test") != profiles.end());
    g_config.bind[0] = "A";
    g_config.keys[0] = "SPACE";
    g_config.stickDeadzone = 0.24f;
    CHECK(Config_LoadInputProfile("unit_test"));
    CHECK(g_config.bind[0] == "B");
    CHECK(g_config.keys[0] == "Z");
    CHECK(std::fabs(g_config.stickDeadzone - 0.33f) < 0.001f);

    g_config.mods = { { "alpha", true }, { "beta", false } };
    CHECK(Config_SaveModProfile("unit_mods", nullptr));
    g_config.mods.clear();
    CHECK(Config_LoadModProfile("unit_mods"));
    CHECK(g_config.mods.size() == 2);
    CHECK(g_config.mods[0].folder == "alpha" && g_config.mods[0].enabled);
    CHECK(g_config.mods[1].folder == "beta" && !g_config.mods[1].enabled);

    g_config.internalScale = 5;
    g_config.vsync = true;
    g_config.frameRate = 60;
    g_config.anisotropy = 8;
    CHECK(Config_SaveVideoProfile("unit_video", nullptr));
    g_config.internalScale = 1;
    g_config.vsync = false;
    g_config.frameRate = 30;
    g_config.anisotropy = 1;
    CHECK(Config_LoadVideoProfile("unit_video"));
    CHECK(g_config.internalScale == 5);
    CHECK(g_config.vsync);
    CHECK(g_config.frameRate == 60);
    CHECK(g_config.anisotropy == 8);
    fs::remove_all(g_userDir, ec);

    fs::remove(tmp, ec);
    if (g_fail)
    {
        fprintf(stderr, "config_test: %d failure(s)\n", g_fail);
        return 1;
    }
    puts("config_test OK");
    return 0;
}
