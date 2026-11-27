#include "music_manifest.h"
#include "wav_file.h"
#include "../platform/platform.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace
{
std::string Trim(std::string s)
{
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    return s;
}
std::string Unquote(std::string s)
{
    s = Trim(s);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')))
        s = s.substr(1, s.size() - 2);
    return s;
}
bool ParseBool(const std::string& v, bool def)
{
    std::string s = v;
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    if (s == "true" || s == "1" || s == "yes") return true;
    if (s == "false" || s == "0" || s == "no") return false;
    return def;
}

const MusicStageInfo kStages[] = {
    {"stg110", "bgm_stg110_rso", "Tropical Resort", 1, "Tropical Resort Act 1"},
    {"stg120", "bgm_stg120_rso", "Tropical Resort", 2, "Tropical Resort Act 2"},
    {"stg130", "bgm_stg130_rso", "Tropical Resort", 3, "Tropical Resort Act 3"},
    {"stg140", "bgm_stg140_rso", "Tropical Resort", 4, "Tropical Resort Act 4"},
    {"stg150", "bgm_stg150_rso", "Tropical Resort", 5, "Tropical Resort Act 5"},
    {"stg160", "bgm_stg160_rso", "Tropical Resort", 6, "Tropical Resort Act 6"},
    {"stg210", "bgm_stg210_fct", "Sweet Mountain", 1, "Sweet Mountain Act 1"},
    {"stg220", "bgm_stg220_fct", "Sweet Mountain", 2, "Sweet Mountain Act 2"},
    {"stg230", "bgm_stg230_fct", "Sweet Mountain", 3, "Sweet Mountain Act 3"},
    {"stg240", "bgm_stg240_fct", "Sweet Mountain", 4, "Sweet Mountain Act 4"},
    {"stg250", "bgm_stg250_fct", "Sweet Mountain", 5, "Sweet Mountain Act 5"},
    {"stg260", "bgm_stg260_fct", "Sweet Mountain", 6, "Sweet Mountain Act 6"},
    {"stg310", "bgm_stg310_shp", "Starlight Carnival", 1, "Starlight Carnival Act 1"},
    {"stg320", "bgm_stg320_shp", "Starlight Carnival", 2, "Starlight Carnival Act 2"},
    {"stg330", "bgm_stg330_shp", "Starlight Carnival", 3, "Starlight Carnival Act 3"},
    {"stg340", "bgm_stg340_shp", "Starlight Carnival", 4, "Starlight Carnival Act 4"},
    {"stg350", "bgm_stg350_shp", "Starlight Carnival", 5, "Starlight Carnival Act 5"},
    {"stg360", "bgm_stg360_shp", "Starlight Carnival", 6, "Starlight Carnival Act 6"},
    {"stg410", "bgm_stg410_pla", "Planet Wisp", 1, "Planet Wisp Act 1"},
    {"stg420", "bgm_stg420_pla", "Planet Wisp", 2, "Planet Wisp Act 2"},
    {"stg430", "bgm_stg430_pla", "Planet Wisp", 3, "Planet Wisp Act 3"},
    {"stg440", "bgm_stg440_pla", "Planet Wisp", 4, "Planet Wisp Act 4"},
    {"stg450", "bgm_stg450_pla", "Planet Wisp", 5, "Planet Wisp Act 5"},
    {"stg460", "bgm_stg460_pla", "Planet Wisp", 6, "Planet Wisp Act 6"},
    {"stg510", "bgm_stg510_qua", "Aquarium Park", 1, "Aquarium Park Act 1"},
    {"stg520", "bgm_stg520_qua", "Aquarium Park", 2, "Aquarium Park Act 2"},
    {"stg530", "bgm_stg530_qua", "Aquarium Park", 3, "Aquarium Park Act 3"},
    {"stg540", "bgm_stg540_qua", "Aquarium Park", 4, "Aquarium Park Act 4"},
    {"stg550", "bgm_stg550_qua", "Aquarium Park", 5, "Aquarium Park Act 5"},
    {"stg560", "bgm_stg560_qua", "Aquarium Park", 6, "Aquarium Park Act 6"},
    {"stg610", "bgm_stg610_ast", "Asteroid Coaster", 1, "Asteroid Coaster Act 1"},
    {"stg620", "bgm_stg620_ast", "Asteroid Coaster", 2, "Asteroid Coaster Act 2"},
    {"stg630", "bgm_stg630_ast", "Asteroid Coaster", 3, "Asteroid Coaster Act 3"},
    {"stg640", "bgm_stg640_ast", "Asteroid Coaster", 4, "Asteroid Coaster Act 4"},
    {"stg650", "bgm_stg650_ast", "Asteroid Coaster", 5, "Asteroid Coaster Act 5"},
    {"stg660", "bgm_stg660_ast", "Asteroid Coaster", 6, "Asteroid Coaster Act 6"},
    {"stg710", "bgm_stg710_elv", "Terminal Velocity", 1, "Terminal Velocity Act 1"},
    {"stg720", "bgm_stg720_elv", "Terminal Velocity", 2, "Terminal Velocity Act 2"},
    {"stg730", "bgm_stg730_elv", "Terminal Velocity", 3, "Terminal Velocity Act 3"},
};
} // namespace

const std::vector<MusicStageInfo>& MusicManifest_Stages()
{
    static const std::vector<MusicStageInfo> v(std::begin(kStages), std::end(kStages));
    return v;
}

const char* MusicManifest_DefaultCue(const std::string& stage)
{
    if (const MusicStageInfo* s = MusicManifest_FindStage(stage)) return s->cue;
    return nullptr;
}

const MusicStageInfo* MusicManifest_FindStage(const std::string& stage)
{
    for (auto& s : kStages)
        if (stage == s.stage) return &s;
    return nullptr;
}

bool MusicManifest_Load(const fs::path& path, std::vector<MusicTrack>& out, std::string& error)
{
    out.clear();
    error.clear();
    std::ifstream in(path);
    if (!in)
    {
        error = "manifest not found";
        return false;
    }
    MusicTrack cur;
    bool inTrack = false;
    auto flush = [&]() {
        if (!inTrack) return;
        if (cur.cue.empty() && !cur.stage.empty())
            if (const char* d = MusicManifest_DefaultCue(cur.stage)) cur.cue = d;
        if (cur.id.empty()) cur.id = !cur.stage.empty() ? cur.stage : cur.cue;
        if (!cur.file.empty() && (!cur.cue.empty() || !cur.stage.empty()))
            out.push_back(cur);
        cur = {};
        inTrack = false;
    };
    std::string line;
    while (std::getline(in, line))
    {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[')
        {
            flush();
            if (line.size() > 9 && line.compare(0, 8, "[tracks.") == 0 && line.back() == ']')
            {
                cur.id = line.substr(8, line.size() - 9);
                inTrack = true;
            }
            continue;
        }
        if (!inTrack) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string val = Unquote(line.substr(eq + 1));
        if (key == "stage") cur.stage = val;
        else if (key == "cue") cur.cue = val;
        else if (key == "file") cur.file = val;
        else if (key == "enabled") cur.enabled = ParseBool(val, true);
        else if (key == "loop") cur.loop = ParseBool(val, true);
        else if (key == "volume") cur.volume = std::strtof(val.c_str(), nullptr);
        else if (key == "fade_in_ms") cur.fadeInMs = std::atoi(val.c_str());
        else if (key == "fade_out_ms") cur.fadeOutMs = std::atoi(val.c_str());
        else if (key == "loop_start") cur.loopStart = std::strtod(val.c_str(), nullptr);
        else if (key == "loop_end") cur.loopEnd = std::strtod(val.c_str(), nullptr);
    }
    flush();
    return true;
}

bool MusicManifest_Save(const fs::path& path, const std::vector<MusicTrack>& tracks, std::string& error)
{
    error.clear();
    // Validate: unique cues among enabled+present mappings; known stages preferred
    std::unordered_map<std::string, std::string> cueOwner;
    for (auto& t : tracks)
    {
        if (t.file.empty())
        {
            error = "track '" + t.id + "' has no file";
            return false;
        }
        if (t.file.find("..") != std::string::npos || (!t.file.empty() && (t.file[0] == '/' || t.file[0] == '\\')))
        {
            error = "track '" + t.id + "' has an unsafe path";
            return false;
        }
        std::string cue = t.cue;
        if (cue.empty() && !t.stage.empty())
            if (const char* d = MusicManifest_DefaultCue(t.stage)) cue = d;
        if (cue.empty())
        {
            error = "track '" + t.id + "' has no cue/stage";
            return false;
        }
        auto it = cueOwner.find(cue);
        if (it != cueOwner.end())
        {
            error = "duplicate cue " + cue + " (" + it->second + " and " + t.id + ")";
            return false;
        }
        cueOwner[cue] = t.id;
    }

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out)
        {
            error = "cannot write temporary manifest";
            return false;
        }
        out << "# Sonic Colors Recomp — custom stage music\n";
        out << "# Managed by the launcher Music Mods page and tools/music_mod.\n\n";
        for (auto& t : tracks)
        {
            std::string cue = t.cue;
            if (cue.empty() && !t.stage.empty())
                if (const char* d = MusicManifest_DefaultCue(t.stage)) cue = d;
            out << "[tracks." << t.id << "]\n";
            if (!t.stage.empty()) out << "stage = \"" << t.stage << "\"\n";
            if (!cue.empty()) out << "cue = \"" << cue << "\"\n";
            out << "file = \"" << t.file << "\"\n";
            out << "enabled = " << (t.enabled ? "true" : "false") << "\n";
            out << "loop = " << (t.loop ? "true" : "false") << "\n";
            out << "volume = " << t.volume << "\n";
            out << "fade_in_ms = " << t.fadeInMs << "\n";
            out << "fade_out_ms = " << t.fadeOutMs << "\n";
            if (t.loopStart > 0.0) out << "loop_start = " << t.loopStart << "\n";
            if (t.loopEnd > 0.0) out << "loop_end = " << t.loopEnd << "\n";
            out << "\n";
        }
    }
    fs::rename(tmp, path, ec);
    if (ec)
    {
        // Windows: replace
        fs::remove(path, ec);
        fs::rename(tmp, path, ec);
        if (ec)
        {
            error = "atomic replace failed: " + ec.message();
            return false;
        }
    }
    return true;
}

MusicTrackStatus MusicManifest_Status(const fs::path& musicDir, const MusicTrack* t)
{
    MusicTrackStatus st;
    if (!t)
    {
        st.kind = MusicTrackStatus::Kind::Original;
        st.detail = "Original soundtrack";
        return st;
    }
    if (!t->enabled)
    {
        st.kind = MusicTrackStatus::Kind::Disabled;
        st.detail = "Disabled — original music";
        return st;
    }
    fs::path p = musicDir / t->file;
    std::error_code ec;
    if (!fs::exists(p, ec))
    {
        st.kind = MusicTrackStatus::Kind::Missing;
        st.detail = "Missing file";
        return st;
    }
    WavProbe probe;
    if (!Wav_Probe(p.string(), probe))
    {
        st.kind = MusicTrackStatus::Kind::Invalid;
        st.detail = probe.error.empty() ? "Invalid WAV" : probe.error;
        return st;
    }
    st.kind = MusicTrackStatus::Kind::Enabled;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.1fs · %d Hz · stereo", probe.durationSec, probe.sampleRate);
    st.detail = buf;
    return st;
}

std::string MusicManifest_SafeFileName(const std::string& stage, const fs::path& src)
{
    std::string base = src.filename().string();
    for (char& c : base)
        if (!(std::isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-')) c = '_';
    if (base.size() < 5 || base.find('.') == std::string::npos) base = stage + ".wav";
    // Prefer stage-prefixed name for clarity
    if (base.find(stage) == std::string::npos)
        base = stage + "_" + base;
    return base;
}

std::string MusicManifest_ImportWav(const fs::path& musicDir, const std::string& stage,
                                    const fs::path& src, std::string& error)
{
    error.clear();
    WavData w;
    if (!Wav_Load(src.string(), w))
    {
        error = w.error.empty() ? "invalid WAV" : w.error;
        return {};
    }
    std::error_code ec;
    fs::path tracks = musicDir / "tracks";
    fs::create_directories(tracks, ec);
    // Already inside managed dir?
    fs::path canonSrc = fs::weakly_canonical(src, ec);
    fs::path canonDir = fs::weakly_canonical(musicDir, ec);
    if (!ec && !canonDir.empty())
    {
        auto rel = fs::relative(canonSrc, canonDir, ec);
        // Use generic_string(): path::native() is wchar_t on Windows (MSVC rejects find("..")).
        if (!ec && !rel.empty() && rel.generic_string().find("..") == std::string::npos)
            return rel.generic_string();
    }
    std::string name = MusicManifest_SafeFileName(stage, src);
    fs::path dest = tracks / name;
    fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        error = "copy failed: " + ec.message();
        return {};
    }
    return (fs::path("tracks") / name).generic_string();
}

namespace
{
bool DirWritable(const fs::path& dir)
{
    std::error_code ec;
    fs::create_directories(dir, ec);
    const fs::path probe = dir / ".scr_music_write_probe";
    FILE* f = std::fopen(probe.string().c_str(), "wb");
    if (!f) return false;
    std::fclose(f);
    fs::remove(probe, ec);
    return true;
}

void EnsureCatalogFile(const fs::path& destDir, const fs::path& catalogSrc)
{
    std::error_code ec;
    const fs::path dest = destDir / "catalog.json";
    if (fs::exists(dest, ec) || !fs::is_regular_file(catalogSrc, ec)) return;
    fs::create_directories(destDir, ec);
    fs::copy_file(catalogSrc, dest, fs::copy_options::overwrite_existing, ec);
}
}   // namespace

fs::path MusicManifest_DefaultDir()
{
    // Prefer a directory that already has catalog.json (packaged redistributable or repo).
    // Do not treat an empty mods/music folder as a hit — that hid the packaged catalog on Windows
    // when cwd differed from the exe directory.
    const fs::path root = Platform_ExeDir();
    const fs::path bases[] = {
        root,
        fs::current_path(),
        root / "..",
        root / "../..",
        Platform_UserDir(),
    };
    fs::path catalogDir;
    for (const fs::path& base : bases)
    {
        fs::path p = base / "mods" / "music";
        std::error_code ec;
        if (fs::is_regular_file(p / "catalog.json", ec))
        {
            catalogDir = fs::weakly_canonical(p, ec);
            break;
        }
    }

    if (!catalogDir.empty())
    {
        if (DirWritable(catalogDir) && !Platform_LauncherDataDir()) return catalogDir;
        // AppImage / read-only install / RecompLauncher (which backs up its data folder, not the
        // shipped one): keep catalog, write manifest/tracks under user data.
        const fs::path userMusic = Platform_UserDir() / "mods" / "music";
        EnsureCatalogFile(userMusic, catalogDir / "catalog.json");
        std::error_code canonEc;
        return fs::weakly_canonical(userMusic, canonEc);
    }

    const fs::path fallback = Platform_UserDir() / "mods" / "music";
    std::error_code ec;
    fs::create_directories(fallback, ec);
    if (DirWritable(fallback)) return fs::weakly_canonical(fallback, ec);
    return fs::current_path() / "mods" / "music";
}

fs::path MusicManifest_Path(const fs::path& musicDir) { return musicDir / "manifest.toml"; }

MusicAssetMapping* MusicManifest_FindAssetByPath(std::vector<MusicAssetMapping>& assets, const std::string& path)
{
    for (auto& a : assets)
        if (a.path == path) return &a;
    return nullptr;
}

bool MusicManifest_LoadAssets(const fs::path& path, std::vector<MusicAssetMapping>& out, std::string& error)
{
    out.clear();
    error.clear();
    std::ifstream in(path);
    if (!in)
    {
        error = "manifest not found";
        return false;
    }
    MusicAssetMapping cur;
    bool inAsset = false;
    bool inTrack = false;
    MusicTrack legacy;
    auto flushAsset = [&]() {
        if (!inAsset) return;
        if (cur.id.empty()) cur.id = cur.path;
        if (cur.archive.empty()) cur.archive = "sonic2010_0.cpk";
        if (!cur.path.empty() && !cur.source.empty()) out.push_back(cur);
        cur = {};
        inAsset = false;
    };
    auto flushTrack = [&]() {
        if (!inTrack) return;
        // Legacy → asset (main path via stage id guess is left to Python prepare;
        // here we only keep tracks that already have an explicit file.)
        if (!legacy.file.empty() && !legacy.stage.empty())
        {
            MusicAssetMapping a;
            a.id = "legacy_" + legacy.stage;
            a.archive = "sonic2010_0.cpk";
            a.source = legacy.file;
            a.stage = legacy.stage;
            a.variant = "main";
            a.cue = legacy.cue;
            a.enabled = legacy.enabled;
            a.loopStart = legacy.loopStart;
            // Path filled by GUI from catalog when editing; prepare migrates via STAGE_AAX
            a.path = ""; // resolved later if empty
            // Only add if not already present as asset without path collision on stage
            bool exists = false;
            for (auto& x : out)
                if (x.stage == a.stage && x.variant == "main" && !x.source.empty()) exists = true;
            if (!exists) out.push_back(a);
        }
        legacy = {};
        inTrack = false;
    };
    std::string line;
    while (std::getline(in, line))
    {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[')
        {
            flushAsset();
            flushTrack();
            if (line.size() > 9 && line.compare(0, 8, "[assets.") == 0 && line.back() == ']')
            {
                cur.id = line.substr(8, line.size() - 9);
                inAsset = true;
            }
            else if (line.size() > 9 && line.compare(0, 8, "[tracks.") == 0 && line.back() == ']')
            {
                legacy.id = line.substr(8, line.size() - 9);
                inTrack = true;
            }
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string val = Unquote(line.substr(eq + 1));
        if (inAsset)
        {
            if (key == "archive") cur.archive = val;
            else if (key == "path") cur.path = val;
            else if (key == "source" || key == "file") cur.source = val;
            else if (key == "stage") cur.stage = val;
            else if (key == "variant") cur.variant = val;
            else if (key == "cue") cur.cue = val;
            else if (key == "enabled") cur.enabled = ParseBool(val, true);
            else if (key == "loop_start") cur.loopStart = std::strtod(val.c_str(), nullptr);
        }
        else if (inTrack)
        {
            if (key == "stage") legacy.stage = val;
            else if (key == "cue") legacy.cue = val;
            else if (key == "file") legacy.file = val;
            else if (key == "enabled") legacy.enabled = ParseBool(val, true);
            else if (key == "loop_start") legacy.loopStart = std::strtod(val.c_str(), nullptr);
        }
    }
    flushAsset();
    flushTrack();
    return true;
}

bool MusicManifest_SaveAssets(const fs::path& path, const std::vector<MusicAssetMapping>& assets, std::string& error)
{
    error.clear();
    std::unordered_map<std::string, std::string> pathOwner;
    for (auto& a : assets)
    {
        if (a.path.empty())
        {
            error = "asset '" + a.id + "' has no path";
            return false;
        }
        if (a.source.empty())
        {
            error = "asset '" + a.id + "' has no source WAV";
            return false;
        }
        if (a.source.find("..") != std::string::npos || a.source[0] == '/' || a.source[0] == '\\')
        {
            error = "asset '" + a.id + "' has an unsafe source path";
            return false;
        }
        auto it = pathOwner.find(a.path);
        if (it != pathOwner.end())
        {
            error = "duplicate path " + a.path + " (" + it->second + " and " + a.id + ")";
            return false;
        }
        pathOwner[a.path] = a.id;
    }
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out)
        {
            error = "cannot write temporary manifest";
            return false;
        }
        out << "# Sonic Colors Recomp — music asset replacements\n";
        out << "# Catalog: catalog.json  |  Managed by Music Library\n\n";
        for (auto& a : assets)
        {
            out << "[assets." << a.id << "]\n";
            out << "archive = \"" << (a.archive.empty() ? "sonic2010_0.cpk" : a.archive) << "\"\n";
            out << "path = \"" << a.path << "\"\n";
            out << "source = \"" << a.source << "\"\n";
            out << "enabled = " << (a.enabled ? "true" : "false") << "\n";
            if (!a.stage.empty()) out << "stage = \"" << a.stage << "\"\n";
            if (!a.variant.empty()) out << "variant = \"" << a.variant << "\"\n";
            if (!a.cue.empty()) out << "cue = \"" << a.cue << "\"\n";
            if (a.loopStart > 0.0) out << "loop_start = " << a.loopStart << "\n";
            out << "\n";
        }
    }
    fs::rename(tmp, path, ec);
    if (ec)
    {
        fs::remove(path, ec);
        fs::rename(tmp, path, ec);
        if (ec)
        {
            error = "atomic replace failed: " + ec.message();
            return false;
        }
    }
    return true;
}

MusicTrackStatus MusicManifest_AssetStatus(const fs::path& musicDir, const MusicAssetMapping* a)
{
    MusicTrackStatus st;
    if (!a)
    {
        st.kind = MusicTrackStatus::Kind::Original;
        st.detail = "Original";
        return st;
    }
    if (!a->enabled)
    {
        st.kind = MusicTrackStatus::Kind::Disabled;
        st.detail = "Disabled";
        return st;
    }
    if (a->source.empty())
    {
        st.kind = MusicTrackStatus::Kind::Original;
        st.detail = "No WAV assigned";
        return st;
    }
    fs::path p = musicDir / a->source;
    std::error_code ec;
    if (!fs::exists(p, ec))
    {
        st.kind = MusicTrackStatus::Kind::Missing;
        st.detail = "Missing WAV";
        return st;
    }
    // Header-only — never decode PCM for list/status (Wav_Load was a per-frame UI killer).
    WavProbe probe;
    if (!Wav_Probe(p.string(), probe))
    {
        st.kind = MusicTrackStatus::Kind::Invalid;
        st.detail = probe.error.empty() ? "Invalid WAV" : probe.error;
        return st;
    }
    st.kind = MusicTrackStatus::Kind::Enabled;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.1fs · %d Hz", probe.durationSec, probe.sampleRate);
    st.detail = buf;
    return st;
}
