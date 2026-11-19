#include "music_mod.h"
#include "../config.h"
#include "../ios/disc.h"
#include "../log.h"
#include "../platform/platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

std::string s_dirOverride;
std::string s_dir;
bool s_debug = false;
bool s_active = false;       // patched CPK registered
bool s_useOriginal = true;
std::string s_patchedCpk;
std::string s_statusJson;
std::string s_patchedSha;    // from current.json / status (hex)
bool s_loggedCpkOpen = false;

struct ProofEntry
{
    std::string path;
    uint64_t offset = 0;
    uint64_t size = 0;
    std::string sha256;
    uint8_t first16[16]{};
    bool haveFirst16 = false;
    bool matched = false;
};
std::vector<ProofEntry> s_proof;
std::string s_proofTsv;

static fs::path ExeDir()
{
    return Platform_ExeDir();
}

static fs::path FindMusicAssetTool()
{
    if (const char* env = getenv("SCR_MUSIC_ASSET"))
    {
        fs::path p(env);
        if (fs::exists(p)) return p;
    }
    const fs::path bases[] = {
        fs::current_path(),
        ExeDir(),
        ExeDir() / "..",
        ExeDir() / "../..",
        Platform_ExeDir(),
        Platform_ExeDir() / "..",
        Platform_ExeDir() / "../..",
    };
    for (auto& base : bases)
    {
        fs::path p = base / "tools" / "music_asset";
        std::error_code ec;
        if (fs::exists(p, ec)) return fs::weakly_canonical(p, ec);
    }
    return {};
}

static fs::path DefaultCacheRoot()
{
    // Prefer XDG/user data so AppImage / installed builds don't write into a
    // read-only or cwd-relative tree.
    const fs::path cands[] = {
        Platform_UserDir() / "music_mods",
        fs::current_path() / "userdata" / "music_mods",
        ExeDir() / "userdata" / "music_mods",
        ExeDir() / ".." / "userdata" / "music_mods",
    };
    for (auto& p : cands)
    {
        std::error_code ec;
        fs::create_directories(p, ec);
        if (fs::is_directory(p)) return p;
    }
    return Platform_UserDir() / "music_mods";
}

static std::string JsonGetString(const std::string& json, const char* key)
{
    const std::string pat = std::string("\"") + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos);
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos);
    if (pos == std::string::npos) return {};
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return json.substr(pos + 1, end - pos - 1);
}

static bool JsonGetBool(const std::string& json, const char* key, bool def)
{
    const std::string pat = std::string("\"") + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ')) pos++;
    if (json.compare(pos, 4, "true") == 0) return true;
    if (json.compare(pos, 5, "false") == 0) return false;
    return def;
}

static int HexNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool ParseHex16(const std::string& hex, uint8_t out[16])
{
    if (hex.size() < 32) return false;
    for (int i = 0; i < 16; i++)
    {
        int a = HexNibble(hex[i * 2]);
        int b = HexNibble(hex[i * 2 + 1]);
        if (a < 0 || b < 0) return false;
        out[i] = (uint8_t)((a << 4) | b);
    }
    return true;
}

static void LoadProofTsv(const fs::path& tsv)
{
    s_proof.clear();
    s_proofTsv = tsv.string();
    std::ifstream in(tsv);
    if (!in)
    {
        LOG_WARN("[MUSIC] payload proof TSV missing: {}", tsv.string());
        return;
    }
    std::string line;
    bool header = true;
    while (std::getline(in, line))
    {
        if (header) { header = false; continue; }
        if (line.empty()) continue;
        // path \t offset \t size \t sha \t first16
        ProofEntry e;
        size_t p0 = 0;
        auto next = [&](std::string& out) -> bool {
            size_t p1 = line.find('\t', p0);
            if (p1 == std::string::npos)
            {
                out = line.substr(p0);
                p0 = line.size();
                return !out.empty();
            }
            out = line.substr(p0, p1 - p0);
            p0 = p1 + 1;
            return true;
        };
        std::string offS, sizeS, sha, hex16;
        if (!next(e.path) || !next(offS) || !next(sizeS) || !next(sha)) continue;
        next(hex16);
        e.offset = std::strtoull(offS.c_str(), nullptr, 10);
        e.size = std::strtoull(sizeS.c_str(), nullptr, 10);
        e.sha256 = sha;
        e.haveFirst16 = ParseHex16(hex16, e.first16);
        s_proof.push_back(std::move(e));
    }
    LOG_INFO("[MUSIC] Loaded {} payload proof entries from {}", s_proof.size(), tsv.string());
}

static bool RunPrepare(const fs::path& manifest, const fs::path& musicDir, const fs::path& cpk,
                       const fs::path& cache, std::string& outJson)
{
    const fs::path tool = FindMusicAssetTool();
    if (tool.empty())
    {
        LOG_WARN("[MUSIC] tools/music_asset not found — cannot build patched CPK");
        return false;
    }
    const fs::path outFile = cache / "prepare_last.json";
    fs::create_directories(cache);
    // Default: do not replace FX variants (main BGM only).
    std::string cmd = "\"" + tool.string() + "\" prepare"
                      " --manifest \"" + manifest.string() + "\""
                      " --music-dir \"" + musicDir.string() + "\""
                      " --cpk \"" + cpk.string() + "\""
                      " --cache \"" + cache.string() + "\""
                      " --no-fx"
                      " > \"" + outFile.string() + "\" 2> \"" + (cache / "prepare_last.err").string() + "\"";
    LOG_INFO("[MUSIC] Preparing assets: {}", cmd);
    const int rc = std::system(cmd.c_str());
    std::ifstream in(outFile);
    if (!in)
    {
        LOG_WARN("[MUSIC] prepare produced no JSON (rc={})", rc);
        return false;
    }
    outJson.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (rc != 0)
    {
        LOG_WARN("[MUSIC] prepare failed (rc={}): {}", rc, outJson.empty() ? "(see prepare_last.err)" : outJson);
        return false;
    }
    return true;
}

} // namespace

void MusicMod_SetDir(const std::string& dir) { s_dirOverride = dir; }
void MusicMod_SetDebug(bool on) { s_debug = on; }
void MusicMod_ForceStage(const std::string&) { /* overlay removed */ }
bool MusicMod_Debug() { return s_debug; }

void MusicMod_Init()
{
    s_active = false;
    s_useOriginal = true;
    s_patchedCpk.clear();
    s_statusJson.clear();
    s_patchedSha.clear();
    s_proof.clear();
    s_loggedCpkOpen = false;

    if (!g_config.musicModsEnabled)
    {
        LOG_INFO("[MUSIC] Music mods disabled in settings — original CPK");
        return;
    }
    if (!s_dirOverride.empty())
        s_dir = s_dirOverride;
    else if (!g_config.musicModsDir.empty())
        s_dir = g_config.musicModsDir;
    else
        s_dir = "mods/music";

    const fs::path musicDir = fs::path(s_dir);
    const fs::path manifest = musicDir / "manifest.toml";
    if (!fs::exists(manifest))
    {
        LOG_INFO("[MUSIC] No manifest at {} — original soundtrack", manifest.string());
        return;
    }

    fs::path cpk;
    const fs::path dataCands[] = {
        fs::path(g_config.dataDir) / "files" / "sonic2010_0.cpk",
        fs::current_path() / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk",
        ExeDir() / "game" / "SNCP8P" / "files" / "sonic2010_0.cpk",
    };
    for (auto& p : dataCands)
        if (fs::exists(p)) { cpk = p; break; }
    if (cpk.empty())
    {
        LOG_WARN("[MUSIC] sonic2010_0.cpk not found — cannot patch");
        return;
    }

    const fs::path cache = DefaultCacheRoot();
    std::string json;
    if (!RunPrepare(manifest, musicDir, cpk, cache, json))
    {
        LOG_WARN("[MUSIC] Falling back to original CPK");
        return;
    }
    s_statusJson = json;
    const bool useOrig = JsonGetBool(json, "use_original", true);
    const bool ok = JsonGetBool(json, "ok", false);
    if (!ok)
    {
        LOG_WARN("[MUSIC] Asset prep reported failure — original CPK");
        return;
    }
    if (useOrig)
    {
        LOG_INFO("[MUSIC] {}", JsonGetString(json, "message").empty()
                                   ? "Using original CPK (no active mappings)"
                                   : JsonGetString(json, "message"));
        return;
    }

    s_patchedCpk = JsonGetString(json, "patched_cpk");
    s_patchedSha = JsonGetString(json, "patched_sha256");
    if (s_patchedCpk.empty() || !fs::exists(s_patchedCpk))
    {
        const fs::path cur = cache / "current.json";
        if (fs::exists(cur))
        {
            std::ifstream in(cur);
            std::string cj((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            s_patchedCpk = JsonGetString(cj, "patched_cpk");
            if (s_patchedSha.empty()) s_patchedSha = JsonGetString(cj, "patched_sha256");
            std::string tsv = JsonGetString(cj, "payload_proof_tsv");
            if (!tsv.empty()) LoadProofTsv(tsv);
        }
    }
    if (s_patchedCpk.empty() || !fs::exists(s_patchedCpk))
    {
        LOG_WARN("[MUSIC] Patched CPK path missing — original CPK");
        return;
    }

    // Prefer TSV path from prepare JSON
    {
        std::string tsv = JsonGetString(json, "payload_proof_tsv");
        if (tsv.empty())
        {
            fs::path guess = fs::path(s_patchedCpk).parent_path() / "payload_proof.tsv";
            if (fs::exists(guess)) tsv = guess.string();
        }
        if (!tsv.empty() && s_proof.empty())
            LoadProofTsv(tsv);
    }

    // Register every patched archive listed in prepare status (SNCP8P: one CPK today).
    int registered = 0;
    {
        const std::string& js = s_statusJson;
        size_t pos = js.find("\"overrides\"");
        if (pos != std::string::npos)
        {
            size_t arr = js.find('[', pos);
            size_t end = arr == std::string::npos ? std::string::npos : js.find(']', arr);
            if (arr != std::string::npos && end != std::string::npos)
            {
                std::string block = js.substr(arr, end - arr + 1);
                size_t i = 0;
                while ((i = block.find("\"disc_name\"", i)) != std::string::npos)
                {
                    auto pull = [&](const char* key, size_t from) -> std::string {
                        auto p = block.find(std::string("\"") + key + "\"", from);
                        if (p == std::string::npos || p > from + 200) return {};
                        p = block.find('"', block.find(':', p));
                        if (p == std::string::npos) return {};
                        auto e = block.find('"', p + 1);
                        if (e == std::string::npos) return {};
                        return block.substr(p + 1, e - p - 1);
                    };
                    std::string disc = pull("disc_name", i);
                    std::string host = pull("patched_cpk", i);
                    i += 10;
                    if (disc.empty() || host.empty() || !fs::exists(host)) continue;
                    Disc_RegisterHostOverride(disc, host);
                    registered++;
                    LOG_INFO("[MUSIC] Host override {} → {}", disc, host);
                }
            }
        }
    }
    if (registered == 0)
    {
        Disc_RegisterHostOverride("sonic2010_0.cpk", s_patchedCpk);
        registered = 1;
        LOG_INFO("[MUSIC] Host override sonic2010_0.cpk → {}", s_patchedCpk);
    }
    s_active = true;
    s_useOriginal = false;
    LOG_INFO("[MUSIC] Manifest: {}", manifest.string());
    LOG_INFO("[MUSIC] Original CPK unchanged: {}", cpk.string());
    LOG_INFO("[MUSIC] Patched archive(s): {}  primary={} sha256={}",
             registered, s_patchedCpk,
             s_patchedSha.empty() ? "(see status.json)" : s_patchedSha);
    LOG_INFO("[MUSIC] Proof entries: {} (CRI loads replacement AAX; no host overlay)", s_proof.size());
    if (s_debug)
        LOG_INFO("[MUSIC][dbg] status={}", s_statusJson.size() > 400 ? s_statusJson.substr(0, 400) + "…" : s_statusJson);
}

void MusicMod_Shutdown()
{
    s_active = false;
    s_patchedCpk.clear();
    s_proof.clear();
}

void MusicMod_Tick() {}
void MusicMod_OnStreamActivity(bool active, int voices, int streaming)
{
    if (!s_debug) return;
    static int last = -1;
    const int key = (active ? 1 : 0) | (voices << 1) | (streaming << 8);
    if (key == last) return;
    last = key;
    LOG_INFO("[MUSIC][dbg] AX stream activity active={} voices={} streaming={}", active, voices, streaming);
}
void MusicMod_OnAxVoiceStats(int total, int running, int streaming)
{
    if (!s_debug) return;
    static int lastT = -1, lastR = -1, lastS = -1;
    if (total == lastT && running == lastR && streaming == lastS) return;
    lastT = total; lastR = running; lastS = streaming;
    LOG_INFO("[MUSIC][dbg] AX voices total={} running={} streaming={}", total, running, streaming);
}
void MusicMod_OnStageName(const char* stage, const char* via)
{
    if (!s_debug && !s_active) return;
    if (stage && stage[0])
        LOG_INFO("[MUSIC] stage context '{}' via {}", stage, via ? via : "?");
}
void MusicMod_OnGuestString(const char* s)
{
    if (!s_debug || !s) return;
    if (std::strncmp(s, "bgm_", 4) == 0)
        LOG_INFO("[MUSIC][dbg] guest cue/string '{}'", s);
}

void MusicMod_OnDiscPath(const std::string& path)
{
    if (!s_active) return;
    std::string lower = path;
    for (char& c : lower) c = (char)tolower((unsigned char)c);
    if (lower.find("sonic2010_0.cpk") == std::string::npos) return;
    if (s_loggedCpkOpen) return;
    s_loggedCpkOpen = true;
    LOG_INFO("[MUSIC] Game opened CPK via disc path '{}' — host override {}",
             path, s_patchedCpk);
}

static std::string Sha256FileRange(const fs::path& path, uint64_t off, uint64_t size)
{
    // Minimal SHA-256 for proof logs (not perf-critical; called once per entry).
    // Prefer shelling out to sha256sum for portability in this Linux primary build.
    const fs::path tmp = fs::temp_directory_path() / "scr_aax_proof.bin";
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    in.seekg((std::streamoff)off);
    std::vector<char> buf((size_t)size);
    in.read(buf.data(), (std::streamsize)size);
    if ((uint64_t)in.gcount() != size) return {};
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(buf.data(), (std::streamsize)size);
    }
    std::string cmd = "sha256sum \"" + tmp.string() + "\" 2>/dev/null";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return {};
    char line[128] = {};
    if (!fgets(line, sizeof(line), p)) { pclose(p); return {}; }
    pclose(p);
    std::error_code ec;
    fs::remove(tmp, ec);
    std::string hex;
    for (int i = 0; i < 64 && line[i]; i++) hex.push_back(line[i]);
    return hex;
}

void MusicMod_OnCpkHostRead(const std::string& hostPath, uint64_t fileOffset, uint32_t length, const uint8_t* data)
{
    if (!s_active || s_proof.empty() || !data || length == 0) return;
    if (!s_patchedCpk.empty())
    {
        if (hostPath.find("sonic2010_0.cpk") == std::string::npos && hostPath != s_patchedCpk)
            return;
    }
    for (auto& e : s_proof)
    {
        if (e.matched) continue;
        const uint64_t end = e.offset + e.size;
        const uint64_t r0 = fileOffset;
        const uint64_t r1 = fileOffset + length;
        if (r1 <= e.offset || r0 >= end) continue;

        if (r0 <= e.offset && r1 > e.offset && e.haveFirst16)
        {
            const uint32_t at = (uint32_t)(e.offset - r0);
            if (at + 16 <= length)
            {
                const bool ok = std::memcmp(data + at, e.first16, 16) == 0;
                if (ok)
                {
                    e.matched = true;
                    std::string liveSha;
                    if (!s_patchedCpk.empty())
                        liveSha = Sha256FileRange(s_patchedCpk, e.offset, e.size);
                    const bool shaOk = !liveSha.empty() && liveSha == e.sha256;
                    LOG_INFO("[MUSIC] PROOF: CPK read supplies replacement AAX '{}' "
                             "@ cpk+{:x} size={} first16=match full_sha256={} (expected {})",
                             e.path, e.offset, (unsigned)e.size,
                             shaOk ? "MATCH" : (liveSha.empty() ? "unverified" : liveSha),
                             e.sha256);
                    if (s_debug && !shaOk && !liveSha.empty())
                        LOG_WARN("[MUSIC][dbg] payload SHA mismatch vs proof TSV");
                }
                else
                {
                    LOG_WARN("[MUSIC] PROOF FAIL: expected AAX '{}' first16 mismatch at cpk+{:x}",
                             e.path, e.offset);
                }
            }
        }
        else if (s_debug)
        {
            LOG_INFO("[MUSIC][dbg] CPK read overlaps '{}' host_off={:x}+{:x} entry={:x}+{:x}",
                     e.path, fileOffset, length, e.offset, e.size);
        }
    }
}

bool MusicMod_SilenceStreams() { return false; }
bool MusicMod_SilenceVoice(bool, bool, uint32_t, int16_t) { return false; }
void MusicMod_MixInto(int16_t*, uint32_t, uint32_t) {}

bool MusicMod_Active() { return s_active; }
bool MusicMod_UsingOriginalCpk() { return s_useOriginal; }
const char* MusicMod_CurrentCue() { return ""; }
const char* MusicMod_CurrentStage() { return ""; }
const char* MusicMod_PatchedCpk() { return s_patchedCpk.c_str(); }
