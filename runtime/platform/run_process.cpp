// Cross-platform child process launch for Music Library and other tools.
#include "platform.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
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

namespace {
unsigned g_lastRunProcessError = 0;

#ifdef _WIN32
std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// CommandLineToArgvW-compatible quoting (spaces, quotes, trailing backslashes).
std::wstring QuoteWinArg(const std::wstring& arg)
{
    if (arg.empty()) return L"\"\"";
    const bool need = arg.find_first_of(L" \t\"") != std::wstring::npos;
    if (!need) return arg;
    std::wstring out = L"\"";
    int bs = 0;
    for (wchar_t c : arg)
    {
        if (c == L'\\')
        {
            ++bs;
            continue;
        }
        if (c == L'"')
        {
            out.append(bs * 2 + 1, L'\\');
            out += L'"';
            bs = 0;
            continue;
        }
        if (bs)
        {
            out.append(bs, L'\\');
            bs = 0;
        }
        out += c;
    }
    if (bs) out.append(bs * 2, L'\\');
    out += L'"';
    return out;
}

std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

void WriteCreateProcessDiag(const fs::path& stderrPath, const std::wstring& cmd, DWORD err)
{
    if (stderrPath.empty()) return;
    std::ofstream f(stderrPath, std::ios::binary | std::ios::trunc);
    if (!f) return;
    f << "Platform_RunProcess: CreateProcessW failed (Win32=" << err << ")\n";
    const std::string utf8 = WideToUtf8(cmd);
    if (utf8.size() <= 1200)
        f << "cmdline: " << utf8 << "\n";
    else
        f << "cmdline: " << utf8.substr(0, 1200) << "…\n";
}
#endif
} // namespace

unsigned Platform_LastRunProcessError() { return g_lastRunProcessError; }

bool Platform_RunProcess(const std::vector<std::string>& argv,
                         const fs::path& stdoutPath,
                         const fs::path& stderrPath,
                         int& exitCode)
{
    exitCode = -1;
    g_lastRunProcessError = 0;
    if (argv.empty() || argv[0].empty()) return false;

#ifdef _WIN32
    std::wstring cmd;
    for (size_t i = 0; i < argv.size(); i++)
    {
        if (i) cmd += L' ';
        cmd += QuoteWinArg(Utf8ToWide(argv[i]));
    }
    const std::wstring app = Utf8ToWide(argv[0]);

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE hOut = INVALID_HANDLE_VALUE, hErr = INVALID_HANDLE_VALUE;
    HANDLE hNullIn = INVALID_HANDLE_VALUE, hNullOut = INVALID_HANDLE_VALUE, hNullErr = INVALID_HANDLE_VALUE;
    auto openWrite = [&](const fs::path& p, HANDLE& h) -> bool {
        if (p.empty()) return true;
        h = CreateFileW(p.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
        return h != INVALID_HANDLE_VALUE;
    };
    if (!openWrite(stdoutPath, hOut) || !openWrite(stderrPath, hErr))
    {
        g_lastRunProcessError = GetLastError();
        if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
        if (hErr != INVALID_HANDLE_VALUE) CloseHandle(hErr);
        return false;
    }

    const bool redirect = hOut != INVALID_HANDLE_VALUE || hErr != INVALID_HANDLE_VALUE;
    STARTUPINFOW si{ sizeof(si) };
    if (redirect)
    {
        // GUI parents often have invalid/non-inheritable console std handles. With
        // STARTF_USESTDHANDLES every slot must be a valid inheritable handle or
        // CreateProcess returns ERROR_INVALID_PARAMETER (87).
        auto openNul = [&](DWORD access) -> HANDLE {
            return CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
        };
        hNullIn = openNul(GENERIC_READ);
        if (hOut == INVALID_HANDLE_VALUE) hNullOut = openNul(GENERIC_WRITE);
        if (hErr == INVALID_HANDLE_VALUE) hNullErr = openNul(GENERIC_WRITE);
        if (hNullIn == INVALID_HANDLE_VALUE ||
            (hOut == INVALID_HANDLE_VALUE && hNullOut == INVALID_HANDLE_VALUE) ||
            (hErr == INVALID_HANDLE_VALUE && hNullErr == INVALID_HANDLE_VALUE))
        {
            g_lastRunProcessError = GetLastError();
            if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
            if (hErr != INVALID_HANDLE_VALUE) CloseHandle(hErr);
            if (hNullIn != INVALID_HANDLE_VALUE) CloseHandle(hNullIn);
            if (hNullOut != INVALID_HANDLE_VALUE) CloseHandle(hNullOut);
            if (hNullErr != INVALID_HANDLE_VALUE) CloseHandle(hNullErr);
            return false;
        }
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = hNullIn;
        si.hStdOutput = hOut != INVALID_HANDLE_VALUE ? hOut : hNullOut;
        si.hStdError = hErr != INVALID_HANDLE_VALUE ? hErr : hNullErr;
    }

    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = cmd;
    // Prefer lpApplicationName for absolute paths; fall back to command-line search.
    const wchar_t* appName = nullptr;
    if (!app.empty() && (app.find(L'\\') != std::wstring::npos || app.find(L'/') != std::wstring::npos))
        appName = app.c_str();

    BOOL ok = CreateProcessW(appName, mutableCmd.data(), nullptr, nullptr, redirect ? TRUE : FALSE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    const DWORD createErr = ok ? 0 : GetLastError();

    if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
    if (hErr != INVALID_HANDLE_VALUE) CloseHandle(hErr);
    if (hNullIn != INVALID_HANDLE_VALUE) CloseHandle(hNullIn);
    if (hNullOut != INVALID_HANDLE_VALUE) CloseHandle(hNullOut);
    if (hNullErr != INVALID_HANDLE_VALUE) CloseHandle(hNullErr);

    if (!ok)
    {
        g_lastRunProcessError = createErr;
        WriteCreateProcessDiag(stderrPath, cmd, createErr);
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    exitCode = (int)code;
    return true;
#else
    std::vector<char*> args;
    args.reserve(argv.size() + 1);
    for (const auto& s : argv) args.push_back(const_cast<char*>(s.c_str()));
    args.push_back(nullptr);

    int outFd = -1, errFd = -1;
    auto openOut = [](const fs::path& p) -> int {
        if (p.empty()) return -1;
        return open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    };
    outFd = openOut(stdoutPath);
    errFd = openOut(stderrPath);
    if ((!stdoutPath.empty() && outFd < 0) || (!stderrPath.empty() && errFd < 0))
    {
        g_lastRunProcessError = (unsigned)errno;
        if (outFd >= 0) close(outFd);
        if (errFd >= 0) close(errFd);
        return false;
    }

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
    if (rc != 0)
    {
        g_lastRunProcessError = (unsigned)rc;
        if (!stderrPath.empty())
        {
            std::ofstream f(stderrPath, std::ios::binary | std::ios::trunc);
            if (f) f << "Platform_RunProcess: posix_spawnp failed (errno=" << rc << ")\n";
        }
        return false;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        g_lastRunProcessError = (unsigned)errno;
        return false;
    }
    if (WIFEXITED(status)) exitCode = WEXITSTATUS(status);
    else exitCode = 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 1);
    return true;
#endif
}
