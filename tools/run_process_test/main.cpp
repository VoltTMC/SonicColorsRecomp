// Regression tests for Platform_RunProcess (shared Win/Linux implementation).
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
#endif

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
        else                                                                                                           \
            printf("PASS %s\n", #cond);                                                                                \
    } while (0)

static std::string ReadFile(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main()
{
    fs::path tmp = fs::temp_directory_path() / "scr_run_process_test";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp / "dir with spaces" / u8"café", ec);
    CHECK(!ec);

    const fs::path spaced = tmp / "dir with spaces" / u8"café";
    const fs::path out = spaced / "out.txt";
    const fs::path err = spaced / "err.txt";
    int code = -1;

    // 1–2: no args / multiple args + stdout redirect
#ifdef _WIN32
    CHECK(Platform_RunProcess({ "cmd.exe", "/c", "echo", "hello" }, out, err, code));
#else
    CHECK(Platform_RunProcess({ "echo", "hello" }, out, {}, code));
#endif
    CHECK(code == 0);
    CHECK(ReadFile(out).find("hello") != std::string::npos);

    // 3: argument containing spaces
    code = -1;
#ifdef _WIN32
    CHECK(Platform_RunProcess({ "cmd.exe", "/c", "echo", "one two" }, out, err, code));
#else
    CHECK(Platform_RunProcess({ "echo", "one two" }, out, {}, code));
#endif
    CHECK(code == 0);
    CHECK(ReadFile(out).find("one two") != std::string::npos);

    // 4: quotation marks in argument
    code = -1;
#ifdef _WIN32
    CHECK(Platform_RunProcess({ "cmd.exe", "/c", "echo", "say \"hi\"" }, out, err, code));
#else
    CHECK(Platform_RunProcess({ "printf", "%s", "say \"hi\"" }, out, {}, code));
#endif
    CHECK(code == 0);
    CHECK(ReadFile(out).find("hi") != std::string::npos);

    // 5: Unicode argument
    code = -1;
    const std::string naive = "na\xc3\xafve"; // UTF-8 naïve
#ifdef _WIN32
    CHECK(Platform_RunProcess({ "cmd.exe", "/c", "echo", naive }, out, err, code));
#else
    CHECK(Platform_RunProcess({ "printf", "%s", naive }, out, {}, code));
#endif
    CHECK(code == 0);
    CHECK(!ReadFile(out).empty());

    // 6–9: custom paths with spaces + stdout/stderr redirect (working dir is process cwd;
    // abstraction does not take cwd — verify redirects into Unicode/spaced paths).
    code = -1;
#ifdef _WIN32
    CHECK(Platform_RunProcess({ "cmd.exe", "/c", "echo out& echo err 1>&2" }, out, err, code));
#else
    CHECK(Platform_RunProcess({ "sh", "-c", "echo out; echo err >&2" }, out, err, code));
#endif
    CHECK(code == 0);
    CHECK(ReadFile(out).find("out") != std::string::npos);
    CHECK(ReadFile(err).find("err") != std::string::npos);

    // 10: non-zero child exit code
    code = -1;
#ifdef _WIN32
    CHECK(Platform_RunProcess({ "cmd.exe", "/c", "exit /b 42" }, out, err, code));
#else
    CHECK(Platform_RunProcess({ "sh", "-c", "exit 42" }, out, err, code));
#endif
    CHECK(code == 42);

    // 11: missing executable — creation failure, not a fake child exit
    code = -1;
    const bool missingOk =
        Platform_RunProcess({ (tmp / "no_such_tool_xyz").string(), "a" }, out, err, code);
    CHECK(!missingOk);
    CHECK(Platform_LastRunProcessError() != 0);
    {
        const std::string e = ReadFile(err);
        CHECK(e.find("failed") != std::string::npos || e.find("CreateProcess") != std::string::npos ||
              e.find("posix_spawnp") != std::string::npos);
    }

    // 12: path containing spaces (exe path itself — use system shell still; verify
    // quoted redirect paths already covered; also launch via absolute cmd/sh path)
#ifdef _WIN32
    {
        wchar_t sys[MAX_PATH];
        GetSystemDirectoryW(sys, MAX_PATH);
        fs::path cmdPath = fs::path(sys) / "cmd.exe";
        code = -1;
        CHECK(Platform_RunProcess({ cmdPath.string(), "/c", "echo spaced-exe" }, out, err, code));
        CHECK(code == 0);
        CHECK(ReadFile(out).find("spaced-exe") != std::string::npos);
    }
#else
    code = -1;
    CHECK(Platform_RunProcess({ "/bin/echo", "spaced-exe" }, out, err, code));
    CHECK(code == 0);
    CHECK(ReadFile(out).find("spaced-exe") != std::string::npos);
#endif

    fs::remove_all(tmp, ec);
    if (g_fail)
    {
        fprintf(stderr, "%d checks failed\n", g_fail);
        return 1;
    }
    printf("run_process_test OK\n");
    return 0;
}
