#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Windows Music Library host-dependency manager (Python embed, cricodecs, vgmstream, LGPL FFmpeg).
// Downloads into %LOCALAPPDATA%\SonicColorsRecomp\dependencies. No-op on non-Windows.
// Installed files + verification are authoritative; metadata alone never forces a redownload.

struct MusicDepsProgress
{
    std::atomic<bool> running{ false };
    std::atomic<bool> cancel{ false };
    std::atomic<uint64_t> bytesDone{ 0 };
    std::atomic<uint64_t> bytesTotal{ 0 };
    // 0=idle 1=download 2=verify_dl 3=extract 4=install 5=verify_install 6=ready 7=error
    std::atomic<int> phase{ 0 };
};

struct MusicDepsPackageStatus
{
    std::string id;
    std::string version;
    std::string purpose;
    std::string state;           // not_installed | files_present | linked | ready | failed | repair
    bool installed = false;      // primary files present on disk
    bool usable = false;         // ready for Music Library use
    std::string path;            // resolved executable / module path
    std::string installDir;
    std::string error;
};

struct MusicDepsStatus
{
    bool supported = false;
    bool ready = false;
    bool busy = false;
    std::string root;
    std::string managedBin;
    std::string python;
    std::string message;
    std::string lastError;
    std::vector<MusicDepsPackageStatus> packages;
};

std::filesystem::path MusicDeps_Root();
std::filesystem::path MusicDeps_ManagedBin();
std::filesystem::path MusicDeps_Python();

void MusicDeps_ApplyEnv();

MusicDepsStatus MusicDeps_Status();
MusicDepsProgress& MusicDeps_Progress();

bool MusicDeps_EnsureAsync(bool force = false);
void MusicDeps_Cancel();

// Soft UI signal: managed tools not all ready (user PATH/drop-ins may still work).
bool MusicDeps_BlocksHostOps();
