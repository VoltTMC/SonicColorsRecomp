// Unit checks for MusicPaths_* (CPK + cache root resolution).
#include "../../runtime/audio/music_paths.h"
#include "../../runtime/config.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

Config g_config;

static fs::path g_exe;
static fs::path g_user;
fs::path Platform_ExeDir() { return g_exe; }
fs::path Platform_UserDir() { return g_user; }
// Stub — music_paths only needs Find for status.toolFound.
std::filesystem::path MusicTool_Find() { return {}; }

static int g_fail = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            g_fail++;                                                                                                  \
        }                                                                                                              \
    } while (0)

static void Touch(const fs::path& p)
{
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << "x";
}

int main()
{
    const fs::path root = fs::temp_directory_path() / "scr_music_paths_test";
    std::error_code ec;
    fs::remove_all(root, ec);

    g_exe = root / "install";
    g_user = root / "userdata";
    g_config = {};
    fs::create_directories(g_exe, ec);
    fs::create_directories(g_user, ec);

    // Portable Windows layout: CPK next to exe under game/extracted/DATA
    const fs::path portableCpk =
        g_exe / "game" / "extracted" / "DATA" / "files" / "sonic2010_0.cpk";
    Touch(portableCpk);

    // Empty cwd mods should not matter; change into an empty temp cwd.
    const fs::path elsewhere = root / "elsewhere";
    fs::create_directories(elsewhere, ec);
    const fs::path prev = fs::current_path();
    fs::current_path(elsewhere, ec);

    fs::path found = MusicPaths_OriginalCpk();
    CHECK(fs::equivalent(found, portableCpk, ec) || found == portableCpk
          || found.filename() == "sonic2010_0.cpk");
    CHECK(fs::exists(found));

    // dataDir override wins when present
    const fs::path dataCpk = root / "customdata" / "files" / "sonic2010_0.cpk";
    Touch(dataCpk);
    g_config.dataDir = (root / "customdata").string();
    found = MusicPaths_OriginalCpk();
    CHECK(fs::exists(found));
    CHECK(found.filename() == "sonic2010_0.cpk");
    // Prefer configured data dir
    {
        auto a = fs::weakly_canonical(found, ec);
        auto b = fs::weakly_canonical(dataCpk, ec);
        CHECK(a == b);
    }

    // Cache root must be under user dir (not cwd/userdata) so Prepare and Play agree.
    g_config.dataDir.clear();
    const fs::path cache = MusicPaths_CacheRoot();
    CHECK(cache.string().find("music_mods") != std::string::npos);
    auto cacheCanon = fs::weakly_canonical(cache, ec);
    auto userCanon = fs::weakly_canonical(g_user / "music_mods", ec);
    CHECK(cacheCanon == userCanon);

    // Status before current.json
    auto st = MusicPaths_QueryStatus();
    CHECK(st.cpkExists);
    CHECK(!st.cacheReady);
    CHECK(st.summary.find("not prepared") != std::string::npos);

    // After writing current.json (patched)
    {
        fs::create_directories(cache, ec);
        std::ofstream out(cache / "current.json");
        out << "{\"use_original\":false,\"patched_cpk\":\"x\"}\n";
    }
    st = MusicPaths_QueryStatus();
    CHECK(st.cacheReady);
    CHECK(!st.useOriginal);
    CHECK(st.summary.find("ready") != std::string::npos);

    fs::current_path(prev, ec);
    fs::remove_all(root, ec);

    if (g_fail)
    {
        fprintf(stderr, "%d checks failed\n", g_fail);
        return 1;
    }
    puts("music_paths_test OK");
    return 0;
}
