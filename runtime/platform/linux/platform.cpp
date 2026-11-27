// Process, paths, time, threads and dialogs on Linux (POSIX; SDL3 for message boxes).
#include "../platform.h"
#include "../../log.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <mutex>
#include <pthread.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace fs = std::filesystem;

void Gamepad_StopRumble();
void Window_RunEventLoop();   // platform/linux/window.cpp

static std::vector<std::string> s_args;
static std::string s_sdlError;   // SDL_Init failed (logged once the log is open)

// ---------------------------------------------------------------- process
void Platform_Init(int argc, char** argv)
{
    s_args.assign(argv, argv + argc);
    // dxvk-native presents through SDL3 windows
    setenv("DXVK_WSI_DRIVER", "SDL3", 0);
    // SDL on the main thread: windows and their events stay here (Platform_RunGame)
    SDL_SetAppMetadata("Sonic Colors - Recompiled", nullptr, "SonicColorsRecomp");
    extern void SdlGamepad_ApplyHints();
    extern void SdlGamepad_Init();
    SdlGamepad_ApplyHints();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) s_sdlError = SDL_GetError();
    else SdlGamepad_Init();
}

static void (*s_game)() = nullptr;

static void* GameThread(void*)
{
    s_game();
    Platform_ExitProcess(0);
}

void Platform_RunGame(void (*game)())
{
    // Guest code runs on a thread with a 256 MiB stack, as the main thread has on Windows
    // (the main guest thread uses the host stack). This thread pumps the window events.
    s_game = game;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 256u << 20);
    pthread_t t;
    if (pthread_create(&t, &attr, GameThread, nullptr) != 0) FATAL("cannot start the game thread");
    pthread_attr_destroy(&attr);
    Window_RunEventLoop();
    Platform_Terminate(0);
}

void Platform_LogHost()
{
    if (FILE* f = fopen("/etc/os-release", "r"))
    {
        char line[256];
        while (fgets(line, sizeof(line), f))
            if (!strncmp(line, "PRETTY_NAME=", 12))
            {
                std::string v = line + 12;
                v.erase(std::remove_if(v.begin(), v.end(), [](char c) { return c == '"' || c == '\n'; }), v.end());
                LOG_INFO("running on {}", v);
            }
        fclose(f);
    }
    if (const char* appimage = getenv("APPIMAGE")) LOG_INFO("AppImage: {}", appimage);
    if (!s_sdlError.empty()) LOG_ERROR("SDL_Init failed: {}", s_sdlError);
    LOG_INFO("SDL {}.{}.{}, video driver {}", SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
             SDL_VERSIONNUM_MICRO(SDL_GetVersion()), SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "(none)");
}

void Platform_Terminate(int code)
{
    fflush(stdout);
    _exit(code);
}

void Platform_ExitProcess(int code) { Platform_Terminate(code); }

void Platform_Restart(bool skipLauncher, bool)
{
    // Replace the process image (same PID): a child would outlive us, and an AppImage is
    // unmounted when its first process exits.
    std::vector<std::string> args = s_args;
    if (skipLauncher && std::find(args.begin(), args.end(), "--skip-launcher") == args.end()) args.push_back("--skip-launcher");
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);
    Gamepad_StopRumble();
    fflush(stdout);
    execv("/proc/self/exe", argv.data());
    LOG_WARN("restart failed ({})", strerror(errno));
    Platform_Terminate(0);
}

std::string Platform_AppImagePath()
{
    const char* p = getenv("APPIMAGE");
    return (p && *p) ? std::string(p) : std::string();
}

void Platform_RelaunchExecutable(const std::string& path)
{
    // Spawn a fresh process for `path`, then exit so an AppImage FUSE mount can unmount.
    std::vector<std::string> args = s_args;
    if (args.empty()) args.push_back(path);
    else args[0] = path;
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);
    Gamepad_StopRumble();
    fflush(stdout);
    pid_t pid = 0;
    int rc = posix_spawn(&pid, path.c_str(), nullptr, nullptr, argv.data(), environ);
    if (rc != 0)
        LOG_WARN("relaunch {} failed ({})", path, strerror(rc));
    Platform_Terminate(rc == 0 ? 0 : 1);
}

uint32_t Platform_ProcessId() { return (uint32_t)getpid(); }

bool Platform_SpawnDetached(const std::vector<std::string>& argv)
{
    if (argv.empty()) return false;
    std::vector<std::string> owned = argv;
    std::vector<char*> ptrs;
    for (auto& a : owned) ptrs.push_back(a.data());
    ptrs.push_back(nullptr);
    pid_t pid = 0;
    int rc = posix_spawn(&pid, owned[0].c_str(), nullptr, nullptr, ptrs.data(), environ);
    if (rc != 0)
    {
        LOG_WARN("spawn {} failed ({})", owned[0], strerror(rc));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- paths and files
fs::path Platform_ExeDir()
{
    static fs::path dir = [] {
        std::error_code ec;
        fs::path exe = fs::read_symlink("/proc/self/exe", ec);
        return ec ? fs::current_path() : exe.parent_path();
    }();
    return dir;
}

static fs::path XdgDir(const char* var, const char* fallback)
{
    const char* v = getenv(var);
    if (v && *v == '/') return v;
    const char* home = getenv("HOME");
    return fs::path(home ? home : "/tmp") / fallback;
}

static bool s_portableRequested = false;
void Platform_RequestPortable() { s_portableRequested = true; }

bool Platform_IsInstalledLayout()
{
    std::error_code ec;
    return fs::exists(Platform_ExeDir() / "installed.txt", ec);
}

bool Platform_IsPortable()
{
    if (Platform_LauncherDataDir()) return false;
    if (s_portableRequested) return true;
    if (getenv("SCR_PORTABLE")) return true;
    if (Platform_IsInstalledLayout()) return false;
    fs::path exe = Platform_ExeDir();
    return !getenv("APPIMAGE") && access(exe.c_str(), W_OK) == 0;
}

// First start with RecompLauncher's data folder: copies what older versions kept beside the
// executable into it (the launcher's backups, cloud saves and Uninstall only look after its
// data folder). The old files stay.
static void CopyLegacyUserData(const fs::path& data)
{
    // Written last, once every item is copied or wasn't there: userdata (the save NAND) is made
    // on every start, so it can't tell whether an earlier copy finished.
    const fs::path marker = data / ".copied-from-program-folder";
    std::error_code ec;
    if (fs::exists(marker, ec)) return;
    const fs::path exe = Platform_ExeDir();
    bool ok = true;
    // Each item is copied only where there is none yet (the launcher may write config.ini first,
    // or an earlier start copied it before being stopped).
    auto copy = [&](const fs::path& from, const fs::path& to) {
        std::error_code ec;
        if (!fs::exists(from, ec) || fs::exists(to, ec)) return;
        fs::path part = to;
        part += ".part";
        fs::remove_all(part, ec);
        fs::copy(from, part, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
        if (!ec) fs::rename(part, to, ec);
        if (!ec) return;
        std::fprintf(stderr, "could not copy %s to %s: %s\n", from.c_str(), to.c_str(), ec.message().c_str());
        fs::remove_all(part, ec);
        ok = false;
    };
    // saves (ios/nand_paths.h): ../userdata, else the older nand/
    const fs::path ud = (exe / ".." / "userdata").lexically_normal();
    copy(fs::exists(ud, ec) ? ud : exe / "nand", data / "userdata");
    for (const char* name : { "config.ini", "config_backups", "input_profiles", "mod_profiles", "video_profiles",
                              "mods", "screenshots", "diagnostics" })
        copy(exe / name, data / name);
    if (!ok) return; // tried again on the next start
    FILE* f = std::fopen(marker.c_str(), "w");
    if (f) std::fclose(f);
}

fs::path Platform_UserDir()
{
    static fs::path dir = [] {
        if (const char* data = Platform_LauncherDataDir())
        {
            fs::path d = data;
            std::error_code ec;
            fs::create_directories(d, ec);
            CopyLegacyUserData(d);
            return d;
        }
        fs::path exe = Platform_ExeDir();
        const bool portable = s_portableRequested || getenv("SCR_PORTABLE");
        if (portable || (!getenv("APPIMAGE") && access(exe.c_str(), W_OK) == 0)) return exe;
        fs::path d = XdgDir("XDG_DATA_HOME", ".local/share") / "SonicColorsRecomp";
        std::error_code ec;
        fs::create_directories(d, ec);
        return d;
    }();
    return dir;
}

fs::path Platform_MusicToolsBinDir()
{
    std::error_code ec;
    fs::path dir = Platform_UserDir() / "tools" / "bin";
    fs::create_directories(dir, ec);
    return dir;
}

fs::path Platform_DolphinTextureDir()
{
    const char* home = getenv("HOME");
    fs::path candidates[] = {
        XdgDir("XDG_DATA_HOME", ".local/share") / "dolphin-emu" / "Load" / "Textures",
        fs::path(home ? home : "") / ".dolphin-emu" / "Load" / "Textures",
        fs::path(home ? home : "") / ".var" / "app" / "org.DolphinEmu.dolphin-emu" / "data" / "dolphin-emu" / "Load" / "Textures",
    };
    std::error_code ec;
    for (auto& c : candidates)
        if (fs::is_directory(c, ec)) return c;
    return candidates[0];
}

bool Platform_ReplaceFile(const std::string& from, const std::string& to) { return rename(from.c_str(), to.c_str()) == 0; }

// Platform_RunProcess lives in runtime/platform/run_process.cpp

std::string Platform_UiFont()
{
    static const char* const kFonts[] = {
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",   // Debian, Ubuntu
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",            // Arch, SteamOS
        "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",     // Fedora
        "/usr/share/fonts/google-noto-vf/NotoSans[wght].ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
    };
    for (const char* f : kFonts)
        if (access(f, R_OK) == 0) return f;
    return {};
}

std::vector<std::string> Platform_UiFallbackFonts()
{
    // the first Noto CJK collection found (all four scripts), then symbols
    static const char* const kCjk[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",                 // Debian, Ubuntu
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",                      // Arch, SteamOS
        "/usr/share/fonts/google-noto-sans-cjk-vf-fonts/NotoSansCJK-VF.ttc",      // Fedora
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    };
    static const char* const kSymbols[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    };
    std::vector<std::string> fonts;
    auto first = [&](const auto& list) {
        for (const char* f : list)
            if (access(f, R_OK) == 0)
            {
                fonts.push_back(f);
                return;
            }
    };
    first(kCjk);
    first(kSymbols);
    return fonts;
}

std::string Platform_Utf8(const wchar_t* s)
{
    std::string out;
    for (; *s; s++)
    {
        uint32_t c = (uint32_t)*s;
        if (c < 0x80) out += (char)c;
        else if (c < 0x800) { out += (char)(0xC0 | c >> 6); out += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { out += (char)(0xE0 | c >> 12); out += (char)(0x80 | (c >> 6 & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
        else { out += (char)(0xF0 | c >> 18); out += (char)(0x80 | (c >> 12 & 0x3F)); out += (char)(0x80 | (c >> 6 & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
    }
    return out;
}

// ---------------------------------------------------------------- time
static uint64_t NowNs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

uint64_t Platform_TickMs() { return NowNs() / 1000000; }
uint64_t Platform_PerfCounter() { return NowNs(); }
uint64_t Platform_PerfFrequency() { return 1000000000ull; }

uint64_t Platform_ProcessRssMb()
{
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return 0;
    char line[256];
    uint64_t kb = 0;
    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "VmRSS: %llu", (unsigned long long*)&kb) == 1) break;
    }
    fclose(f);
    return kb / 1024;
}

void Platform_SleepMs(uint32_t ms)
{
    timespec ts{ (time_t)(ms / 1000), (long)(ms % 1000) * 1000000 };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
}

// ---------------------------------------------------------------- threads
void Thread_SetName(const char* name)
{
    char n[16];
    snprintf(n, sizeof(n), "%s", name);
    pthread_setname_np(pthread_self(), n);
}

void Thread_SetPriority(ThreadPriority p)
{
    // raising priority needs privileges (fails quietly without them)
    setpriority(PRIO_PROCESS, (id_t)gettid(), p == ThreadPriority::TimeCritical ? -10 : 5);
}

namespace
{
struct Event
{
    std::mutex m;
    std::condition_variable cv;
    bool set = false;
};
}   // namespace

PlatformEvent::PlatformEvent() : impl(new Event()) {}
PlatformEvent::~PlatformEvent() { delete (Event*)impl; }

void PlatformEvent::Set()
{
    Event* e = (Event*)impl;
    {
        std::lock_guard lk(e->m);
        e->set = true;
    }
    e->cv.notify_one();
}

static void WaitNs(Event* e, uint64_t ns, bool infinite)
{
    std::unique_lock lk(e->m);
    if (infinite) e->cv.wait(lk, [&] { return e->set; });
    else e->cv.wait_for(lk, std::chrono::nanoseconds(ns), [&] { return e->set; });
    e->set = false;
}

void PlatformEvent::Wait(uint32_t ms) { WaitNs((Event*)impl, (uint64_t)ms * 1000000, ms == 0xFFFFFFFFu); }

void Platform_TimerThreadSetup()
{
    Thread_SetPriority(ThreadPriority::TimeCritical);
    prctl(PR_SET_TIMERSLACK, 1, 0, 0, 0);   // wake up on time (the default slack is 50 us)
}

void Platform_TimerWait(PlatformEvent& wake, uint64_t hns)
{
    // at most a second at a time (no deadline at all shows up as a huge value)
    WaitNs((Event*)wake.impl, std::clamp<uint64_t>(hns, 10, 10000000) * 100, false);
}

// ---------------------------------------------------------------- memory
void* Platform_AllocPages(size_t bytes)
{
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? nullptr : p;
}

// ---------------------------------------------------------------- dialogs
MsgResult Platform_MessageBox(const std::string& title, const std::string& text, MsgButtons buttons, MsgIcon icon)
{
    std::vector<SDL_MessageBoxButtonData> b;
    auto add = [&](MsgResult r, const char* label, SDL_MessageBoxButtonFlags flags) { b.push_back({ flags, (int)r, label }); };
    switch (buttons)
    {
    case MsgButtons::Ok: add(MsgResult::Ok, "OK", SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT); break;
    case MsgButtons::OkCancel:
        add(MsgResult::Ok, "OK", SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT);
        add(MsgResult::Cancel, "Cancel", SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT);
        break;
    case MsgButtons::YesNoCancel:
        add(MsgResult::Yes, "Yes", SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT);
        add(MsgResult::No, "No", 0);
        add(MsgResult::Cancel, "Cancel", SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT);
        break;
    }
    SDL_MessageBoxData d{};
    d.flags = icon == MsgIcon::Error ? SDL_MESSAGEBOX_ERROR : icon == MsgIcon::Question ? SDL_MESSAGEBOX_WARNING : SDL_MESSAGEBOX_INFORMATION;
    d.title = title.c_str();
    d.message = text.c_str();
    d.numbuttons = (int)b.size();
    d.buttons = b.data();
    int pressed = (int)MsgResult::Cancel;
    if (!SDL_ShowMessageBox(&d, &pressed))
    {
        LOG_WARN("cannot show a message box ({}): {}", SDL_GetError(), text);
        return buttons == MsgButtons::Ok ? MsgResult::Ok : MsgResult::Cancel;
    }
    return pressed < 0 ? MsgResult::Cancel : (MsgResult)pressed;
}

void Platform_ErrorBox(const std::string& title, const std::string& text)
{
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), text.c_str(), nullptr))
        fprintf(stderr, "%s: %s\n", title.c_str(), text.c_str());
}
