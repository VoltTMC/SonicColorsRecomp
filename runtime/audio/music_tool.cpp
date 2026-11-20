#include "music_tool.h"
#include "../platform/platform.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

std::filesystem::path MusicTool_Find()
{
    if (const char* env = getenv("SCR_MUSIC_ASSET"))
    {
        fs::path p(env);
        std::error_code ec;
        if (fs::exists(p, ec)) return fs::weakly_canonical(p, ec);
    }

    // Prefer the install tree over cwd so packaged tools/music_asset is found when
    // the launcher is started from a different working directory (common on Windows).
    const fs::path roots[] = {
        Platform_ExeDir(),
        Platform_ExeDir() / "..",
        Platform_ExeDir() / "../..",
        fs::current_path(),
    };

#ifdef _WIN32
    const char* names[] = {
        "music_asset.exe",
        "music_asset.cmd",
        "music_asset.bat",
        "music_asset",
    };
#else
    const char* names[] = { "music_asset" };
#endif

    for (const fs::path& base : roots)
    {
        for (const char* name : names)
        {
            std::error_code ec;
            fs::path p = base / "tools" / name;
            if (fs::exists(p, ec)) return fs::weakly_canonical(p, ec);
            p = base / name;
            if (fs::exists(p, ec)) return fs::weakly_canonical(p, ec);
        }
    }
    return {};
}

bool MusicTool_Run(const std::vector<std::string>& args,
                   const fs::path& stdoutPath,
                   const fs::path& stderrPath,
                   int& exitCode)
{
    const fs::path tool = MusicTool_Find();
    if (tool.empty())
    {
        exitCode = -1;
        return false;
    }
    // Prefer user-writable drop-ins (survive updates) over install-dir tools/bin.
    const fs::path userBin = Platform_MusicToolsBinDir();
#ifdef _WIN32
    _putenv_s("SCR_MUSIC_BIN", userBin.string().c_str());
#else
    setenv("SCR_MUSIC_BIN", userBin.string().c_str(), 1);
#endif
    std::vector<std::string> argv;
    argv.reserve(args.size() + 1);
    argv.push_back(tool.string());
    argv.insert(argv.end(), args.begin(), args.end());
    return Platform_RunProcess(argv, stdoutPath, stderrPath, exitCode);
}
