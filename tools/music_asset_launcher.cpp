// Thin host launcher for music_asset_pkg (cross-platform).
// Finds a Python interpreter, sets PYTHONPATH to tools/, runs -m music_asset_pkg.cli.
// Built as music_asset.exe (Windows) or music_asset_launcher (dev); the POSIX
// tools/music_asset bash wrapper remains the primary Linux entry for developers.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static fs::path ExeDir()
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path();
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0)
    {
        buf[n] = 0;
        return fs::path(buf).parent_path();
    }
    return fs::current_path();
#endif
}

static fs::path ToolsDir()
{
    const fs::path exe = ExeDir();
    std::error_code ec;
    // Packaged: <root>/tools/music_asset.exe  or  <root>/music_asset.exe with tools/ beside
    for (const fs::path& cand : {
             exe,
             exe / "tools",
             exe / "..",
             exe / ".." / "tools",
             fs::current_path() / "tools",
             fs::current_path(),
         })
    {
        if (fs::exists(cand / "music_asset_pkg" / "cli.py", ec))
            return fs::weakly_canonical(cand, ec);
        if (fs::exists(cand / "tools" / "music_asset_pkg" / "cli.py", ec))
            return fs::weakly_canonical(cand / "tools", ec);
    }
    return exe;
}

static bool ExistsFile(const fs::path& p)
{
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}

static fs::path FindPython(const fs::path& root)
{
#ifdef _WIN32
    const fs::path local[] = {
        root / ".venv-music" / "Scripts" / "python.exe",
        root / ".." / ".venv-music" / "Scripts" / "python.exe",
        ExeDir() / "python" / "python.exe",
        ExeDir() / "tools" / "python" / "python.exe",
    };
#else
    const fs::path local[] = {
        root / ".venv-music" / "bin" / "python",
        root / ".." / ".venv-music" / "bin" / "python",
    };
#endif
    for (const auto& p : local)
        if (ExistsFile(p)) return p;

#ifdef _WIN32
    for (const wchar_t* name : { L"python.exe", L"python3.exe", L"py.exe" })
    {
        wchar_t buf[MAX_PATH];
        if (SearchPathW(nullptr, name, nullptr, MAX_PATH, buf, nullptr))
            return fs::path(buf);
    }
#else
    for (const char* name : { "python3", "python" })
    {
        if (const char* pathEnv = getenv("PATH"))
        {
            std::string path = pathEnv;
            size_t start = 0;
            while (start <= path.size())
            {
                size_t end = path.find(':', start);
                fs::path dir = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
                fs::path cand = dir / name;
                if (ExistsFile(cand)) return cand;
                if (end == std::string::npos) break;
                start = end + 1;
            }
        }
    }
#endif
    return {};
}

#ifdef _WIN32
static int RunWindows(const fs::path& python, const fs::path& tools, int argc, char** argv)
{
    auto widen = [](const std::string& s) {
        std::wstring w(MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), (int)w.size());
        return w;
    };
    auto quote = [](const std::wstring& a) {
        if (a.find_first_of(L" \t\"") == std::wstring::npos) return a;
        std::wstring o = L"\"";
        for (wchar_t c : a)
            o += (c == L'"') ? L"\\\"" : std::wstring(1, c);
        return o + L'"';
    };

    std::wstring cmd = quote(python.wstring()) + L" -m music_asset_pkg.cli";
    for (int i = 1; i < argc; i++)
    {
        cmd += L' ';
        cmd += quote(widen(argv[i]));
    }

    std::wstring envBlock;
    {
        wchar_t* env = GetEnvironmentStringsW();
        std::wstring pyPath = L"PYTHONPATH=" + tools.wstring();
        envBlock = pyPath;
        envBlock.push_back(L'\0');
        if (env)
        {
            for (wchar_t* p = env; *p;)
            {
                std::wstring e = p;
                p += e.size() + 1;
                if (e.rfind(L"PYTHONPATH=", 0) == 0) continue;
                envBlock += e;
                envBlock.push_back(L'\0');
            }
            FreeEnvironmentStringsW(env);
        }
        envBlock.push_back(L'\0');
    }

    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = cmd;
    if (!CreateProcessW(python.wstring().c_str(), mutableCmd.data(), nullptr, nullptr, TRUE, 0,
                        envBlock.data(), tools.wstring().c_str(), &si, &pi))
    {
        fprintf(stderr, "music_asset: CreateProcess failed (%lu)\n", GetLastError());
        return 127;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}
#endif

int main(int argc, char** argv)
{
    const fs::path tools = ToolsDir();
    const fs::path root = tools.parent_path().empty() ? tools : tools.parent_path();
    // Prefer repo root when tools/ is tools/
    fs::path repo = tools.filename() == "tools" ? tools.parent_path() : root;
    const fs::path py = FindPython(repo);
    if (py.empty())
    {
        fprintf(stderr,
                "music_asset: Python not found. Install Python 3.11+ and cricodecs,\n"
                "or set SCR_MUSIC_ASSET to a working launcher. See tools/MUSIC_ASSET_WINDOWS.txt\n");
        return 127;
    }

#ifdef _WIN32
    SetEnvironmentVariableW(L"PYTHONPATH", tools.wstring().c_str());
    return RunWindows(py, tools, argc, argv);
#else
    std::string pyPath = "PYTHONPATH=" + tools.string();
    putenv(strdup(pyPath.c_str())); // intentional leak for process lifetime
    std::vector<char*> args;
    std::string mod = "-m";
    std::string pkg = "music_asset_pkg.cli";
    args.push_back(const_cast<char*>(py.string().c_str()));
    args.push_back(mod.data());
    args.push_back(pkg.data());
    for (int i = 1; i < argc; i++) args.push_back(argv[i]);
    args.push_back(nullptr);
    execv(py.c_str(), args.data());
    perror("music_asset: execv");
    return 127;
#endif
}
