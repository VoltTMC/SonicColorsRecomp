#pragma once
#include "mods/mods.h"

#include <filesystem>
#include <string>
#include <vector>

// User settings loaded from config.ini next to the executable (created with
// defaults on first run). Command line options override them.
struct Config
{
    // [Video]
    int frameRate = 30;          // 30 (original), 60, or 0 (uncapped present / 60 Hz sim)
    int hostFpsCap = 0;          // when uncapped: max re-present rate (0 ≈ unlimited)
    int internalScale = 3;       // EFB resolution multiplier (1 = 640x528)
    int windowWidth = 1280;
    int windowHeight = 720;
    bool windowMaximized = false; // last windowed session was maximized (ignored while fullscreen)
    bool fullscreen = false;
    bool exclusiveFullscreen = false; // false = borderless desktop (default); true = exclusive mode
    bool vsync = false;
    bool widescreen = true;
    bool integerScale = false;   // present at integer multiples of 640×(360|480); letterbox rest
    int anisotropy = 1;          // forced anisotropic filtering (1 = as the game sets it)
    bool asyncShaders = true;    // compile new shaders in the background (no hitches)
    bool gpuThread = true;       // GX processing/rendering on its own thread
    bool texturePack = true;     // load Dolphin-format custom textures if present
    std::string texturePackDir;  // empty: <exe>/textures, game/textures, Dolphin's Load/Textures
    // [System]
    int language = 1;                // Wii IPL.LNG: 0 ja, 1 en, 2 de, 3 fr, 4 es, 5 it, 6 nl
    bool pauseOnFocusLoss = false;   // freeze the game while the window is in the background
    bool showFpsOverlay = false;     // on-screen FPS while playing (host present rate)
    bool showInputHints = false;     // corner prompts for Jump / Slash / Pause binds
    bool autoBackupSavesOnLaunch = false; // copy save tree to save_backups/ before each Play
    bool autoBackupSavesOnWrite = false;  // copy after NAND flush (stage clear / menu save), debounced
    int saveBackupKeep = 20;              // max save_backups/ folders to keep (0 = unlimited)
    int screenshotKeep = 100;             // max screenshots/*.png to keep (0 = unlimited)
    float uiScale = 1.0f;                // launcher + overlay text/UI scale (0.75–1.50)
    // Host UI hotkeys (F1–F12 names). Escape still closes the overlay.
    std::string hotkeySettings = "F1";
    std::string hotkeyScreenshot = "F12";
    std::string hotkeyFullscreen = "F11";
    int launcherTab = 0;                 // 0 Play, 1 Settings, 2 Mods, 3 Music
    int settingsTab = 0;                 // 0 Video, 1 Input, 2 Audio, 3 System
    int launcherWidth = 1060;            // last launcher window size (logical px)
    int launcherHeight = 740;
    std::string lastSeenVersion;         // g_version last acknowledged on Play (What's new)
    std::string lastPlayedAt;            // local "YYYY-MM-DD HH:MM" when Play / --skip-launcher started
    // [Audio]
    float masterVolume = 1.0f;     // 0..1 host scale for the main (TV) mix
    float speakerVolume = 0.6f;    // 0..1 relative Wii Remote speaker level (× master)
    std::string audioDevice;       // empty = system default; Linux: SDL device name
    // [Input]
    bool rumble = true;
    bool remoteSpeaker = true;   // play Wii Remote speaker sounds on the PC
    // Stick dead zones as a fraction of full throw (0 = none, ~0.24 ≈ XInput left default).
    float stickDeadzone = 0.24f;
    float stickDeadzoneRight = 0.26f;
    // Analog trigger dead zone (0 = none, ~0.12 ≈ XInput 30/255).
    float triggerDeadzone = 0.12f;
    // button bindings, action -> controller button name (A, B, X, Y, LB, RB, LT, RT,
    // START, BACK, LS, RS, UP, DOWN, LEFT, RIGHT or NONE); see config.cpp for actions.
    // Wii "-" is unbound by default: the game does not use it, and BACK (the View button)
    // opens the settings overlay.
    std::string bind[12] = { "A", "X", "B", "Y", "RB", "LB", "START", "NONE", "LS", "RS", "RT", "LT" };
    // [Keyboard] keyboard and mouse bindings: the 12 actions above, then HOME, the D-pad
    // and the movement stick (see kKeyActions). Each is a comma-separated list of key
    // names (SPACE, J, LSHIFT, MOUSE1, ...; see input/gamepad.cpp) or NONE.
    std::string keys[21] = { "SPACE", "J, MOUSE1", "K", "L", "LSHIFT, MOUSE2", "Q", "ENTER", "BACKSPACE", "NONE", "NONE",
                             "U", "NONE", "H", "UP", "DOWN", "LEFT", "RIGHT", "W", "S", "A", "D" };
    // [Mods] load order and on/off state by mod folder (mods/mods.h); mods not listed load
    // after these, enabled
    std::vector<ModSetting> mods;
    // [Paths]
    std::string dataDir;
    std::string isoPath;
    std::string modsDir;   // empty: mods/ next to the exe or game/mods
    // [Music]
    bool musicModsEnabled = true;
    std::string musicModsDir;   // empty: mods/music next to the exe / repo
    int musicFilterCat = 0;     // Music Library Category combo (0 = All)
    int musicFilterVar = 0;     // 0 All, 1 Main, 2 FX
    int musicFilterRepl = 0;    // 0 All, 1 Replaced, 2 Original
    int musicFilterWorld = 0;   // Music Library World combo (0 = All)
};

extern Config g_config;

extern const char* const kBindActions[12];
extern const char* const kKeyActions[21];   // config.ini names of Config::keys

// Loads config.ini (writing the defaults if it does not exist) and remembers its path.
void Config_Load(const std::string& path);
// Writes the current g_config back to config.ini (with comments).
bool Config_Save();
// Sets lastPlayedAt to the local clock and saves (Play / --skip-launcher).
void Config_StampLastPlayed();
// Copies the current config.ini into user_data/config_backups/ (before a settings reset).
bool Config_BackupCurrent(std::string* outPath = nullptr);
// Input/keyboard (+ deadzone/rumble) profiles under user_data/input_profiles/<name>.ini
std::filesystem::path Config_InputProfilesDir();
std::vector<std::string> Config_ListInputProfiles();
std::string Config_InputProfileText(const Config& c);
bool Config_ApplyInputProfileText(const std::string& text, Config& c);
bool Config_SaveInputProfile(const std::string& name, std::string* outPath = nullptr);
bool Config_LoadInputProfile(const std::string& name);
// Mod enable/order profiles under user_data/mod_profiles/<name>.ini
std::filesystem::path Config_ModProfilesDir();
std::vector<std::string> Config_ListModProfiles();
std::string Config_ModProfileText(const Config& c);
bool Config_ApplyModProfileText(const std::string& text, Config& c);
bool Config_SaveModProfile(const std::string& name, std::string* outPath = nullptr);
bool Config_LoadModProfile(const std::string& name);
// Video/graphics profiles under user_data/video_profiles/<name>.ini
std::filesystem::path Config_VideoProfilesDir();
std::vector<std::string> Config_ListVideoProfiles();
std::string Config_VideoProfileText(const Config& c);
bool Config_ApplyVideoProfileText(const std::string& text, Config& c);
bool Config_SaveVideoProfile(const std::string& name, std::string* outPath = nullptr);
bool Config_LoadVideoProfile(const std::string& name);
const std::string& Config_Path();
std::string Config_Text(const Config& c);
// Resolve an F1–F12 hotkey name to a virtual-key code (fallback if empty/invalid).
int Config_HotkeyVk(const std::string& name, int fallbackVk);
// Canonical F1–F12 name for a virtual-key (or fallback name).
std::string Config_HotkeyName(const std::string& name, const char* fallbackName);
