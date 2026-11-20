#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Windows Music Library host-dependency manager (Python embed, cricodecs, vgmstream, LGPL FFmpeg).
// Downloads into %LOCALAPPDATA%\SonicColorsRecomp\dependencies. No-op on non-Windows.

struct MusicDepsProgress
{
    std::atomic<bool> running{ false };
    std::atomic<bool> cancel{ false };
    std::atomic<uint64_t> bytesDone{ 0 };
    std::atomic<uint64_t> bytesTotal{ 0 };
    // 0=idle 1=download 2=verify 3=extract 4=configure 5=done 6=error
    std::atomic<int> phase{ 0 };
};

struct MusicDepsPackageStatus
{
    std::string id;
    std::string version;
    std::string purpose;
    bool installed = false;
    bool usable = false;
    std::string path;   // resolved exe or install dir
    std::string error;
};

struct MusicDepsStatus
{
    bool supported = false;          // false on Linux
    bool ready = false;              // all required managed packages usable (or overridden)
    bool busy = false;
    std::string root;
    std::string managedBin;
    std::string python;
    std::string message;
    std::string lastError;
    std::vector<MusicDepsPackageStatus> packages;
};

// LOCALAPPDATA\SonicColorsRecomp\dependencies (Windows); empty elsewhere.
std::filesystem::path MusicDeps_Root();
std::filesystem::path MusicDeps_ManagedBin();
std::filesystem::path MusicDeps_Python();

// Apply SCR_MUSIC_MANAGED_BIN / SCR_MUSIC_PYTHON to the process environment (Win32 API).
void MusicDeps_ApplyEnv();

MusicDepsStatus MusicDeps_Status();
MusicDepsProgress& MusicDeps_Progress();

// Start background ensure if not already running. Windows only.
bool MusicDeps_EnsureAsync(bool force = false);
void MusicDeps_Cancel();

// True when original preview / prepare should wait for managed setup (Windows + not ready).
bool MusicDeps_BlocksHostOps();
