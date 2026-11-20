// Host-side tests for Fs_WriteFileAtomic (NAND save C1 / M7). No game / GUI.
#include "../../runtime/ios/fs_atomic.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

void Log_Write(int, const std::string&) {}

static int Fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static bool ReadAll(const fs::path& p, std::string& out)
{
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), {});
    return true;
}

static bool WriteRaw(const fs::path& p, const std::string& s)
{
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(s.data(), (std::streamsize)s.size());
    return (bool)f;
}

int main()
{
    const fs::path root = fs::temp_directory_path() / "scr_nand_fs_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "title" / "00010000", ec);

    const fs::path dest = root / "title" / "00010000" / "banner.bin";
    const std::string oldPayload = "OLD_SAVE_BYTES";
    const std::string newPayload = "NEW_SAVE_BYTES_LONGER";

    // --- success: create
    if (!Fs_WriteFileAtomic(dest, oldPayload.data(), oldPayload.size()))
        return Fail("atomic create");
    std::string got;
    if (!ReadAll(dest, got) || got != oldPayload) return Fail("read after create");
    if (fs::exists(fs::path(dest.string() + ".tmp"), ec)) return Fail("tmp left after create");

    // --- success: replace
    if (!Fs_WriteFileAtomic(dest, newPayload.data(), newPayload.size()))
        return Fail("atomic replace");
    if (!ReadAll(dest, got) || got != newPayload) return Fail("read after replace");
    if (fs::exists(fs::path(dest.string() + ".tmp"), ec)) return Fail("tmp left after replace");

    // --- write failure: read-only parent → keep previous dest, no tmp
    // (POSIX only — Windows directory READONLY does not block creating files inside.)
#ifndef _WIN32
    if (!WriteRaw(dest, oldPayload)) return Fail("seed for fail case");
    const fs::path parent = dest.parent_path();
    if (chmod(parent.c_str(), 0555) != 0) return Fail("chmod parent 0555");
    const bool failed = !Fs_WriteFileAtomic(dest, newPayload.data(), newPayload.size());
    chmod(parent.c_str(), 0755); // always restore before asserts
    if (!failed) return Fail("expected write failure on read-only parent");
    if (!ReadAll(dest, got) || got != oldPayload) return Fail("dest mutated after failed write");
    if (fs::exists(fs::path(dest.string() + ".tmp"), ec)) return Fail("tmp left after failed write");
#endif

    // --- rename failure: dest path is an existing directory → keep sibling? we use a file dest
    // Make `dest` a directory so rename(tmp→dest) fails; seed a sibling marker.
    fs::remove(dest, ec);
    fs::create_directory(dest, ec);
    const fs::path marker = root / "title" / "00010000" / "marker.txt";
    if (!WriteRaw(marker, "SAFE")) return Fail("marker seed");
    if (Fs_WriteFileAtomic(dest, newPayload.data(), newPayload.size()))
        return Fail("expected rename failure onto directory");
    if (!ReadAll(marker, got) || got != "SAFE") return Fail("marker damaged");
    if (fs::exists(fs::path(dest.string() + ".tmp"), ec)) return Fail("tmp left after rename fail");

    fs::remove_all(root, ec);
    std::printf("nand_fs_test OK\n");
    return 0;
}
