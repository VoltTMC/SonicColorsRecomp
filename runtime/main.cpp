// SonicColorsRecomp: statically recompiled Sonic Colors (Wii) PAL SNCP8P
#include "log.h"
#include "config.h"
#include "perf.h"
#include "gx/video.h"
#include "cpu/cpu.h"
#include "kernel/memory.h"
#include "kernel/boot.h"
#include "kernel/scheduler.h"
#include "kernel/threads.h"
#include "hw/hw.h"
#include "ios/disc.h"
#include "ios/ios.h"
#include "ios/nand_paths.h"
#include "mods/mods.h"
#include "audio/music_mod.h"
#include "install/install.h"
#include "install/releases.h"
#include "install/verify.h"
#include "region.h"
#include "platform/platform.h"
#include "platform/input.h"
#include "ui/settings_ui.h"
#include "ui/update_check.h"
#include "version.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <string>
#ifdef _WIN32
#include <stdlib.h>
#endif

void NAND_InitSystemFiles();
void Renderer_Init();
#include "audio/audio_out.h"
void Crash_Init();
void Crash_StartWatchdog();
extern bool g_noMessageBox;
extern int g_logLevel;
extern "C" uint32_t g_cfgFrameRate;

// Restarts the game (settings that need a restart): same command line, no launcher.
void App_Restart()
{
    LOG_INFO("restarting");
    Platform_Restart(true, true);
}

// What the game thread needs from main()
static std::string s_dataDir, s_isoPath, s_nand;
static uint32_t s_watchAddr = 0;
static int s_profStart = -1, s_profDur = 0;
static uint64_t s_processStartMs = 0;

static void RunGame()
{
    if (!Memory_Init()) FATAL("memory init failed");
    Crash_Init();
    Cpu_Init();
    Sched_Init();
    Threads_Init();
    HW_Init();
    Renderer_Init();
    AudioOut_Init();
    AudioOut_ApplyVolumes(g_config.masterVolume, g_config.speakerVolume);
    if (!g_config.audioDevice.empty()) AudioOut_SetDevice(g_config.audioDevice);
    MusicMod_Init();
    { extern bool g_gpuThread; if (g_gpuThread) GX_StartThread(); }
    if (!Disc_Open(s_dataDir, s_isoPath, Mods_Loaded())) FATAL("cannot open disc data");
    std::filesystem::create_directories(s_nand);
    IOS_Init(s_nand);
    NAND_InitSystemFiles();

    uint32_t entry = Boot_Setup(s_dataDir);
    if (s_watchAddr) { extern void Crash_SetWatch(uint32_t); Crash_SetWatch(s_watchAddr); }
    if (!entry) FATAL("boot failed");
    if (s_processStartMs)
        LOG_INFO("startup: guest entry ready in {} ms", Platform_TickMs() - s_processStartMs);
    Crash_StartWatchdog();
    if (s_profStart >= 0) { extern void Crash_StartProfiler(int, int); Crash_StartProfiler(s_profStart, s_profDur); }
    Threads_RunMain(entry);
}

static void PrintUsage()
{
    // Fixed product name — argv[0] is often an AppImage/IDE wrapper, not our binary.
    const char* exe = "SonicColorsRecomp";
    std::printf(
        "Sonic Colors Recomp — native PC recompilation of Sonic Colors (Wii)\n"
        "\n"
        "Usage: %s [options]\n"
        "\n"
        "General:\n"
        "  -h, --help              Show this help and exit\n"
        "  -V, --version           Print build version and exit\n"
        "  --check-updates         Query GitHub for the latest release tag and exit\n"
        "                          (exit 0 = up to date / check ok; 2 = newer available;\n"
        "                          1 = check failed). Does not download anything.\n"
        "  --download-update       If a newer release exists, download the platform\n"
        "                          package into user_data/downloads/ and exit\n"
        "                          (0 = up to date or downloaded; 2 = newer but no\n"
        "                          matching asset; 1 = failed). Does not install.\n"
        "  --save-diagnostics      Write diagnostics/scr_diag_*.txt (config + log tails)\n"
        "                          and print its path; exit 0 on success, 1 on failure.\n"
        "  --portable              Keep config/saves/logs next to the executable\n"
        "                          (also: SCR_PORTABLE=1). Portable ZIP default; installer\n"
        "                          builds use %APPDATA%\\SonicColorsRecomp unless this is set.\n"
        "                          useful for AppImage when you want a side-by-side folder.\n"
        "  --skip-launcher         Start the game without the launcher UI\n"
        "  --data DIR              Game data directory (extracted DATA)\n"
        "  --iso PATH              Disc image used by the installer\n"
        "  --game PATH             Disc image or extracted DATA folder (for launchers)\n"
        "  --prepare               Windowless install from --game/--iso into --data\n"
        "                          (or the default game/extracted/DATA). Prints progress\n"
        "                          lines; no UI. Used by RecompLauncher Easy Setup.\n"
        "  --verify-data           Windowless integrity check of --data (or config/\n"
        "                          default extracted DATA). Full SHA-1 of every file;\n"
        "                          progress lines. Exit 0 = intact, 1 = missing/damaged.\n"
        "  --verify-data-quick     Same as --verify-data but size + main.dol only (fast).\n"
        "  --nand DIR              Host NAND / save directory for this run\n"
        "                          (also SCR_TEST_NAND_DIR; isolated from normal userdata)\n"
        "  --mods DIR              Mods directory override\n"
        "  --nobox                 Suppress message boxes / dialogs\n"
        "  --verbose               Extra log detail\n"
        "\n"
        "Display / pacing:\n"
        "  --frame-rate 30|60|0    Pacing (0 = uncapped). Also --60fps\n"
        "                          60 = true 60 Hz guest + spring cooldown gate.\n"
        "  --guest-60              Force wait=1 (implied by --60fps)\n"
        "  --vsync / --no-vsync    Override config VSync for this run\n"
        "  --show-fps              Show on-screen FPS overlay for this run\n"
        "  --show-hints            Show Jump/Slash/Pause input hints for this run\n"
        "  --spring-cd-gate        Force spring gate on (implied by --60fps);\n"
        "                          SCR_SPRING_CD_GATE=0 disables\n"
        "  --spring-diag           Log Spring/WideSpring/SpringPole collide+act+upd (SCR_SPRING_DIAG)\n"
        "  --scale N               Internal resolution scale (1–8)\n"
        "  --single-core           Process GX on the CPU thread\n"
        "\n"
        "Audio / music:\n"
        "  --mute                  Silence host audio\n"
        "  --list-audio-devices    Print playback devices and exit\n"
        "  --list-gamepads         Print connected controllers (slots 0–3) and exit\n"
        "  --music-mods DIR        Music replacement directory\n"
        "  --music-debug           Log music-mod decisions\n"
        "  --music-force-stage ID  Force stage id for music cues (e.g. stg110)\n"
        "\n"
        "Config and saves live under the user/data directory; F1 opens in-game settings,\n"
        "F11 toggles fullscreen. F12 saves a screenshot. See docs/ for music modding and compatibility notes.\n",
        exe);
}

int main(int argc, char** argv)
{
    Platform_Init(argc, argv);
    s_processStartMs = Platform_TickMs();
    // packaging: fill the shader cache that ships with the exe (no window, no game data)
    if (argc == 3 && std::string(argv[1]) == "--precompile-shaders")
    {
        extern int Renderer_PrecompileShaders(const std::string& dir);
        return Renderer_PrecompileShaders(argv[2]) ? 1 : 0;
    }
    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        if (a == "-h" || a == "--help")
        {
            PrintUsage();
            return 0;
        }
        if (a == "-V" || a == "--version")
        {
            std::printf("SonicColorsRecomp %s\n", g_version);
            return 0;
        }
        if (a == "--check-updates")
        {
            UpdateCheck_Start(true);
            while (UpdateCheck_Busy())
                Platform_SleepMs(50);
            const std::string msg = UpdateCheck_Message();
            std::printf("%s\n", msg.c_str());
            if (!UpdateCheck_HasResult())
                return 1;
            if (msg.find("failed") != std::string::npos || msg.find("Could not") != std::string::npos ||
                msg.find("No release") != std::string::npos)
                return 1;
            if (UpdateCheck_NewerAvailable())
            {
                const std::string notes = UpdateCheck_Notes();
                if (!notes.empty()) std::printf("\n%s\n", notes.c_str());
                return 2;
            }
            return 0;
        }
        if (a == "--download-update")
        {
            UpdateCheck_Start(true);
            while (UpdateCheck_Busy())
                Platform_SleepMs(50);
            const std::string msg = UpdateCheck_Message();
            std::printf("%s\n", msg.c_str());
            if (!UpdateCheck_HasResult())
                return 1;
            if (msg.find("failed") != std::string::npos || msg.find("Could not") != std::string::npos ||
                msg.find("No release") != std::string::npos)
                return 1;
            if (!UpdateCheck_NewerAvailable())
                return 0;
            if (!UpdateCheck_CanDownload())
            {
                std::printf("Newer release has no matching package for this platform.\n");
                return 2;
            }
            UpdateCheck_StartDownload();
            UpdateCheck_WaitDownload();
            const std::string dl = UpdateCheck_DownloadMessage();
            if (!dl.empty()) std::printf("%s\n", dl.c_str());
            if (!UpdateCheck_DownloadOk())
                return 1;
            const std::string path = UpdateCheck_DownloadPath();
            if (!path.empty()) std::printf("%s\n", path.c_str());
            return 0;
        }
        if (a == "--list-audio-devices")
        {
            auto devices = AudioOut_ListDevices();
            for (const auto& d : devices)
            {
                if (d.name.empty())
                    std::printf("default\n");
                else
                    std::printf("%s\n", d.name.c_str());
            }
            return 0;
        }
        if (a == "--list-gamepads")
        {
            Platform_Init(argc, argv);
            Input_EnsurePadsProbed();
            const int n = Pad_Count();
            std::printf("pads: %d\n", n);
            for (int i = 0; i < 4; i++)
            {
                const char* name = Pad_Name(i);
                PadState st{};
                const bool ok = Pad_Read(i, st);
                if (name || ok)
                    std::printf("%d: %s buttons=%04x sticks=(%d,%d)(%d,%d) triggers=%u/%u\n", i,
                                name ? name : "(unnamed)", ok ? st.buttons : 0, ok ? st.lx : 0,
                                ok ? st.ly : 0, ok ? st.rx : 0, ok ? st.ry : 0, ok ? st.lt : 0,
                                ok ? st.rt : 0);
            }
            return 0;
        }
        if (a == "--portable")
            Platform_RequestPortable();
    }
    std::string root = Platform_UserDir().string();   // the exe directory on Windows
    Log_Open((root + "/SonicColorsRecomp.log").c_str());
    Config_Load(root + "/config.ini");
    std::string dataDir = g_config.dataDir, isoPath = g_config.isoPath;
    uint32_t watchAddr = 0;
    int profStart = -1, profDur = 0;
    std::string nandDir;   // --nand: separate save data (test runs)
    // command line overrides for this run only (never written to config.ini)
    bool fps60 = false, singleCore = false, skipLauncher = false, showFps = false, showHints = false;
    int vsyncOverride = -1;   // -1 = config; 0 = --no-vsync; 1 = --vsync
    bool prepare = false, saveDiagnostics = false, verifyData = false, verifyDataQuick = false;
    int scaleOverride = 0;
    int fpsOverride = -1;   // --frame-rate 30|60|0; --60fps sets 60
    auto applyGamePath = [&](const std::string& g) {
        std::error_code ec;
        if (std::filesystem::is_directory(g, ec) &&
            std::filesystem::exists(std::filesystem::path(g) / "sys" / "boot.bin", ec))
            dataDir = g;
        else
            isoPath = g;
    };
    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        if (a == "--data" && i + 1 < argc) dataDir = argv[++i];
        else if (a.size() > 7 && a.compare(0, 7, "--data=") == 0) dataDir = a.substr(7);
        else if (a == "--iso" && i + 1 < argc) isoPath = argv[++i];
        else if (a.size() > 6 && a.compare(0, 6, "--iso=") == 0) isoPath = a.substr(6);
        else if (a == "--game" && i + 1 < argc) applyGamePath(argv[++i]);
        else if (a.size() > 7 && a.compare(0, 7, "--game=") == 0) applyGamePath(a.substr(7));
        else if (a == "--prepare") prepare = true;
        else if (a == "--verify-data") verifyData = true;
        else if (a == "--verify-data-quick") verifyDataQuick = true;
        else if (a == "--save-diagnostics") saveDiagnostics = true;
        else if (a == "--verbose") g_logLevel = 0;
        else if (a == "--show-fps") showFps = true;
        else if (a == "--show-hints") showHints = true;
        else if (a == "--60fps") fps60 = true;
        else if (a == "--guest-60")
        {
#ifdef _WIN32
            _putenv_s("SCR_GUEST_60", "1");
#else
            setenv("SCR_GUEST_60", "1", 1);
#endif
        }
        else if (a == "--spring-cd-gate")
        {
#ifdef _WIN32
            _putenv_s("SCR_SPRING_CD_GATE", "1");
#else
            setenv("SCR_SPRING_CD_GATE", "1", 1);
#endif
        }
        else if (a == "--spring-diag")
        {
#ifdef _WIN32
            _putenv_s("SCR_SPRING_DIAG", "1");
#else
            setenv("SCR_SPRING_DIAG", "1", 1);
#endif
        }
        else if (a == "--frame-rate" && i + 1 < argc)
        {
            int v = atoi(argv[++i]);
            fpsOverride = (v <= 0) ? 0 : (v >= 60 ? 60 : 30);
        }
        else if (a == "--vsync") vsyncOverride = 1;
        else if (a == "--no-vsync") vsyncOverride = 0;
        else if (a == "--profile" && i + 2 < argc) { profStart = atoi(argv[++i]); profDur = atoi(argv[++i]); }
        else if (a == "--dump-audio") { extern FILE* g_audioDump; g_audioDump = fopen("audio_dump.raw", "wb"); }
        else if (a == "--shots-start" && i + 1 < argc) { extern uint32_t g_screenshotStart; g_screenshotStart = (uint32_t)atoi(argv[++i]); }
        else if (a == "--dbg-nodepth") { extern int g_dbgNoDepth; g_dbgNoDepth = 1; }
        else if (a == "--dbg-nocull") { extern int g_dbgNoCull; g_dbgNoCull = 1; }
        else if (a == "--watch" && i + 1 < argc) watchAddr = (uint32_t)strtoul(argv[++i], nullptr, 16);
        else if (a == "--capture" && i + 1 < argc) { extern uint32_t g_captureFrame; g_captureFrame = (uint32_t)atoi(argv[++i]); }
        else if (a == "--nobox") g_noMessageBox = true;
        else if (a == "--profile-gx") { extern bool g_profileGX; g_profileGX = true; }
        else if (a == "--single-core") singleCore = true;
        else if (a == "--skip-launcher") skipLauncher = true;
        else if (a == "--dump-textures") { extern bool g_dumpTextures; g_dumpTextures = true; }
        else if (a == "--autopress") { extern bool g_autoPress; g_autoPress = true; }
        else if (a == "--input-debug") { extern void InputDebug_SetEnabled(bool); InputDebug_SetEnabled(true); }
        else if (a == "--boost-debug") { extern void BoostDebug_SetEnabled(bool); BoostDebug_SetEnabled(true); }
        else if (a == "--input-script" && i + 1 < argc) { extern void Gamepad_LoadScript(const char*); Gamepad_LoadScript(argv[++i]); }
        else if (a == "--tas" && i + 1 < argc) { extern void Tas_Load(const char*); Tas_Load(argv[++i]); }
        else if (a == "--telemetry" && i + 1 < argc) { extern void Tas_SetTelemetry(const char*); Tas_SetTelemetry(argv[++i]); }
        else if (a == "--nand" && i + 1 < argc) nandDir = argv[++i];
        else if (a == "--mods" && i + 1 < argc) Mods_SetDirOverride(argv[++i]);
        else if (a == "--music-mods" && i + 1 < argc) MusicMod_SetDir(argv[++i]);
        else if (a == "--music-debug") MusicMod_SetDebug(true);
        else if (a == "--music-force-stage" && i + 1 < argc) MusicMod_ForceStage(argv[++i]);
        else if (a == "--mute") { extern bool g_audioMute; g_audioMute = true; }
        else if (a == "--perf") { extern void Perf_SetEnabled(bool); Perf_SetEnabled(true); }
        else if (a == "--scale" && i + 1 < argc) scaleOverride = std::clamp(atoi(argv[++i]), 1, 8);
        else if (a == "--shots" && i + 1 < argc) { extern int g_screenshotEvery; g_screenshotEvery = atoi(argv[++i]); }
    }
    // Default locations: <exe>/game/... or the repo layout
    auto pick = [&](std::string& v, const char* rel) {
        if (!v.empty()) return;
        for (std::string base : { root, root + "/..", root + "/../..", root + "/../../.." })
        {
            std::filesystem::path p = std::filesystem::path(base) / rel;
            if (std::filesystem::exists(p)) { v = std::filesystem::weakly_canonical(p).string(); return; }
        }
    };
    pick(dataDir, "game/extracted/DATA");
    if (isoPath.empty())
    {
        // Prefer a remembered path from a previous install if it still exists.
        if (!g_config.isoPath.empty() && std::filesystem::exists(g_config.isoPath) &&
            Install_LooksLikeDiscImage(g_config.isoPath))
            isoPath = g_config.isoPath;
    }
    if (isoPath.empty())
    {
        // Any supported disc image in game/ next to the exe or the repo root
        for (std::string b : { root, root + "/.." })
        {
            std::error_code ec;
            for (auto& e : std::filesystem::directory_iterator(std::filesystem::path(b) / "game", ec))
            {
                if (!e.is_regular_file(ec)) continue;
                if (Install_LooksLikeDiscImage(e.path()))
                {
                    isoPath = e.path().string();
                    break;
                }
            }
            if (!isoPath.empty()) break;
        }
    }

    // installs go to game/ next to the exe (or the repo's game/ when running from out/)
    std::filesystem::path gameDir = std::filesystem::path(root) / "game";
    if (!std::filesystem::exists(gameDir) && std::filesystem::exists(std::filesystem::path(root) / ".." / "game"))
        gameDir = std::filesystem::weakly_canonical(std::filesystem::path(root) / ".." / "game");
    std::filesystem::path installDest = gameDir / "extracted" / "DATA";

    // RecompLauncher: skip our own launcher when started from its Play button.
    if (const char* rl = std::getenv("RECOMPLAUNCHER"); rl && rl[0] == '1')
        skipLauncher = true;
    // Big Picture: prefer fullscreen unless the player already chose otherwise in config.
    if (const char* mode = std::getenv("RECOMPLAUNCHER_MODE");
        mode && !std::strcmp(mode, "bigpicture") && !g_config.fullscreen)
        g_config.fullscreen = true;

    if (saveDiagnostics)
    {
        std::string err;
        const std::string path = SettingsUI_WriteDiagnosticReport(err);
        if (path.empty())
        {
            std::fprintf(stderr, "save-diagnostics failed: %s\n", err.c_str());
            return 1;
        }
        std::printf("%s\n", path.c_str());
        return 0;
    }

    if (prepare)
    {
        if (isoPath.empty())
        {
            std::fprintf(stderr, "prepare: pass --game <disc-image> or --iso <disc-image>\n");
            return 1;
        }
        std::filesystem::path dest = dataDir.empty() ? installDest : std::filesystem::path(dataDir);
        const int rc = Install_PrepareCli(isoPath, dest);
        if (rc == 0)
        {
            g_config.dataDir = dest.string();
            g_config.isoPath = isoPath;
            Config_Save();
            LOG_INFO("prepare: wrote data={} iso={}", g_config.dataDir, g_config.isoPath);
        }
        return rc;
    }

    if (verifyData || verifyDataQuick)
    {
        std::filesystem::path dest = dataDir.empty() ? installDest : std::filesystem::path(dataDir);
        return Install_VerifyCli(dest, verifyDataQuick);
    }

    // Launcher on every start (settings, install / repair the game data); TAS runs,
    // --skip-launcher (test scripts) and restarts from the in-game settings go straight to
    // the game. --nobox only suppresses message boxes and dialogs.
    extern bool Tas_Active();
    if (!Tas_Active() && !skipLauncher)
    {
        extern bool Launcher_Run(std::string& dataDir, const std::string& isoPath, const std::filesystem::path& installDest);
        if (!Launcher_Run(dataDir, isoPath, installDest)) return 0;
    }
    else if (!Tas_Active() && skipLauncher)
        Config_StampLastPlayed();   // RecompLauncher / --skip-launcher (Play button stamps itself)

    g_cfgFrameRate = fpsOverride >= 0 ? (uint32_t)fpsOverride : (fps60 ? 60u : (uint32_t)g_config.frameRate);
    if (showFps) g_config.showFpsOverlay = true;   // this run only; not written until Settings save
    if (showHints) g_config.showInputHints = true;
    Perf_SetHostFpsCap(g_config.hostFpsCap);
    { extern bool g_gpuThread; g_gpuThread = g_config.gpuThread && !singleCore; }
    g_videoConfig.internalScale = scaleOverride ? scaleOverride : g_config.internalScale;
    g_videoConfig.windowWidth = g_config.windowWidth;
    g_videoConfig.windowHeight = g_config.windowHeight;
    g_videoConfig.fullscreen = g_config.fullscreen;
    g_videoConfig.vsync = vsyncOverride >= 0 ? (vsyncOverride != 0) : g_config.vsync;
    g_videoConfig.widescreen = g_config.widescreen;
    g_videoConfig.integerScale = g_config.integerScale;
    {
        const uint32_t wf = Perf_WaitFields();
        if (g_cfgFrameRate >= 60 || Perf_Guest60Forced())
            LOG_INFO("guest 60 Hz (wait={}): spring cooldown gate on (WideSpring hold)", wf);
        if (const char* g = std::getenv("SCR_SPRING_CD_GATE"); g && g[0] == '0')
            LOG_WARN("SCR_SPRING_CD_GATE=0: spring cooldown gate disabled");
        if (std::getenv("SCR_SPRING_DIAG"))
            LOG_INFO("SCR_SPRING_DIAG: Spring/WideSpring/SpringPole event logging on");
        LOG_INFO("SonicColorsRecomp starting; data={} iso={} fps={} wait={} scale={}", dataDir, isoPath,
                 g_cfgFrameRate, wf, g_videoConfig.internalScale);
    }
    if (dataDir.empty())
    {
        // no launcher (restart, --skip-launcher): install directly
        if (g_noMessageBox)
            FATAL("Sonic Colors game data is not installed. Start once with the launcher to install "
                  "from your disc image ({}), or run: scr_install <disc-image> <dest-DATA-dir>",
                  kReleaseNames);
        extern bool InstallUI_Run(const std::filesystem::path&, const std::string&, const std::string&);
        if (!InstallUI_Run(installDest, isoPath, "")) return 1;
        dataDir = installDest.string();
    }

    // Guard against --data / --skip-launcher pointing at an empty or wrong tree.
    {
        std::string problem;
        DataStatus st = Install_QuickCheck(dataDir, problem);
        if (st != DataStatus::Ok)
        {
            if (g_noMessageBox)
                FATAL("Game data at {} is {}: {}. Supported release: {}",
                      dataDir, st == DataStatus::Missing ? "missing" : "damaged",
                      problem.empty() ? "(no detail)" : problem, kReleaseNames);
            extern bool InstallUI_Run(const std::filesystem::path&, const std::string&, const std::string&);
            if (!InstallUI_Run(dataDir.empty() ? installDest : std::filesystem::path(dataDir),
                               isoPath, st == DataStatus::Corrupt ? problem : ""))
                return 1;
            if (dataDir.empty()) dataDir = installDest.string();
        }
    }

    std::string gameId = Release_InstalledId(dataDir);
    {
        std::string error;
        if (!Region_Select(gameId, error))
            FATAL("Cannot start the game: {}. Supported release: {}", error, kReleaseNames);
    }
    Mods_SetLoaded(Mods_Scan(Mods_Roots(g_config.modsDir), g_config.mods, gameId));
    { extern void SettingsUI_SetRelease(const std::string&, const std::string&); SettingsUI_SetRelease(gameId, dataDir); }
    s_dataDir = dataDir;
    s_isoPath = isoPath;
    // Isolated host NAND for saves. Prefer ../userdata (repo layout when exe is in out/),
    // else <exe>/nand. Override with --nand or SCR_TEST_NAND_DIR (test fixtures).
    // See ios/nand_paths.h. Never point test runs at the user's normal userdata.
    if (nandDir.empty())
    {
        if (const char* td = std::getenv("SCR_TEST_NAND_DIR"))
            if (td[0])
                nandDir = td;
    }
    if (nandDir.empty())
        s_nand = nand_paths::DefaultRoot().string();
    else
        s_nand = nandDir;
    {
        std::error_code ec;
        const bool isolated = !nandDir.empty();
        LOG_INFO("NAND root: {} ({})", s_nand,
                 isolated ? "ISOLATED test override — normal userdata untouched"
                          : "default / user path");
        if (isolated)
            std::filesystem::create_directories(s_nand, ec);
    }
    if (g_config.autoBackupSavesOnLaunch && nandDir.empty())
    {
        std::filesystem::path dest;
        std::string err;
        if (nand_backup::BackupTree(std::filesystem::path(s_nand), dest, err))
        {
            LOG_INFO("auto save backup: {}", dest.string());
            if (g_config.saveBackupKeep > 0)
            {
                size_t n = nand_backup::PruneBackupsIn(std::filesystem::path(s_nand), g_config.saveBackupKeep);
                if (n) LOG_INFO("pruned {} old save backup(s); keeping {}", n, g_config.saveBackupKeep);
            }
        }
        else
            LOG_INFO("auto save backup skipped: {}", err);
    }
    s_watchAddr = watchAddr;
    s_profStart = profStart;
    s_profDur = profDur;
    Platform_RunGame(RunGame);
}
