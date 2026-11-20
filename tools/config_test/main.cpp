// Config_Text / Config_Load roundtrip for Audio (+ a few Video) keys. No game / GUI.
#include "../../runtime/config.h"

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

    fs::remove(tmp, ec);
    if (g_fail)
    {
        fprintf(stderr, "config_test: %d failure(s)\n", g_fail);
        return 1;
    }
    puts("config_test OK");
    return 0;
}
