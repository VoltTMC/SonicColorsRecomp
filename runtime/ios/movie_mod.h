#pragma once
#include <filesystem>
#include <string>
#include <vector>

// A3y: loose SFD movie replacements via Disc_RegisterHostOverride.
// Call MovieMod_Init() before Disc_Open (with MusicMod_Init). Original
// Data/files/movie/ and sonic2010_0.cpk are never modified.

void MovieMod_SetDir(const std::string& dir);  // empty → default userdata/movie
void MovieMod_Init();
void MovieMod_Shutdown();

bool MovieMod_Active();
int MovieMod_OverrideCount();
const char* MovieMod_Dir();

// Path helpers (also used by unit tests).
struct MovieModCandidate
{
    std::string discRel;              // e.g. "movie/ev110.sfd"
    std::filesystem::path host;
    uint64_t size = 0;
    bool valid = false;
    std::string rejectReason;         // if !valid
};

// Enumerate .sfd under root (supports flat, movie/, files/movie/).
// knownDiscRels: lower-case "movie/name.sfd" → canonical casing.
std::vector<MovieModCandidate> MovieMod_ScanDir(
    const std::filesystem::path& root,
    const std::vector<std::string>& knownDiscRels);

// True if bytes look like an MPEG-PS / Sofdec pack (pack/system/padding start).
bool MovieMod_LooksLikeSfd(const uint8_t* data, size_t n);

void MovieMod_OnDiscPath(const std::string& path);
