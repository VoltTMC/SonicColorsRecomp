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

    const fs::path roots[] = {
        fs::current_path(),
        Platform_ExeDir(),
        Platform_ExeDir() / "..",
        Platform_ExeDir() / "../..",
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
    std::vector<std::string> argv;
    argv.reserve(args.size() + 1);
    argv.push_back(tool.string());
    argv.insert(argv.end(), args.begin(), args.end());
    return Platform_RunProcess(argv, stdoutPath, stderrPath, exitCode);
}
