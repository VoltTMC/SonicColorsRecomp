// Process, paths, time, threads and dialogs on Windows.
#include "../platform.h"
#include "../../log.h"

#include <algorithm>
#include <windows.h>
#include <timeapi.h>

namespace fs = std::filesystem;

void Gamepad_StopRumble();

// ---------------------------------------------------------------- process
void Platform_Init(int, char**)
{
    // Per-monitor DPI so the window and ImGui framebuffer scale correctly on high-DPI displays.
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    using DPI_AWARENESS_CONTEXT = HANDLE;
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)(LONG_PTR)-4)
#endif
    using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
    {
        if (auto fn = (SetDpiAwarenessContextFn)(void*)GetProcAddress(user32, "SetProcessDpiAwarenessContext"))
            fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
    // Keep full timer resolution and CPU speed even when the window is minimized or in
    // the background (Windows 11 power throttling would coarsen all waits to ~15.6 ms).
    PROCESS_POWER_THROTTLING_STATE pt{};
    pt.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    pt.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    pt.StateMask = 0;
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &pt, sizeof(pt));
}

void Platform_RunGame(void (*game)())
{
    // the window has its own thread (gx/video.cpp); the game runs on the main thread,
    // whose stack is large (/STACK in CMakeLists.txt)
    game();
    Platform_ExitProcess(0);
}

// What a bug report needs to tell graphics problems apart: Wine/Proton (its D3D11 is DXVK
// or wined3d, its HLSL compiler vkd3d-shader) and which d3dcompiler DLL is loaded.
void Platform_LogHost()
{
    using WineVersionFn = const char*(__cdecl*)();
    if (auto wineVersion = (WineVersionFn)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version"))
        LOG_INFO("running under Wine {}", wineVersion());
    if (HMODULE dc = GetModuleHandleW(L"d3dcompiler_47.dll"))
    {
        wchar_t path[MAX_PATH];
        char utf8[MAX_PATH * 3];
        GetModuleFileNameW(dc, path, MAX_PATH);
        WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), nullptr, nullptr);
        LOG_INFO("HLSL compiler: {}", utf8);
    }
}

void Platform_Terminate(int code)
{
    TerminateProcess(GetCurrentProcess(), (UINT)code);
    for (;;) {}
}

void Platform_ExitProcess(int code) { ExitProcess((UINT)code); }

void Platform_Restart(bool skipLauncher, bool terminate)
{
    std::wstring cmd = GetCommandLineW();
    if (skipLauncher && cmd.find(L"--skip-launcher") == std::wstring::npos) cmd += L" --skip-launcher";
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(exe, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    else
        LOG_WARN("restart failed ({})", GetLastError());
    Gamepad_StopRumble();
    if (terminate) Platform_Terminate(0);
    Platform_ExitProcess(0);
}

// ---------------------------------------------------------------- paths and files
fs::path Platform_ExeDir()
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path();
}

static bool s_portable = false;
void Platform_RequestPortable() { s_portable = true; }

fs::path Platform_UserDir()
{
    if (s_portable) return Platform_ExeDir();
    if (const char* env = getenv("SCR_USER_DIR"))
        if (env[0]) return fs::path(env);
    const fs::path exe = Platform_ExeDir();
    std::error_code ec;
    // Portable ZIP / legacy layout: keep data next to the exe when already present or marked.
    if (fs::exists(exe / "portable.txt", ec) || fs::exists(exe / "config.ini", ec)
        || fs::exists(exe / "userdata", ec) || fs::exists(exe / "nand", ec))
        return exe;
    if (const char* appdata = getenv("APPDATA"))
        return fs::path(appdata) / "SonicColorsRecomp";
    return exe;
}

fs::path Platform_DolphinTextureDir()
{
    const char* up = getenv("USERPROFILE");
    return up ? fs::path(up) / "Documents" / "Dolphin Emulator" / "Load" / "Textures" : fs::path();
}

bool Platform_ReplaceFile(const std::string& from, const std::string& to)
{
    auto widen = [](const std::string& s) {
        std::wstring w(MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), (int)w.size());
        return w;
    };
    return MoveFileExW(widen(from).c_str(), widen(to).c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

bool Platform_RunProcess(const std::vector<std::string>& argv,
                         const fs::path& stdoutPath,
                         const fs::path& stderrPath,
                         int& exitCode)
{
    exitCode = -1;
    if (argv.empty() || argv[0].empty()) return false;

    auto widen = [](const std::string& s) {
        std::wstring w(MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), (int)w.size());
        return w;
    };

    // Quote each arg for CreateProcess command line (UTF-16).
    auto quote = [](const std::wstring& a) {
        if (a.find_first_of(L" \t\"") == std::wstring::npos) return a;
        std::wstring o = L"\"";
        for (wchar_t c : a)
        {
            if (c == L'"') o += L"\\\"";
            else o += c;
        }
        o += L'"';
        return o;
    };

    std::wstring cmd;
    for (size_t i = 0; i < argv.size(); i++)
    {
        if (i) cmd += L' ';
        cmd += quote(widen(argv[i]));
    }

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE hOut = INVALID_HANDLE_VALUE, hErr = INVALID_HANDLE_VALUE;
    auto openOut = [&](const fs::path& p, HANDLE& h) -> bool {
        if (p.empty()) return true;
        std::wstring wp = p.wstring();
        h = CreateFileW(wp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
        return h != INVALID_HANDLE_VALUE;
    };
    if (!openOut(stdoutPath, hOut) || !openOut(stderrPath, hErr))
    {
        if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
        if (hErr != INVALID_HANDLE_VALUE) CloseHandle(hErr);
        return false;
    }

    STARTUPINFOW si{ sizeof(si) };
    if (hOut != INVALID_HANDLE_VALUE || hErr != INVALID_HANDLE_VALUE)
    {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = hOut != INVALID_HANDLE_VALUE ? hOut : GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = hErr != INVALID_HANDLE_VALUE ? hErr : GetStdHandle(STD_ERROR_HANDLE);
    }

    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = cmd;
    BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                             nullptr, nullptr, &si, &pi);
    if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
    if (hErr != INVALID_HANDLE_VALUE) CloseHandle(hErr);
    if (!ok) return false;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    exitCode = (int)code;
    return true;
}

std::string Platform_UiFont()
{
    char dir[MAX_PATH];
    UINT n = GetWindowsDirectoryA(dir, MAX_PATH);
    std::string font = n ? std::string(dir, n) + "\\Fonts\\segoeui.ttf" : std::string();
    return !font.empty() && GetFileAttributesA(font.c_str()) != INVALID_FILE_ATTRIBUTES ? font : std::string();
}

std::vector<std::string> Platform_UiFallbackFonts()
{
    std::vector<std::string> fonts;
    char dir[MAX_PATH];
    UINT n = GetWindowsDirectoryA(dir, MAX_PATH);
    if (!n) return fonts;
    for (const char* file : { "msyh.ttc", "msjh.ttc", "YuGothM.ttc", "malgun.ttf", "seguisym.ttf" })
    {
        std::string path = std::string(dir, n) + "\\Fonts\\" + file;
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) fonts.push_back(path);
    }
    return fonts;
}

std::string Platform_Utf8(const wchar_t* s)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, s, -1, out.data(), n, nullptr, nullptr);
    return out;
}

// ---------------------------------------------------------------- time
uint64_t Platform_TickMs() { return GetTickCount64(); }

uint64_t Platform_PerfCounter()
{
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (uint64_t)q.QuadPart;
}

uint64_t Platform_PerfFrequency()
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return (uint64_t)f.QuadPart;
}

void Platform_SleepMs(uint32_t ms) { Sleep(ms); }

// ---------------------------------------------------------------- threads
void Thread_SetName(const char* name)
{
    wchar_t w[64];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, w, 64);
    SetThreadDescription(GetCurrentThread(), w);
}

void Thread_SetPriority(ThreadPriority p)
{
    SetThreadPriority(GetCurrentThread(), p == ThreadPriority::TimeCritical ? THREAD_PRIORITY_TIME_CRITICAL : THREAD_PRIORITY_BELOW_NORMAL);
}

PlatformEvent::PlatformEvent() : impl(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {}
PlatformEvent::~PlatformEvent() { CloseHandle(impl); }
void PlatformEvent::Set() { SetEvent(impl); }
void PlatformEvent::Wait(uint32_t ms) { WaitForSingleObject(impl, ms); }

// Waits use a high-resolution waitable timer: plain Sleep/WaitForSingleObject follow
// the system tick, which Windows 11 keeps at ~15.6 ms for minimized or throttled
// processes regardless of timeBeginPeriod. Late events made the game's audio replay
// stale buffers (stutter) and could delay VI interrupts.
static HANDLE s_hrTimer = nullptr;

void Platform_TimerThreadSetup()
{
    Thread_SetPriority(ThreadPriority::TimeCritical);
    timeBeginPeriod(1);
    s_hrTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!s_hrTimer) LOG_WARN("scheduler: no high-resolution timer, timing may be coarse");
}

void Platform_TimerWait(PlatformEvent& wake, uint64_t hns)
{
    if (s_hrTimer)
    {
        LARGE_INTEGER due;
        due.QuadPart = -(LONGLONG)hns;   // relative
        if (due.QuadPart > -1) due.QuadPart = -1;
        SetWaitableTimerEx(s_hrTimer, &due, 0, nullptr, nullptr, nullptr, 0);
        HANDLE hs[2] = { wake.impl, s_hrTimer };
        WaitForMultipleObjects(2, hs, FALSE, INFINITE);
        CancelWaitableTimer(s_hrTimer);
    }
    else
        WaitForSingleObject(wake.impl, (DWORD)std::max<uint64_t>(1, hns / 10000));
}

// ---------------------------------------------------------------- memory
void* Platform_AllocPages(size_t bytes) { return VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); }

// ---------------------------------------------------------------- dialogs
static std::wstring Widen(const std::string& s)
{
    std::wstring w(MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), (int)w.size());
    return w;
}

MsgResult Platform_MessageBox(const std::string& title, const std::string& text, MsgButtons buttons, MsgIcon icon)
{
    UINT type = buttons == MsgButtons::YesNoCancel ? MB_YESNOCANCEL : buttons == MsgButtons::OkCancel ? MB_OKCANCEL : MB_OK;
    type |= icon == MsgIcon::Error ? MB_ICONERROR : icon == MsgIcon::Question ? MB_ICONQUESTION : MB_ICONINFORMATION;
    switch (MessageBoxW(nullptr, Widen(text).c_str(), Widen(title).c_str(), type))
    {
    case IDOK: return MsgResult::Ok;
    case IDYES: return MsgResult::Yes;
    case IDNO: return MsgResult::No;
    default: return MsgResult::Cancel;
    }
}

void Platform_ErrorBox(const std::string& title, const std::string& text)
{
    MessageBoxA(nullptr, text.c_str(), title.c_str(), MB_ICONERROR);
}
