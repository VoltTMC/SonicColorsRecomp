#include "music_paths.h"
#include "music_tool.h"
#include "../config.h"
#include "../platform/platform.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace
{
fs::path EnsureWritable(const fs::path& dir)
{
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

bool FileExists(const fs::path& p)
{
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}
}   // namespace

fs::path MusicPaths_OriginalCpk()
{
    const fs::path exe = Platform_ExeDir();
    const fs::path cwd = fs::current_path();
    const fs::path user = Platform_UserDir();
    const fs::path cands[] = {
        fs::path(g_config.dataDir) / "files" / "sonic2010_0.cpk",
        // Installed / AppData layout (game data under user dir).
        user / "game" / "extracted" / "DATA" / "files" / "sonic2010_0.cpk",
        user / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk",
        // Portable Windows / redistributable: game data next to the exe.
        exe / "game" / "extracted" / "DATA" / "files" / "sonic2010_0.cpk",
        exe / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk",
        // Repo / launcher cwd layouts.
        cwd / "game" / "extracted" / "DATA" / "files" / "sonic2010_0.cpk",
        cwd / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk",
        // Dev tree when binary lives in out/ or out-windows/.
        exe / ".." / "game" / "extracted" / "DATA" / "files" / "sonic2010_0.cpk",
        exe / ".." / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk",
        exe / "../.." / "game" / "extracted" / "DATA" / "files" / "sonic2010_0.cpk",
        exe / "../.." / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk",
    };
    for (const fs::path& p : cands)
    {
        if (p.empty() || p.filename() != "sonic2010_0.cpk") continue;
        if (FileExists(p))
        {
            std::error_code ec;
            return fs::weakly_canonical(p, ec);
        }
    }
    // Best-effort default for error messages (portable layout).
    if (!g_config.dataDir.empty())
        return fs::path(g_config.dataDir) / "files" / "sonic2010_0.cpk";
    return exe / "game" / "extracted" / "DATA" / "files" / "sonic2010_0.cpk";
}

fs::path MusicPaths_CacheRoot()
{
    // Must match music_mod / game startup: prefer user dir so Prepare and Play agree
    // even when the launcher is started from a different working directory.
    const fs::path exe = Platform_ExeDir();
    const fs::path cands[] = {
        Platform_UserDir() / "music_mods",
        exe / "music_mods",
        exe / "userdata" / "music_mods",
        fs::current_path() / "userdata" / "music_mods",
    };
    for (const fs::path& p : cands)
    {
        std::error_code ec;
        fs::create_directories(p, ec);
        if (fs::is_directory(p, ec)) return fs::weakly_canonical(p, ec);
    }
    return EnsureWritable(Platform_UserDir() / "music_mods");
}

fs::path MusicPaths_PreviewCache()
{
    const fs::path exe = Platform_ExeDir();
    const fs::path cands[] = {
        Platform_UserDir() / "music_preview",
        exe / "music_preview",
        exe / "userdata" / "music_preview",
        fs::current_path() / "userdata" / "music_preview",
    };
    for (const fs::path& p : cands)
    {
        std::error_code ec;
        fs::create_directories(p, ec);
        if (fs::is_directory(p, ec)) return fs::weakly_canonical(p, ec);
    }
    return EnsureWritable(Platform_UserDir() / "music_preview");
}

MusicPathsStatus MusicPaths_QueryStatus()
{
    MusicPathsStatus st;
    st.cpk = MusicPaths_OriginalCpk();
    st.cpkExists = FileExists(st.cpk);
    st.cacheRoot = MusicPaths_CacheRoot();
    st.currentJson = st.cacheRoot / "current.json";
    st.tool = MusicTool_Find();
    st.toolFound = !st.tool.empty() && FileExists(st.tool);

    std::error_code ec;
    st.cacheReady = FileExists(st.currentJson);
    st.useOriginal = true;
    if (st.cacheReady)
    {
        try
        {
            std::ifstream in(st.currentJson);
            auto j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_object())
            {
                st.useOriginal = j.value("use_original", false);
                if (!j.contains("use_original") && j.contains("patched_cpk"))
                    st.useOriginal = false;
            }
        }
        catch (...)
        {
            st.cacheReady = false;
        }
    }

    if (!st.cpkExists)
    {
        st.summary = "Original CPK not found — install game data first";
        st.detail = "Looked for sonic2010_0.cpk under the configured data dir and next to the executable.";
    }
    else if (!st.cacheReady)
    {
        st.summary = "Replacement CPK: not prepared yet (using original soundtrack)";
        st.detail = "Assign WAV replacements, then Prepare. current.json is created only after a successful prepare.";
    }
    else if (st.useOriginal)
    {
        st.summary = "Replacement CPK: using original soundtrack";
        st.detail = st.currentJson.string();
    }
    else
    {
        st.summary = "Replacement CPK ready";
        st.detail = st.currentJson.string();
    }

    if (st.cpkExists && !st.toolFound)
    {
        if (!st.detail.empty()) st.detail += " ";
        st.detail += "music_asset tool missing — original preview/prepare need tools/music_asset "
                     "(see MUSIC_ASSET_WINDOWS.txt).";
    }
    return st;
}
