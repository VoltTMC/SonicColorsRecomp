// Unit test for save backup / restore (isolated temp tree; no game / IOS).
#include "../../runtime/ios/nand_backup.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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

int main()
{
    fs::path tmp = fs::temp_directory_path() / "scr_nand_backup_test";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp / "title" / "00010000" / "534e4350" / "data", ec);
    {
        std::ofstream f(tmp / "title" / "00010000" / "534e4350" / "data" / "banner.bin");
        f << "save";
    }

    fs::path bak;
    std::string err;
    CHECK(nand_backup::BackupTree(tmp, bak, err));
    CHECK(fs::exists(bak / "title" / "00010000" / "534e4350" / "data" / "banner.bin"));
    CHECK(bak.string().find("save_backups") != std::string::npos);

    fs::remove_all(tmp / "title", ec);
    CHECK(!fs::exists(tmp / "title" / "00010000" / "534e4350" / "data" / "banner.bin", ec));
    CHECK(nand_backup::RestoreTree(tmp, bak, err));
    CHECK(fs::exists(tmp / "title" / "00010000" / "534e4350" / "data" / "banner.bin"));

    fs::path latest;
    CHECK(nand_backup::LatestBackupIn(tmp, latest));
    CHECK(latest == bak);

    // Second backup (newer name) — ListBackupsIn must be newest-first.
    {
        std::ofstream f(tmp / "title" / "00010000" / "534e4350" / "data" / "banner.bin");
        f << "save2";
    }
    // Ensure a distinct timestamp folder even if BackupTree runs in the same second.
    fs::path bak2 = tmp / "save_backups" / "99991231_235959";
    fs::create_directories(bak2 / "title" / "00010000" / "534e4350" / "data", ec);
    {
        std::ofstream f(bak2 / "title" / "00010000" / "534e4350" / "data" / "banner.bin");
        f << "newer";
    }
    auto listed = nand_backup::ListBackupsIn(tmp);
    CHECK(listed.size() >= 2);
    CHECK(listed.front().filename() == "99991231_235959");
    CHECK(nand_backup::LatestBackupIn(tmp, latest));
    CHECK(latest.filename() == "99991231_235959");

    fs::path empty = tmp / "empty_root";
    fs::create_directories(empty, ec);
    fs::path ignored;
    CHECK(!nand_backup::BackupTree(empty, ignored, err));

    fs::remove_all(tmp, ec);
    if (g_fail)
    {
        fprintf(stderr, "nand_backup_test: %d failure(s)\n", g_fail);
        return 1;
    }
    puts("nand_backup_test OK");
    return 0;
}
