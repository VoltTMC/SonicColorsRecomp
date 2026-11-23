// A3y unit tests: SFD header sniff, scan/precedence, unused names, invalid reject.
#include "../../runtime/ios/movie_mod.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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
    } while (0)

// Stubs — movie_mod.cpp pulls these when linked into the full game; the test only
// needs ScanDir / LooksLikeSfd (no Disc_RegisterHostOverride).
void Log_Write(int, const std::string&) {}
#include "../../runtime/config.h"
Config g_config;
std::filesystem::path Platform_UserDir() { return fs::temp_directory_path() / "scr_movie_mod_test_user"; }
std::filesystem::path Platform_ExeDir() { return fs::temp_directory_path() / "scr_movie_mod_test_exe"; }
bool Platform_IsPortable() { return false; }

// disc.cpp symbols pulled if we linked movie_mod Init — we only test scan helpers.
void Disc_RegisterHostOverride(const std::string&, const std::filesystem::path&) {}
void Disc_ClearHostOverrides() {}

static void WriteBytes(const fs::path& p, const uint8_t* data, size_t n)
{
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write((const char*)data, (std::streamsize)n);
}

int main()
{
    // --- header sniff ---
    const uint8_t pack[] = { 0x00, 0x00, 0x01, 0xBA, 0x21, 0x00, 0x01, 0x00 };
    const uint8_t pad[] = { 0x00, 0x00, 0x01, 0xBF, 0x00, 0x00, 0x00, 0x00 };
    const uint8_t junk[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A };  // PNG
    CHECK(MovieMod_LooksLikeSfd(pack, sizeof(pack)));
    CHECK(MovieMod_LooksLikeSfd(pad, sizeof(pad)));
    CHECK(!MovieMod_LooksLikeSfd(junk, sizeof(junk)));
    CHECK(!MovieMod_LooksLikeSfd(nullptr, 0));

    const std::vector<std::string> known = { "movie/ev110.sfd", "movie/ev350.sfd",
                                             "movie/segalogo_us.sfd" };

    fs::path root = fs::temp_directory_path() / "scr_movie_mod_test_root";
    fs::remove_all(root);
    fs::create_directories(root);

    // Valid pack header as ev110
    WriteBytes(root / "ev110.sfd", pack, sizeof(pack));
    // Valid via mirrored movie/ subfolder
    WriteBytes(root / "movie" / "ev350.sfd", pad, sizeof(pad));
    // Unknown name
    WriteBytes(root / "not_a_real_movie.sfd", pack, sizeof(pack));
    // Invalid header matching a real name
    WriteBytes(root / "segalogo_us.sfd", junk, sizeof(junk));
    // Path traversal attempt ignored (unsafe name skipped by SafeFileName — written as normal)
    WriteBytes(root / "files" / "movie" / "ev110.sfd", pack, sizeof(pack));  // duplicate

    auto cands = MovieMod_ScanDir(root, known);
    int valid = 0, unused = 0, rejected = 0;
    bool sawEv110 = false, sawEv350 = false;
    for (auto& c : cands)
    {
        if (c.valid)
        {
            valid++;
            if (c.discRel == "movie/ev110.sfd") sawEv110 = true;
            if (c.discRel == "movie/ev350.sfd") sawEv350 = true;
        }
        else if (c.rejectReason == "no matching original movie")
            unused++;
        else
            rejected++;
    }
    CHECK(sawEv110);
    CHECK(sawEv350);
    CHECK(valid >= 2);
    CHECK(unused >= 1);    // not_a_real_movie.sfd
    CHECK(rejected >= 1);  // segalogo_us bad header

    fs::remove_all(root);
    if (g_fail)
    {
        fprintf(stderr, "movie_mod_test: %d failure(s)\n", g_fail);
        return 1;
    }
    printf("movie_mod_test OK\n");
    return 0;
}
