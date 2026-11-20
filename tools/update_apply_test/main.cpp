// Unit checks for update package validation + Windows apply helper script generation.
#include "../../runtime/ui/update_apply.h"

#include <cstdio>
#include <cstdlib>
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

static fs::path WriteFile(const fs::path& p, const std::string& data)
{
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.write(data.data(), (std::streamsize)data.size());
    return p;
}

int main()
{
    CHECK(UpdateApply_PsSingleQuote("a").find("'a'") != std::string::npos);
    CHECK(UpdateApply_PsSingleQuote("a'b") == "'a''b'");

    const std::string ps1 = UpdateApply_WindowsPs1(R"(C:\Games\upd.zip)", R"(C:\Games\Sonic Colors)",
                                                   "SonicColorsRecomp.exe", 4242, R"(C:\Games\downloads\_stage)",
                                                   R"(C:\Games\downloads\apply_update.log)");
    // Regression: old apply_update.cmd wrote %%ZIP%% via fprintf("%s"), so Expand-Archive
    // never saw the real path. Embedded single-quoted paths must appear literally.
    CHECK(ps1.find("%%ZIP%%") == std::string::npos);
    CHECK(ps1.find("%ZIP%") == std::string::npos);
    CHECK(ps1.find(R"(C:\Games\upd.zip)") != std::string::npos);
    CHECK(ps1.find("4242") != std::string::npos);
    CHECK(ps1.find("Expand-Archive") != std::string::npos);
    CHECK(ps1.find("robocopy") != std::string::npos);
    CHECK(ps1.find("Get-Process") != std::string::npos);

    const fs::path tmp = fs::temp_directory_path() / "scr_update_apply_test";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp);

    const fs::path empty = WriteFile(tmp / "empty.bin", "");
    std::string err;
    CHECK(!UpdateApply_FileSizeOk(empty, 0, err));

    const fs::path sized = WriteFile(tmp / "sized.bin", std::string(16, 'x'));
    CHECK(UpdateApply_FileSizeOk(sized, 16, err));
    CHECK(!UpdateApply_FileSizeOk(sized, 15, err));
    CHECK(err.find("does not match") != std::string::npos);

    const fs::path zipOk = WriteFile(tmp / "ok.zip", std::string("PK\x03\x04", 4) + std::string(12, '\0'));
    CHECK(UpdateApply_LooksLikeZip(zipOk));
    CHECK(!UpdateApply_LooksLikeAppImage(zipOk));

    const fs::path elf = WriteFile(tmp / "fake.AppImage", std::string("\x7f" "ELF", 4) + std::string(8, '\0'));
    CHECK(UpdateApply_LooksLikeAppImage(elf));
    CHECK(!UpdateApply_LooksLikeZip(elf));

    // Minimal real zip: `zip` CLI if present; otherwise skip archive-content checks.
    const fs::path payload = tmp / "payload" / "SonicColorsRecomp";
    fs::create_directories(payload);
    WriteFile(payload / "SonicColorsRecomp.exe", "fake-exe");
    const fs::path goodZip = tmp / "good.zip";
    const fs::path travZip = tmp / "trav.zip";
    const int zipRc = std::system(("cd '" + (tmp / "payload").string() + "' && zip -qr '" + goodZip.string()
                                   + "' SonicColorsRecomp")
                                      .c_str());
    if (zipRc == 0)
    {
        CHECK(UpdateApply_ZipContainsExe(goodZip, "SonicColorsRecomp.exe", err));
        // Path-traversal entry
        fs::create_directories(tmp / "travsrc");
        WriteFile(tmp / "travsrc" / "x.exe", "x");
        // zip allows storing odd names; craft with ../ via zip -y is awkward — use Python if available.
        const int py = std::system(("python3 - <<'PY'\n"
                                    "import zipfile\n"
                                    "z=zipfile.ZipFile(r'"
                                    + travZip.string()
                                    + "', 'w')\n"
                                      "z.writestr('../SonicColorsRecomp.exe', b'x')\n"
                                      "z.close()\n"
                                      "PY")
                                       .c_str());
        if (py == 0)
        {
            CHECK(!UpdateApply_ZipContainsExe(travZip, "SonicColorsRecomp.exe", err));
            CHECK(err.find("unsafe") != std::string::npos || err.find("does not contain") != std::string::npos);
        }
        const fs::path badZip = tmp / "bad.zip";
        WriteFile(tmp / "payload" / "only.txt", "no exe here");
        fs::remove(payload / "SonicColorsRecomp.exe", ec);
        std::system(("cd '" + (tmp / "payload").string() + "' && zip -qr '" + badZip.string() + "' .").c_str());
        CHECK(!UpdateApply_ZipContainsExe(badZip, "SonicColorsRecomp.exe", err));
    }
    else
        fprintf(stderr, "note: zip CLI unavailable; skipped archive content checks\n");

    fs::remove_all(tmp, ec);

    if (g_fail)
    {
        fprintf(stderr, "%d checks failed\n", g_fail);
        return 1;
    }
    puts("update_apply_test OK");
    return 0;
}
