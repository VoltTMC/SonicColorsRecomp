#include "music_catalog.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace
{
std::string JStr(const nlohmann::json& a, const char* key, const std::string& def = {})
{
    if (!a.contains(key) || a[key].is_null()) return def;
    if (a[key].is_string()) return a[key].get<std::string>();
    if (a[key].is_number_integer()) return std::to_string(a[key].get<int>());
    return def;
}

int JInt(const nlohmann::json& a, const char* key, int def = 0)
{
    if (!a.contains(key) || a[key].is_null()) return def;
    if (a[key].is_number_integer()) return a[key].get<int>();
    if (a[key].is_number_float()) return (int)a[key].get<double>();
    return def;
}
} // namespace

std::filesystem::path MusicCatalog_DefaultPath(const fs::path& musicDir)
{
    return musicDir / "catalog.json";
}

bool MusicCatalog_Load(const fs::path& path, MusicCatalog& out, std::string& error)
{
    out = {};
    error.clear();
    std::ifstream in(path);
    if (!in)
    {
        error = "catalog not found: " + path.string();
        return false;
    }
    nlohmann::json j;
    try
    {
        in >> j;
    }
    catch (const std::exception& ex)
    {
        error = std::string("catalog JSON: ") + ex.what();
        return false;
    }
    out.musicCount = j.value("music_count", 0);
    if (j.contains("archives") && j["archives"].is_array() && !j["archives"].empty())
        out.archiveSha = j["archives"][0].value("sha256", "");
    if (!j.contains("assets") || !j["assets"].is_array())
    {
        error = "catalog missing assets[]";
        return false;
    }
    for (auto& a : j["assets"])
    {
        if (JStr(a, "kind", "music") != "music") continue;
        MusicCatalogAsset e;
        e.id = JStr(a, "id");
        e.archive = JStr(a, "archive", "sonic2010_0.cpk");
        e.path = JStr(a, "path");
        e.filename = JStr(a, "filename");
        e.category = JStr(a, "category", "other");
        e.variant = JStr(a, "variant", "main");
        e.label = JStr(a, "label", e.filename);
        e.cue = JStr(a, "cue");
        e.stage = JStr(a, "stage");
        e.world = JStr(a, "world");
        e.act = JInt(a, "act", 0);
        e.fileId = JInt(a, "file_id", 0);
        e.fileSize = a.contains("file_size") && a["file_size"].is_number()
                         ? a["file_size"].get<uint64_t>()
                         : 0ull;
        e.confidence = a.contains("confidence") && a["confidence"].is_number()
                           ? a["confidence"].get<float>()
                           : 0.f;
        if (a.contains("audio") && a["audio"].is_object())
        {
            auto& au = a["audio"];
            e.sampleRate = JInt(au, "sample_rate", 0);
            e.channels = JInt(au, "channels", 0);
            e.segmentCount = JInt(au, "segment_count", 0);
            e.durationSec = au.contains("duration_sec") && au["duration_sec"].is_number()
                                ? au["duration_sec"].get<double>()
                                : 0.0;
            e.hasLoop = au.contains("has_loop_segments") && au["has_loop_segments"].is_boolean()
                            ? au["has_loop_segments"].get<bool>()
                            : false;
        }
        if (!e.path.empty()) out.assets.push_back(std::move(e));
    }
    out.musicCount = (int)out.assets.size();
    return true;
}
