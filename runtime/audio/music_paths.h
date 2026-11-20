#pragma once
#include <filesystem>
#include <string>

// Shared Music Library path resolution (launcher UI + in-game music_mod).
// Prefer Platform_UserDir / exe-adjacent layouts; do not depend on cwd alone.

std::filesystem::path MusicPaths_OriginalCpk();
std::filesystem::path MusicPaths_CacheRoot();
std::filesystem::path MusicPaths_PreviewCache();

// Human-readable diagnostics for the Music tab (no secrets).
struct MusicPathsStatus
{
    std::filesystem::path cpk;
    bool cpkExists = false;
    std::filesystem::path cacheRoot;
    std::filesystem::path currentJson;
    bool cacheReady = false;       // current.json exists and is readable
    bool useOriginal = true;       // true when no patched override is active
    std::filesystem::path tool;
    bool toolFound = false;
    std::string summary;           // one-line UI status
    std::string detail;            // longer diagnostic
};

MusicPathsStatus MusicPaths_QueryStatus();
