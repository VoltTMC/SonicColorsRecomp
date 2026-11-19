#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Shared music-mod manifest (resource-based) + legacy stage catalog.

struct MusicAssetMapping
{
    std::string id;          // assets.<id>
    std::string archive;     // sonic2010_0.cpk
    std::string path;        // internal CPK path
    std::string source;      // relative WAV under music dir
    std::string stage;       // optional stg110
    std::string variant;     // main / fx
    std::string cue;
    bool enabled = true;
    double loopStart = 0.0;
};

// Legacy stage-keyed track (still read/written for migration).
struct MusicTrack
{
    std::string id;
    std::string stage;
    std::string cue;
    std::string file;
    bool enabled = true;
    bool loop = true;
    float volume = 1.f;
    int fadeInMs = 500;
    int fadeOutMs = 750;
    double loopStart = 0.0;
    double loopEnd = 0.0;
};

struct MusicStageInfo
{
    const char* stage;
    const char* cue;
    const char* world;
    int act;
    const char* label;
};

const std::vector<MusicStageInfo>& MusicManifest_Stages();
const char* MusicManifest_DefaultCue(const std::string& stage);
const MusicStageInfo* MusicManifest_FindStage(const std::string& stage);

bool MusicManifest_Load(const std::filesystem::path& path, std::vector<MusicTrack>& out, std::string& error);
bool MusicManifest_Save(const std::filesystem::path& path, const std::vector<MusicTrack>& tracks, std::string& error);

bool MusicManifest_LoadAssets(const std::filesystem::path& path, std::vector<MusicAssetMapping>& out, std::string& error);
bool MusicManifest_SaveAssets(const std::filesystem::path& path, const std::vector<MusicAssetMapping>& assets, std::string& error);

struct MusicTrackStatus
{
    enum class Kind { Original, Enabled, Disabled, Missing, Invalid } kind = Kind::Original;
    std::string detail;
};

MusicTrackStatus MusicManifest_Status(const std::filesystem::path& musicDir, const MusicTrack* t);
MusicTrackStatus MusicManifest_AssetStatus(const std::filesystem::path& musicDir, const MusicAssetMapping* a);

std::string MusicManifest_SafeFileName(const std::string& stage, const std::filesystem::path& src);
std::string MusicManifest_ImportWav(const std::filesystem::path& musicDir, const std::string& stage,
                                    const std::filesystem::path& src, std::string& error);

std::filesystem::path MusicManifest_DefaultDir();
std::filesystem::path MusicManifest_Path(const std::filesystem::path& musicDir);

MusicAssetMapping* MusicManifest_FindAssetByPath(std::vector<MusicAssetMapping>& assets, const std::string& path);
