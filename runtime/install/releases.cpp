#include "releases.h"

#include <cstdio>
#include <iterator>
#include <string>

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

std::string Release_UnsupportedMessage(const std::string& gameId)
{
    if (gameId.empty())
        return std::string("Could not read a game ID. Need ") + kReleaseNames + ".";
    // Common other Colors discs — name them so players are not left guessing.
    if (gameId.starts_with("SNCE"))
        return "This is the USA (NTSC-U) Sonic Colors disc (" + gameId +
               "). This build only supports Europe / PAL (SNCP8P). USA support is not available yet.";
    if (gameId.starts_with("SNCJ"))
        return "This is the Japan (NTSC-J) Sonic Colors disc (" + gameId +
               "). This build only supports Europe / PAL (SNCP8P). Japan support is not available yet.";
    if (gameId.starts_with("SNC") && gameId != "SNCP8P")
        return "This looks like a Sonic Colors disc (" + gameId +
               "), but only Europe / PAL SNCP8P is supported by this build.";
    return "The disc/game ID is " + gameId + ", not a supported Sonic Colors release (" + kReleaseNames + ").";
}
