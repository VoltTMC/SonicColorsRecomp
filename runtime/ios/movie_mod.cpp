#include "movie_mod.h"
#include "disc.h"
#include "../config.h"
#include "../log.h"
#include "../platform/platform.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <vector>

namespace fs = std::filesystem;

namespace
{

std::string s_dirOverride;
std::string s_dir;
bool s_active = false;
int s_overrideCount = 0;
std::set<std::string> s_loggedOpen;  // lower disc rel already logged

std::string Lower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string Utf8(const fs::path& p)
{
    auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

bool SafeFileName(const std::string& name)
{
    if (name.empty() || name.size() > 200) return false;
    if (name == "." || name == "..") return false;
    for (unsigned char c : name)
    {
        if (c < 32) return false;
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|')
            return false;
    }
    return true;
}

fs::path DefaultMovieDir()
{
    return Platform_UserDir() / "movie";
}

// Original SNCP8P loose movies under files/movie/ (not in sonic2010_0.cpk).
std::vector<std::string> DiscoverKnownMovies()
{
    std::vector<std::string> out;
    std::vector<fs::path> roots;
    if (!g_config.dataDir.empty())
        roots.push_back(fs::path(g_config.dataDir) / "files" / "movie");
    const fs::path user = Platform_UserDir();
    const fs::path exe = Platform_ExeDir();
    for (fs::path p : {
             user / "game" / "SNCP8P" / "files" / "movie",
             user / "game" / "extracted" / "DATA" / "files" / "movie",
             exe / "game" / "SNCP8P" / "files" / "movie",
             exe / "game" / "extracted" / "DATA" / "files" / "movie",
             fs::current_path() / "game" / "SNCP8P" / "files" / "movie",
             fs::current_path() / "game" / "extracted" / "DATA" / "files" / "movie",
         })
        roots.push_back(p);

    std::set<std::string> seen;
    std::error_code ec;
    for (auto& root : roots)
    {
        if (!fs::is_directory(root, ec)) continue;
        for (auto& e : fs::directory_iterator(root, ec))
        {
            if (!e.is_regular_file(ec)) continue;
            std::string name = Utf8(e.path().filename());
            if (Lower(name).size() < 5 || Lower(name).substr(Lower(name).size() - 4) != ".sfd")
                continue;
            if (!SafeFileName(name)) continue;
            std::string disc = std::string("movie/") + name;
            if (seen.insert(Lower(disc)).second) out.push_back(disc);
        }
        if (!out.empty()) break;
    }
    if (out.empty())
    {
        // Fallback catalog (Europe SNCP8P) so overrides still register before disc open.
        static const char* kNames[] = {
            "ev110.sfd",      "ev140.sfd",      "ev150.sfd",      "ev170.sfd",      "ev180.sfd",
            "ev210.sfd",      "ev215.sfd",      "ev220.sfd",      "ev225.sfd",      "ev230.sfd",
            "ev235.sfd",      "ev240.sfd",      "ev245.sfd",      "ev250.sfd",      "ev310.sfd",
            "ev320.sfd",      "ev330.sfd",      "ev340.sfd",      "ev350.sfd",      "ev360.sfd",
            "ev370.sfd",      "ev380.sfd",      "ev390.sfd",      "ev420.sfd",      "ev450.sfd",
            "ev460.sfd",      "ev470.sfd",      "ev480.sfd",      "ev490.sfd",      "ev510.sfd",
            "opening_eu.sfd", "opening_us.sfd", "segalogo_jp.sfd", "segalogo_us.sfd",
        };
        for (const char* n : kNames) out.push_back(std::string("movie/") + n);
    }
    std::sort(out.begin(), out.end());
    return out;
}

void WarnPluralMoviesFolders()
{
    std::error_code ec;
    const fs::path user = Platform_UserDir();
    for (fs::path p : { user / "movies", user / "Data" / "files" / "movies",
                        fs::current_path() / "Data" / "files" / "movies",
                        Platform_ExeDir() / "Data" / "files" / "movies" })
    {
        if (fs::is_directory(p, ec))
            LOG_WARN("[MOVIE] Found '{}' — the game uses singular 'movie', not 'movies'. "
                     "Place .sfd files in {} (or mods/<name>/files/movie/).",
                     Utf8(p), Utf8(DefaultMovieDir()));
    }
}

}  // namespace

bool MovieMod_LooksLikeSfd(const uint8_t* data, size_t n)
{
    if (!data || n < 4) return false;
    // MPEG program stream: pack (BA), system header (BB), padding (BE), or private (BF).
    for (size_t i = 0; i + 3 < n && i < 64; i++)
    {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01)
        {
            const uint8_t t = data[i + 3];
            if (t == 0xBA || t == 0xBB || t == 0xBE || t == 0xBF || t == 0xB9 || t == 0xE0 ||
                t == 0xC0)
                return true;
        }
    }
    return false;
}

std::vector<MovieModCandidate> MovieMod_ScanDir(const fs::path& root,
                                                const std::vector<std::string>& knownDiscRels)
{
    std::map<std::string, std::string> known;  // lower → canonical
    for (auto& r : knownDiscRels) known.emplace(Lower(r), r);

    std::vector<MovieModCandidate> out;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return out;

    // Collect candidate files from flat root, movie/, and files/movie/.
    std::vector<std::pair<std::string, fs::path>> files;  // basename → host
    auto consider = [&](const fs::path& dir) {
        if (!fs::is_directory(dir, ec)) return;
        for (auto& e : fs::directory_iterator(dir, ec))
        {
            if (!e.is_regular_file(ec)) continue;
            std::string name = Utf8(e.path().filename());
            std::string ln = Lower(name);
            if (ln.size() < 5 || ln.substr(ln.size() - 4) != ".sfd") continue;
            if (!SafeFileName(name)) continue;
            files.push_back({ name, e.path() });
        }
    };
    consider(root);
    consider(root / "movie");
    consider(root / "files" / "movie");

    std::set<std::string> usedHost;
    for (auto& [name, host] : files)
    {
        MovieModCandidate c;
        c.host = host;
        const std::string want = Lower(std::string("movie/") + name);
        auto it = known.find(want);
        if (it == known.end())
        {
            c.discRel = std::string("movie/") + name;
            c.valid = false;
            c.rejectReason = "no matching original movie";
            out.push_back(std::move(c));
            continue;
        }
        c.discRel = it->second;
        const auto sz = fs::file_size(host, ec);
        if (ec || sz == 0)
        {
            c.valid = false;
            c.rejectReason = "empty or unreadable";
            out.push_back(std::move(c));
            continue;
        }
        if (sz > 0xFFFFFFFFull)
        {
            c.valid = false;
            c.rejectReason = "file too large for FST";
            out.push_back(std::move(c));
            continue;
        }
        c.size = (uint64_t)sz;

        uint8_t hdr[64]{};
        {
            std::ifstream in(host, std::ios::binary);
            if (!in)
            {
                c.valid = false;
                c.rejectReason = "cannot open";
                out.push_back(std::move(c));
                continue;
            }
            in.read((char*)hdr, sizeof(hdr));
        }
        if (!MovieMod_LooksLikeSfd(hdr, sizeof(hdr)))
        {
            c.valid = false;
            c.rejectReason = "not an MPEG-PS/Sofdec SFD header";
            out.push_back(std::move(c));
            continue;
        }
        // Prefer first valid host for a given disc name.
        if (!usedHost.insert(Lower(c.discRel)).second)
        {
            c.valid = false;
            c.rejectReason = "duplicate name ignored";
            out.push_back(std::move(c));
            continue;
        }
        c.valid = true;
        out.push_back(std::move(c));
    }
    return out;
}

void MovieMod_SetDir(const std::string& dir) { s_dirOverride = dir; }

void MovieMod_Init()
{
    s_active = false;
    s_overrideCount = 0;
    s_loggedOpen.clear();
    s_dir.clear();

    if (!g_config.movieModsEnabled)
    {
        LOG_INFO("[MOVIE] Movie replacements disabled in settings — originals only");
        return;
    }

    if (!s_dirOverride.empty())
        s_dir = s_dirOverride;
    else if (!g_config.movieModsDir.empty())
        s_dir = g_config.movieModsDir;
    else
        s_dir = DefaultMovieDir().string();

    WarnPluralMoviesFolders();

    const fs::path root = fs::path(s_dir);
    std::error_code ec;
    if (!fs::exists(root, ec))
    {
        // Create empty drop folder so the path is discoverable.
        fs::create_directories(root, ec);
        LOG_INFO("[MOVIE] No replacements yet — drop .sfd files in {} (mirrored names: "
                 "ev110.sfd, opening_eu.sfd, …). Also supported: mods/<name>/files/movie/",
                 Utf8(root));
        return;
    }
    if (!fs::is_directory(root, ec))
    {
        LOG_WARN("[MOVIE] {} is not a directory — originals only", Utf8(root));
        return;
    }

    const auto known = DiscoverKnownMovies();
    auto cands = MovieMod_ScanDir(root, known);
    int unused = 0, rejected = 0;
    for (auto& c : cands)
    {
        if (!c.valid)
        {
            if (c.rejectReason == "no matching original movie")
            {
                unused++;
                LOG_INFO("[MOVIE] unused file (no original match): {} ({})", Utf8(c.host.filename()),
                         c.rejectReason);
            }
            else
            {
                rejected++;
                LOG_WARN("[MOVIE] rejected {}: {}", Utf8(c.host.filename()), c.rejectReason);
            }
            continue;
        }
        Disc_RegisterHostOverride(c.discRel, c.host);
        s_overrideCount++;
        LOG_INFO("[MOVIE] override {} → {} ({} bytes)", c.discRel, Utf8(c.host), c.size);
    }

    if (s_overrideCount > 0)
    {
        s_active = true;
        LOG_INFO("[MOVIE] {} replacement(s) from {} (unused={}, rejected={}); originals "
                 "unchanged on disc",
                 s_overrideCount, Utf8(root), unused, rejected);
    }
    else
        LOG_INFO("[MOVIE] No valid replacements in {} (unused={}, rejected={})", Utf8(root), unused,
                 rejected);
}

void MovieMod_Shutdown()
{
    s_active = false;
    s_overrideCount = 0;
    s_loggedOpen.clear();
}

bool MovieMod_Active() { return s_active; }
int MovieMod_OverrideCount() { return s_overrideCount; }
const char* MovieMod_Dir() { return s_dir.c_str(); }

void MovieMod_OnDiscPath(const std::string& path)
{
    if (!s_active) return;
    const std::string l = Lower(path);
    // Host path or disc-relative.
    if (l.find("movie/") == std::string::npos && l.find("movie\\") == std::string::npos)
    {
        if (l.size() < 5 || l.substr(l.size() - 4) != ".sfd") return;
    }
    if (!s_loggedOpen.insert(l).second) return;
    LOG_INFO("[MOVIE] disc read {}", path);
}
