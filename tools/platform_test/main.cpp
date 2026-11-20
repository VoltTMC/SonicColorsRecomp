// Music tool discovery + Platform_RunProcess stubs (Unicode path / subprocess).
#include "../../runtime/audio/music_tool.h"
#include "../../runtime/platform/platform.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace fs = std::filesystem;

static fs::path s_exeDir;
static int g_fail = 0;

#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            g_fail++;                                                                                                  \
        }                                                                                                              \
        else                                                                                                           \
            printf("PASS %s\n", #cond);                                                                                \
    } while (0)

fs::path Platform_ExeDir() { return s_exeDir; }

bool Platform_RunProcess(const std::vector<std::string>& argv,
                         const fs::path& stdoutPath,
                         const fs::path& stderrPath,
                         int& exitCode)
{
    exitCode = -1;
    if (argv.empty()) return false;
#ifdef _WIN32
    auto widen = [](const std::string& s) {
        std::wstring w(MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), (int)w.size());
        return w;
    };
    auto quote = [](const std::wstring& a) {
        if (a.find_first_of(L" \t\"") == std::wstring::npos) return a;
        std::wstring o = L"\"";
        for (wchar_t c : a) o += (c == L'"') ? L"\\\"" : std::wstring(1, c);
        return o + L'"';
    };
    std::wstring cmd;
    for (size_t i = 0; i < argv.size(); i++)
    {
        if (i) cmd += L' ';
        cmd += quote(widen(argv[i]));
    }
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE hOut = INVALID_HANDLE_VALUE, hErr = INVALID_HANDLE_VALUE;
    if (!stdoutPath.empty())
        hOut = CreateFileW(stdoutPath.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!stderrPath.empty())
        hErr = CreateFileW(stderrPath.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
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
    BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                             &si, &pi);
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
#else
    std::vector<char*> args;
    for (auto& s : argv) args.push_back(const_cast<char*>(s.c_str()));
    args.push_back(nullptr);
    int outFd = -1, errFd = -1;
    if (!stdoutPath.empty()) outFd = open(stdoutPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!stderrPath.empty()) errFd = open(stderrPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    if (outFd >= 0)
    {
        posix_spawn_file_actions_adddup2(&fa, outFd, STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&fa, outFd);
    }
    if (errFd >= 0)
    {
        posix_spawn_file_actions_adddup2(&fa, errFd, STDERR_FILENO);
        posix_spawn_file_actions_addclose(&fa, errFd);
    }
    pid_t pid = 0;
    int rc = posix_spawnp(&pid, args[0], &fa, nullptr, args.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    if (outFd >= 0) close(outFd);
    if (errFd >= 0) close(errFd);
    if (rc != 0) return false;
    int st = 0;
    waitpid(pid, &st, 0);
    exitCode = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
    return true;
#endif
}

// Unused stubs required if something pulls more of platform.h (not linked).
void Platform_Init(int, char**) {}
void Platform_RequestPortable() {}
fs::path Platform_UserDir() { return s_exeDir; }
bool Platform_ReplaceFile(const std::string& from, const std::string& to)
{
    std::error_code ec;
    fs::rename(from, to, ec);
    return !ec;
}

int main()
{
    fs::path tmp = fs::temp_directory_path() / "scr_platform_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp / "tools");
    s_exeDir = tmp;

#ifdef _WIN32
    const char* toolName = "music_asset.exe";
#else
    const char* toolName = "music_asset";
#endif
    {
        std::ofstream(tmp / "tools" / toolName) << "#!/bin/sh\necho ok\n";
#ifndef _WIN32
        fs::permissions(tmp / "tools" / toolName, fs::perms::owner_all);
#endif
    }

#ifdef _WIN32
    _putenv_s("SCR_MUSIC_ASSET", "");
#else
    unsetenv("SCR_MUSIC_ASSET");
#endif
    // Prefer the synthetic exe dir over the real repo cwd (which also has tools/).
    fs::current_path(tmp);
    fs::path found = MusicTool_Find();
    CHECK(found.filename() == toolName);
    CHECK(found.parent_path().filename() == "tools");

    // Unicode + spaces in path
    fs::path uni = tmp / u8"path with spaces and café";
    fs::create_directories(uni);
    fs::path out = uni / "out.txt";
    fs::path err = uni / "err.txt";
    int code = -1;
#ifdef _WIN32
    bool started = Platform_RunProcess({ "cmd.exe", "/c", "echo hello" }, out, err, code);
#else
    bool started = Platform_RunProcess({ "echo", "hello" }, out, err, code);
#endif
    CHECK(started);
    CHECK(code == 0);
    {
        std::ifstream in(out);
        std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(s.find("hello") != std::string::npos);
    }

    // SCR_MUSIC_ASSET override
    fs::path overrideTool = tmp / "override_tool";
    std::ofstream(overrideTool) << "x";
#ifdef _WIN32
    _putenv_s("SCR_MUSIC_ASSET", overrideTool.string().c_str());
#else
    setenv("SCR_MUSIC_ASSET", overrideTool.string().c_str(), 1);
#endif
    std::error_code ec;
    CHECK(MusicTool_Find() == fs::weakly_canonical(overrideTool, ec));
#ifdef _WIN32
    _putenv_s("SCR_MUSIC_ASSET", "");
#else
    unsetenv("SCR_MUSIC_ASSET");
#endif

    fs::remove_all(tmp);
    if (g_fail) { fprintf(stderr, "%d checks failed\n", g_fail); return 1; }
    printf("platform_test OK\n");
    return 0;
}
