#include "releases.h"

#include <cstdio>
#include <iterator>

namespace
{
constexpr KnownFile kFilesSNCP8P[] = {
#include "files_sncp8p.inc"
};

constexpr uint64_t TitleId(const char* id)
{
    return 0x00010000'00000000ull | ((uint64_t)(uint8_t)id[0] << 24) | ((uint64_t)(uint8_t)id[1] << 16) |
           ((uint64_t)(uint8_t)id[2] << 8) | (uint8_t)id[3];
}
}   // namespace

const GameRelease kReleases[1] = {
    { "SNCP8P", "Europe", TitleId("SNCP"), "EUR", "RVL-001(EUR)", "LEH", "PAL", "EU", 0x6E,
      kFilesSNCP8P, std::size(kFilesSNCP8P) },
};

const char* const kReleaseNames = "Europe (SNCP8P / Sonic Colors PAL)";

const GameRelease* Release_Find(const std::string& gameId)
{
    for (auto& r : kReleases)
        if (gameId == r.gameId) return &r;
    return nullptr;
}

std::string Release_InstalledId(const std::filesystem::path& data)
{
    char id[6] = {};
#ifdef _WIN32
    FILE* f = _wfopen((data / "sys" / "boot.bin").c_str(), L"rb");
#else
    FILE* f = fopen((data / "sys" / "boot.bin").c_str(), "rb");
#endif
    if (!f) return {};
    size_t n = fread(id, 1, 6, f);
    fclose(f);
    return n == 6 ? std::string(id, 6) : std::string();
}
