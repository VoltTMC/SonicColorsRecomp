#pragma once
// Operating system services used by the runtime. Each platform implements them once:
// platform/win32 (Win32 API) and platform/linux (POSIX and SDL3).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

// ---------------------------------------------------------------- process
// Called first thing in main(): keeps the command line for restarts, and on Windows
// opts out of power throttling.
void Platform_Init(int argc, char** argv);
// Runs the game (guest code never returns). Windows: on this thread. Linux: on a thread
// with a large stack, while this (the main) thread handles the window's events.
[[noreturn]] void Platform_RunGame(void (*game)());
// Logs what a bug report needs to know about the host (Wine, HLSL compiler, ...).
void Platform_LogHost();
// Ends the process at once, without running any cleanup (other threads may be in the
// middle of something).
[[noreturn]] void Platform_Terminate(int code);
// Ends the process the normal way (Windows: ExitProcess).
[[noreturn]] void Platform_ExitProcess(int code);
// Starts the game again with the same command line (plus --skip-launcher when asked),
// then ends this process (terminate: as Platform_Terminate, else Platform_ExitProcess).
[[noreturn]] void Platform_Restart(bool skipLauncher, bool terminate);
// Path of the running AppImage (`$APPIMAGE`), or empty when not an AppImage / on Windows.
std::string Platform_AppImagePath();
// Spawns `path` with the original argv (argv[0] replaced), then ends this process.
[[noreturn]] void Platform_RelaunchExecutable(const std::string& path);
// This process id (for deferred update helpers that wait until we exit).
uint32_t Platform_ProcessId();
// Start argv[0]… without waiting (false if spawn failed).
bool Platform_SpawnDetached(const std::vector<std::string>& argv);

// ---------------------------------------------------------------- paths and files
// The directory of the executable (read-only files shipped with it).
std::filesystem::path Platform_ExeDir();
// Force config/saves/logs next to the executable (call before first Platform_UserDir).
// Also enabled by SCR_PORTABLE=1 or --portable.
void Platform_RequestPortable();
// True when user data is kept next to the executable (portable ZIP / --portable).
bool Platform_IsPortable();
// True when the exe directory contains installed.txt (Windows installer layout).
bool Platform_IsInstalledLayout();
// Where the game writes: log, config.ini, saves (nand), shader cache, screenshots and,
// by default, the installed game data. Linux: exe dir when writable / portable, else
// $XDG_DATA_HOME/SonicColorsRecomp. Windows: installed.txt → %APPDATA%\SonicColorsRecomp;
// portable.txt / legacy exe data / --portable → exe dir; else %APPDATA%\SonicColorsRecomp
// (or SCR_USER_DIR). Linux under RecompLauncher: its data folder (below).
std::filesystem::path Platform_UserDir();
// Linux: RecompLauncher's data folder (RECOMPLAUNCHER_DATA), or null when not started by
// it. The launcher's backups, cloud saves and Uninstall look after this folder, not the files
// beside the executable, so when this is set it is the user directory, over --portable and
// SCR_PORTABLE. Null on Windows.
inline const char* Platform_LauncherDataDir()
{
#ifdef _WIN32
    return nullptr;
#else
    const char* d = std::getenv("RECOMPLAUNCHER_DATA");
    return d && *d ? d : nullptr;
#endif
}
// Writable drop-in tools for Music Library (UserDir/tools/bin). Created on demand.
std::filesystem::path Platform_MusicToolsBinDir();
// Dolphin's custom texture folder (Load/Textures) for this user.
std::filesystem::path Platform_DolphinTextureDir();
// Replaces `to` with `from` in one step (config.ini is written to a temporary file first).
bool Platform_ReplaceFile(const std::string& from, const std::string& to);

// Run argv[0] with remaining args (no shell). Optional stdout/stderr file redirects
// (empty path = inherit). Unicode-safe on Windows. Sets exitCode when started.
bool Platform_RunProcess(const std::vector<std::string>& argv,
                         const std::filesystem::path& stdoutPath,
                         const std::filesystem::path& stderrPath,
                         int& exitCode);
// After a failed Platform_RunProcess: Win32 GetLastError, or posix_spawnp/errno; else 0.
unsigned Platform_LastRunProcessError();
// A UI font of the system (empty: use ImGui's own).
std::string Platform_UiFont();
// fonts merged after it for Chinese, Japanese and Korean text and symbols (existing files only)
std::vector<std::string> Platform_UiFallbackFonts();
// UTF-8 of a wide string (UTF-16 on Windows, UTF-32 elsewhere).
std::string Platform_Utf8(const wchar_t* s);

// ---------------------------------------------------------------- time
uint64_t Platform_TickMs();          // milliseconds since some point (GetTickCount64)
uint64_t Platform_PerfCounter();     // high-resolution counter
uint64_t Platform_PerfFrequency();   // its ticks per second
void Platform_SleepMs(uint32_t ms);
// Working-set / RSS of this process in mebibytes (0 if unknown).
uint64_t Platform_ProcessRssMb();

// ---------------------------------------------------------------- threads
enum class ThreadPriority { BelowNormal, TimeCritical };
void Thread_SetName(const char* name);
void Thread_SetPriority(ThreadPriority p);

// Auto-reset event: Set wakes one waiter (or the next one to wait).
class PlatformEvent
{
public:
    PlatformEvent();
    ~PlatformEvent();
    PlatformEvent(const PlatformEvent&) = delete;
    PlatformEvent& operator=(const PlatformEvent&) = delete;
    void Set();
    void Wait(uint32_t ms);
    void* impl;
};

// The scheduler's timer thread: high priority and waits that are precise to well under a
// millisecond even when the window is minimized.
void Platform_TimerThreadSetup();
// Waits until `wake` is set or `hns` (100 ns units) have passed.
void Platform_TimerWait(PlatformEvent& wake, uint64_t hns);

// ---------------------------------------------------------------- memory
// Zeroed, page-aligned memory that is never freed.
void* Platform_AllocPages(size_t bytes);

// ---------------------------------------------------------------- dialogs
enum class MsgButtons { Ok, OkCancel, YesNoCancel };
enum class MsgIcon { Info, Question, Error };
enum class MsgResult { Ok, Cancel, Yes, No };
MsgResult Platform_MessageBox(const std::string& title, const std::string& text, MsgButtons buttons, MsgIcon icon);
// The fatal error / crash box (no buttons to choose from).
void Platform_ErrorBox(const std::string& title, const std::string& text);
