#include "music_deps.h"
#include "../log.h"
#include "../net/http.h"
#include "../platform/platform.h"

#include <nlohmann/json.hpp>

#include <archive.h>
#include <archive_entry.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cctype>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

MusicDepsProgress g_progress;
std::mutex g_mu;
MusicDepsStatus g_status;
std::string g_lastError;
bool g_threadRunning = false;

#ifdef _WIN32
constexpr bool kWinMusicDeps = true;
#else
constexpr bool kWinMusicDeps = false;
#endif

std::string Sha256File(const fs::path& path)
{
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    FILE* f =
#ifdef _WIN32
        _wfopen(path.wstring().c_str(), L"rb");
#else
        std::fopen(path.string().c_str(), "rb");
#endif
    if (!f) return {};
    uint8_t block[64];
    size_t used = 0;
    uint64_t total = 0;
    auto compress = [&](const uint8_t* b) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)b[4 * i] << 24) | ((uint32_t)b[4 * i + 1] << 16) | ((uint32_t)b[4 * i + 2] << 8) | b[4 * i + 3];
        for (int i = 16; i < 64; i++)
        {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b0 = h[1], c = h[2], d = h[3], e = h[4], f0 = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++)
        {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f0) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b0) ^ (a & c) ^ (b0 & c);
            uint32_t t2 = S0 + maj;
            hh = g;
            g = f0;
            f0 = e;
            e = d + t1;
            d = c;
            c = b0;
            b0 = a;
            a = t1 + t2;
        }
        h[0] += a;
        h[1] += b0;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f0;
        h[6] += g;
        h[7] += hh;
    };
    while (true)
    {
        size_t n = std::fread(block + used, 1, 64 - used, f);
        used += n;
        total += n;
        if (used < 64) break;
        compress(block);
        used = 0;
    }
    std::fclose(f);
    block[used++] = 0x80;
    if (used > 56)
    {
        while (used < 64) block[used++] = 0;
        compress(block);
        used = 0;
    }
    while (used < 56) block[used++] = 0;
    uint64_t bits = total * 8;
    for (int i = 0; i < 8; i++) block[63 - i] = (uint8_t)(bits >> (8 * i));
    compress(block);
    char out[65];
    for (int i = 0; i < 8; i++) std::snprintf(out + i * 8, 9, "%08x", h[i]);
    return out;
}

bool IeEq(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

bool MatchGlob(const std::string& name, const std::string& glob)
{
    if (glob == "*.dll")
    {
        if (name.size() < 4) return false;
        return IeEq(name.substr(name.size() - 4), ".dll");
    }
    return IeEq(name, glob);
}

bool FileOk(const fs::path& p)
{
    std::error_code ec;
    return fs::is_regular_file(p, ec) && fs::file_size(p, ec) > 0;
}

bool CopyFileOverwrite(const fs::path& from, const fs::path& to)
{
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    return !ec && FileOk(to);
}

fs::path LocalAppData()
{
#ifdef _WIN32
    if (const char* p = std::getenv("LOCALAPPDATA"))
        if (p[0]) return fs::path(p);
#endif
    return {};
}

fs::path ManifestPath()
{
    const fs::path cands[] = {
        Platform_ExeDir() / "data" / "music_deps_manifest.json",
        Platform_ExeDir() / "music_deps_manifest.json",
        Platform_ExeDir() / ".." / "data" / "music_deps_manifest.json",
    };
    std::error_code ec;
    for (const auto& c : cands)
        if (fs::is_regular_file(c, ec)) return fs::weakly_canonical(c, ec);
    return {};
}

json LoadManifest(std::string& err)
{
    const fs::path p = ManifestPath();
    if (p.empty())
    {
        err = "music_deps_manifest.json not found beside the application";
        return {};
    }
    std::ifstream in(p);
    if (!in)
    {
        err = "cannot read " + p.string();
        return {};
    }
    try
    {
        return json::parse(in);
    }
    catch (const std::exception& e)
    {
        err = e.what();
        return {};
    }
}

// Append a relative path that may use '/' or '\\' as component separators.
// Never append a string containing '/' as a single filename (Wine Win32 listing breaks).
fs::path AppendRel(fs::path base, const std::string& rel)
{
    std::string cur;
    auto flush = [&] {
        if (!cur.empty())
        {
            base /= cur;
            cur.clear();
        }
    };
    for (char c : rel)
    {
        if (c == '/' || c == '\\') flush();
        else cur.push_back(c);
    }
    flush();
    return base;
}

fs::path ExtractDir(const fs::path& root, const json& pkg)
{
    const std::string rel = pkg.value("extract_to", pkg.value("id", "") + "/" + pkg.value("version", ""));
    return AppendRel(root, rel);
}

// Primary on-disk artifact from the extracted package (authoritative for "installed").
fs::path PrimaryArtifact(const fs::path& root, const json& pkg)
{
    const std::string id = pkg.value("id", "");
    const fs::path dir = ExtractDir(root, pkg);
    if (id == "cricodecs")
    {
        // Windows wheel is a binary extension module: __init__.cp312-win_amd64.pyd
        // (no __init__.py). Also accept pure-Python / alternate layouts.
        const fs::path pkgDir = dir / "cricodecs";
        const fs::path py = pkgDir / "__init__.py";
        if (FileOk(py)) return py;
        std::error_code ec;
        if (fs::is_directory(pkgDir, ec))
        {
            for (auto& ent : fs::directory_iterator(pkgDir, ec))
            {
                if (!ent.is_regular_file()) continue;
                const auto n = ent.path().filename().string();
                if (n.rfind("__init__", 0) != 0) continue;
                if (n.size() >= 4 && IeEq(n.substr(n.size() - 4), ".pyd")) return ent.path();
                if (n.size() >= 3 && IeEq(n.substr(n.size() - 3), ".py")) return ent.path();
            }
        }
        // Preferred expected path for error messages when nothing is present yet.
        return pkgDir / "__init__.cp312-win_amd64.pyd";
    }
    const std::string rel = pkg.value("exe_relative", "");
    if (!rel.empty()) return AppendRel(dir, rel);
    return {};
}

// Published path for Music Library / doctor (managed bin for CLI tools).
fs::path LinkedArtifact(const fs::path& root, const json& pkg)
{
    const std::string id = pkg.value("id", "");
    if (id == "vgmstream") return root / "current" / "bin" / "vgmstream-cli.exe";
    if (id == "ffmpeg_lgpl") return root / "current" / "bin" / "ffmpeg.exe";
    if (id == "python_embed") return PrimaryArtifact(root, pkg);
    if (id == "cricodecs") return PrimaryArtifact(root, pkg);
    return {};
}

void WriteReadyMarker(const fs::path& extractDir, const json& pkg)
{
    std::error_code ec;
    fs::create_directories(extractDir, ec);
    std::ofstream m(extractDir / ".ready");
    m << "ok\nid=" << pkg.value("id", "") << "\nversion=" << pkg.value("version", "")
      << "\nsha256=" << pkg.value("sha256", "") << "\n";
}

bool ExtractZip(const fs::path& zip, const fs::path& dest, std::string& error, std::atomic<bool>* cancel)
{
    std::error_code ec;
    fs::create_directories(dest, ec);
    archive* a = archive_read_new();
    archive_read_support_format_zip(a);
#ifdef _WIN32
    if (archive_read_open_filename_w(a, zip.c_str(), 1 << 16) != ARCHIVE_OK)
#else
    if (archive_read_open_filename(a, zip.c_str(), 1 << 16) != ARCHIVE_OK)
#endif
    {
        error = archive_error_string(a) ? archive_error_string(a) : "cannot open zip";
        archive_read_free(a);
        return false;
    }
    archive_entry* e = nullptr;
    int r;
    std::vector<char> buf(1 << 16);
    uint64_t total = 0;
    const uint64_t kMax = 2ull << 30;
    while ((r = archive_read_next_header(a, &e)) == ARCHIVE_OK || r == ARCHIVE_WARN)
    {
        if (cancel && *cancel)
        {
            error = "canceled";
            archive_read_free(a);
            return false;
        }
        const char* pathname = archive_entry_pathname(e);
        if (!pathname) continue;
        std::string rel = pathname;
        for (char& c : rel)
            if (c == '\\') c = '/';
        if (rel.find("..") != std::string::npos || (rel.size() >= 2 && rel[1] == ':'))
        {
            error = "archive path traversal rejected";
            archive_read_free(a);
            return false;
        }
        fs::path out = dest / rel;
        if (archive_entry_filetype(e) == AE_IFDIR)
        {
            fs::create_directories(out, ec);
            continue;
        }
        if (archive_entry_filetype(e) != AE_IFREG) continue;
        fs::create_directories(out.parent_path(), ec);
#ifdef _WIN32
        FILE* f = _wfopen(out.wstring().c_str(), L"wb");
#else
        FILE* f = std::fopen(out.string().c_str(), "wb");
#endif
        if (!f)
        {
            error = "cannot write " + out.string();
            archive_read_free(a);
            return false;
        }
        la_ssize_t n;
        while ((n = archive_read_data(a, buf.data(), buf.size())) > 0)
        {
            total += (uint64_t)n;
            if (total > kMax || std::fwrite(buf.data(), 1, (size_t)n, f) != (size_t)n)
            {
                n = -1;
                break;
            }
        }
        std::fclose(f);
        if (n < 0)
        {
            error = total > kMax ? "archive too large" : "archive read failed";
            archive_read_free(a);
            return false;
        }
    }
    archive_read_free(a);
    return true;
}

bool PthEnablesSite(const fs::path& pth)
{
#ifdef _WIN32
    FILE* f = _wfopen(pth.wstring().c_str(), L"rb");
#else
    FILE* f = std::fopen(pth.string().c_str(), "rb");
#endif
    if (!f) return false;
    char buf[512];
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = 0;
    const std::string s(buf, n);
    return s.find("import site") != std::string::npos && s.find("Lib\\site-packages") != std::string::npos;
}

void EnableEmbedSite(const fs::path& pyDir)
{
    std::error_code ec;
    fs::create_directories(pyDir / "Lib" / "site-packages", ec);
    for (auto& ent : fs::directory_iterator(pyDir, ec))
    {
        if (!ent.is_regular_file()) continue;
        const auto name = ent.path().filename().string();
        if (name.size() < 5 || name.substr(name.size() - 4) != "._pth") continue;
        if (PthEnablesSite(ent.path()))
        {
            LOG_INFO("[Dependency] python_embed: site-packages already enabled in {}", name);
            return;
        }
        // Zip extract often leaves ._pth read-only; clear that before rewrite.
        fs::permissions(ent.path(),
                        fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read,
                        fs::perm_options::replace, ec);
#ifdef _WIN32
        SetFileAttributesW(ent.path().wstring().c_str(), FILE_ATTRIBUTE_NORMAL);
        FILE* f = _wfopen(ent.path().wstring().c_str(), L"wb");
#else
        FILE* f = std::fopen(ent.path().string().c_str(), "wb");
#endif
        if (!f)
        {
            LOG_WARN("[Dependency] python_embed: cannot rewrite {} (site-packages will be invisible)", name);
            return;
        }
        // One backslash: Lib\site-packages (required by Windows embed ._pth).
        static const char kPth[] = "python312.zip\n.\nLib\\site-packages\nimport site\n";
        const bool wrote = std::fwrite(kPth, 1, sizeof(kPth) - 1, f) == sizeof(kPth) - 1;
        std::fclose(f);
        if (!wrote || !PthEnablesSite(ent.path()))
        {
            LOG_WARN("[Dependency] python_embed: failed to enable site-packages in {}", name);
            return;
        }
        LOG_INFO("[Dependency] python_embed: enabled site-packages in {}", name);
        return;
    }
    LOG_WARN("[Dependency] python_embed: no python*._pth found under {}", pyDir.string());
}

fs::path FindNamedUnder(const fs::path& root, const std::string& fileName)
{
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return {};
    for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); ++it)
    {
        if (!it->is_regular_file()) continue;
        if (IeEq(it->path().filename().string(), fileName)) return it->path();
    }
    return {};
}

int CopyGlobFromDir(const fs::path& srcBin, const fs::path& destBin, const std::vector<std::string>& globs,
                    std::string& err)
{
    std::error_code ec;
    int copied = 0;
    for (auto& ent : fs::directory_iterator(srcBin, ec))
    {
        if (!ent.is_regular_file()) continue;
        const auto n = ent.path().filename().string();
        bool match = false;
        for (const auto& g : globs)
            if (MatchGlob(n, g))
            {
                match = true;
                break;
            }
        if (!match) continue;
        if (!CopyFileOverwrite(ent.path(), destBin / n))
        {
            err = "failed to copy " + n + " → " + (destBin / n).string();
            return -1;
        }
        ++copied;
    }
    return copied;
}

// Copy CLI tools + DLLs into current/bin from extract trees (manifest-driven).
bool LinkBins(const fs::path& root, const json& man, std::string& err)
{
    const fs::path bin = root / "current" / "bin";
    std::error_code ec;
    fs::create_directories(bin, ec);
    bool ok = true;

    for (const auto& pkg : man["packages"])
    {
        const std::string id = pkg.value("id", "");
        if (id != "vgmstream" && id != "ffmpeg_lgpl") continue;

        const fs::path extract = ExtractDir(root, pkg);
        if (!fs::is_directory(extract, ec))
        {
            LOG_WARN("[Dependency] {} LinkBins: extract dir missing {}", id, extract.string());
            continue;
        }

        const std::string primaryName = id == "ffmpeg_lgpl" ? "ffmpeg.exe" : "vgmstream-cli.exe";
        std::vector<std::string> globs;
        if (pkg.contains("bin_glob") && pkg["bin_glob"].is_array())
            for (auto& g : pkg["bin_glob"]) globs.push_back(g.get<std::string>());
        if (globs.empty())
        {
            globs.push_back(primaryName);
            globs.emplace_back("*.dll");
        }

        fs::path srcBin = extract;
        if (pkg.contains("bin_subdir"))
            srcBin = AppendRel(extract, pkg.value("bin_subdir", ""));

        auto tryCopy = [&](const fs::path& dir) -> int {
            if (!fs::is_directory(dir, ec)) return 0;
            std::string copyErr;
            const int n = CopyGlobFromDir(dir, bin, globs, copyErr);
            if (n < 0)
            {
                err = id + ": " + copyErr;
                return -1;
            }
            return n;
        };

        int copied = tryCopy(srcBin);
        if (copied == 0)
        {
            // Nested / unexpected archive layout: locate the primary exe and use its directory.
            const fs::path found = FindNamedUnder(extract, primaryName);
            if (!found.empty())
            {
                LOG_INFO("[Dependency] {} LinkBins: found {} under {}", id, primaryName, found.parent_path().string());
                copied = tryCopy(found.parent_path());
            }
        }
        if (copied < 0)
        {
            ok = false;
            continue;
        }

        LOG_INFO("[Dependency] {} LinkBins: copied {} file(s) → {}", id, copied, bin.string());
        if (copied == 0)
        {
            // Last resort: copy the primary artifact alone if FindNamedUnder found nothing usable.
            const fs::path primary = PrimaryArtifact(root, pkg);
            if (FileOk(primary) && CopyFileOverwrite(primary, bin / primaryName))
            {
                LOG_INFO("[Dependency] {} LinkBins: published primary only → {}", id, (bin / primaryName).string());
                // Sibling DLLs next to the primary.
                std::string copyErr;
                CopyGlobFromDir(primary.parent_path(), bin, {"*.dll"}, copyErr);
                copied = 1;
            }
            else
            {
                err = id + ": LinkBins found no matching files under " + extract.string();
                ok = false;
            }
        }
    }
    return ok;
}

struct Resolve
{
    bool filesPresent = false;
    bool linkedPresent = false;
    bool usable = false;
    fs::path primary;
    fs::path linked;
    fs::path installDir;
    std::string state;
    std::string detail;
};

Resolve ResolvePackage(const fs::path& root, const json& pkg)
{
    Resolve r;
    r.installDir = ExtractDir(root, pkg);
    r.primary = PrimaryArtifact(root, pkg);
    r.linked = LinkedArtifact(root, pkg);
    r.filesPresent = !r.primary.empty() && FileOk(r.primary);
    r.linkedPresent = !r.linked.empty() && FileOk(r.linked);
    // Tools are usable from extract dir OR managed bin; python/cricodecs need primary files.
    const std::string id = pkg.value("id", "");
    if (id == "vgmstream" || id == "ffmpeg_lgpl")
        r.usable = r.filesPresent || r.linkedPresent;
    else
        r.usable = r.filesPresent;

    if (r.usable && r.linkedPresent)
        r.state = "ready";
    else if (r.usable && r.filesPresent)
        r.state = "files_present"; // needs LinkBins for CLI tools
    else if (r.filesPresent)
        r.state = "repair";
    else
        r.state = "not_installed";

    r.detail = "primary=" + r.primary.string() + " linked=" + r.linked.string();
    return r;
}

bool InstallOne(const json& pkg, const fs::path& root, std::string& err)
{
    if (g_progress.cancel.load())
    {
        err = "canceled";
        return false;
    }
    const std::string id = pkg.value("id", "");
    const std::string ver = pkg.value("version", "");
    const std::string url = pkg.value("url", "");
    const std::string wantSha = pkg.value("sha256", "");
    const std::string archive = pkg.value("archive", "zip");
    if (id.empty() || url.empty() || wantSha.size() != 64)
    {
        err = "invalid package entry: " + id;
        return false;
    }

    const fs::path destDir = ExtractDir(root, pkg);
    const fs::path dlDir = root / ".download";
    std::error_code ec;
    fs::create_directories(dlDir, ec);
    const fs::path archivePath = dlDir / (id + "-" + ver + (archive == "wheel" ? ".whl" : ".zip"));

    LOG_INFO("[Dependency] {}", id);
    LOG_INFO("[Dependency] Download: starting {}", url);
    g_progress.phase = 1;
    g_progress.bytesDone = 0;
    g_progress.bytesTotal = 0;
    HttpProgress httpProg;
    std::thread cancelPump([&] {
        while (g_progress.running.load() && g_progress.phase.load() == 1)
        {
            if (g_progress.cancel.load()) httpProg.cancel = true;
            g_progress.bytesDone = httpProg.done.load();
            g_progress.bytesTotal = httpProg.total.load();
            Platform_SleepMs(50);
        }
    });

    std::string dlErr;
    const bool dlOk = Http_Download(url, archivePath, dlErr, &httpProg, 800ull << 20);
    g_progress.phase = 2;
    cancelPump.join();
    if (!dlOk)
    {
        err = id + ": download failed: " + dlErr;
        LOG_WARN("[Dependency] Download: failed — {}", dlErr);
        return false;
    }
    LOG_INFO("[Dependency] Download: passed");

    const std::string got = Sha256File(archivePath);
    if (!IeEq(got, wantSha))
    {
        err = id + ": SHA-256 mismatch (got " + got + ")";
        LOG_WARN("[Dependency] SHA-256: failed (got {})", got);
        fs::remove(archivePath, ec);
        return false;
    }
    LOG_INFO("[Dependency] SHA-256: passed");

    g_progress.phase = 3;
    // For cricodecs, only clear the package dir under site-packages, not the whole tree.
    if (id == "cricodecs")
    {
        fs::remove_all(destDir / "cricodecs", ec);
        fs::remove_all(destDir / "cricodecs-1.2.0.dist-info", ec);
        // Also remove any dist-info matching the wheel
        if (fs::is_directory(destDir, ec))
        {
            for (auto& ent : fs::directory_iterator(destDir, ec))
            {
                const auto n = ent.path().filename().string();
                if (n.find("cricodecs") == 0 && n.find("dist-info") != std::string::npos)
                    fs::remove_all(ent.path(), ec);
            }
        }
        fs::create_directories(destDir, ec);
    }
    else
    {
        fs::remove_all(destDir, ec);
        fs::create_directories(destDir, ec);
    }

    if (!ExtractZip(archivePath, destDir, err, &g_progress.cancel))
    {
        LOG_WARN("[Dependency] Extraction: failed — {}", err);
        return false;
    }
    LOG_INFO("[Dependency] Extraction: passed → {}", destDir.string());

    g_progress.phase = 4;
    if (id == "python_embed") EnableEmbedSite(destDir);

    const fs::path primary = PrimaryArtifact(root, pkg);
    if (!FileOk(primary))
    {
        err = id + ": expected executable/module missing after extract: " + primary.string();
        LOG_WARN("[Dependency] Expected artifact: {}", primary.string());
        LOG_WARN("[Dependency] Verification: failed — artifact missing after extraction");
        return false;
    }
    LOG_INFO("[Dependency] Expected artifact: {}", primary.string());
    LOG_INFO("[Dependency] Verification: passed");

    WriteReadyMarker(destDir, pkg);
    fs::remove(archivePath, ec);
    return true;
}

void WriteState(const fs::path& root, const json& state)
{
    std::error_code ec;
    fs::create_directories(root, ec);
    const fs::path tmp = root / "state.json.part";
    const fs::path out = root / "state.json";
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    f << state.dump(2) << "\n";
    f.close();
    fs::rename(tmp, out, ec);
    if (ec)
    {
        fs::remove(out, ec);
        fs::rename(tmp, out, ec);
    }
}

void RefreshStatusLocked()
{
    g_status = {};
    g_status.supported = kWinMusicDeps;
    g_status.root = MusicDeps_Root().string();
    g_status.managedBin = MusicDeps_ManagedBin().string();
    g_status.python = MusicDeps_Python().string();
    g_status.busy = g_threadRunning;
    g_status.lastError = g_lastError;

    if (!kWinMusicDeps)
    {
        g_status.ready = true;
        g_status.message = "Managed Music deps are Windows-only; use system tools / music_deps_setup.sh";
        return;
    }

    std::string err;
    json man = LoadManifest(err);
    if (man.is_null() || !man.contains("packages"))
    {
        g_status.message = err.empty() ? "manifest missing" : err;
        g_status.ready = false;
        return;
    }

    // One-shot LinkBins repair when extracts exist but current/bin is stale (no download).
    // Avoid running on every UI poll — only when something looks incomplete.
    {
        bool needLink = false;
        for (const auto& pkg : man["packages"])
        {
            const std::string id = pkg.value("id", "");
            if (id != "vgmstream" && id != "ffmpeg_lgpl") continue;
            const Resolve r = ResolvePackage(MusicDeps_Root(), pkg);
            if (r.filesPresent && !r.linkedPresent) needLink = true;
        }
        if (needLink && !g_threadRunning)
        {
            std::string linkErr;
            LinkBins(MusicDeps_Root(), man, linkErr);
            if (!linkErr.empty()) LOG_WARN("[Dependency] LinkBins repair: {}", linkErr);
        }
    }

    bool allOk = true;
    for (const auto& pkg : man["packages"])
    {
        const Resolve r = ResolvePackage(MusicDeps_Root(), pkg);
        MusicDepsPackageStatus st;
        st.id = pkg.value("id", "");
        st.version = pkg.value("version", "");
        st.purpose = pkg.value("purpose", "");
        st.installDir = r.installDir.string();
        st.installed = r.filesPresent;

        if ((st.id == "vgmstream" || st.id == "ffmpeg_lgpl") && r.linkedPresent)
        {
            st.path = r.linked.string();
            st.usable = true;
            st.state = "ready";
        }
        else if ((st.id == "vgmstream" || st.id == "ffmpeg_lgpl") && r.filesPresent)
        {
            st.path = r.primary.string();
            st.usable = false;
            st.state = "repair";
            st.error = "extracted but not published to managed bin";
        }
        else if (r.filesPresent && (st.id == "python_embed" || st.id == "cricodecs"))
        {
            st.path = r.primary.string();
            st.usable = true;
            st.state = "ready";
        }
        else
        {
            st.path = r.primary.string();
            st.usable = false;
            st.state = "not_installed";
            st.error = "primary artifact missing";
        }

        if (!st.usable) allOk = false;
        g_status.packages.push_back(std::move(st));
    }
    g_status.ready = allOk;
    g_status.python = MusicDeps_Python().string();
    g_status.message = allOk ? "Music host tools ready" : "Music host tools need install or repair";
}

void EnsureWorker(bool force)
{
    std::string err;
    json man = LoadManifest(err);
    if (man.is_null())
    {
        std::lock_guard lk(g_mu);
        g_lastError = err;
        g_progress.phase = 7;
        g_progress.running = false;
        g_threadRunning = false;
        RefreshStatusLocked();
        return;
    }

    const fs::path root = MusicDeps_Root();
    std::error_code ec;
    fs::create_directories(root, ec);

    // Always try to publish existing extracts into current/bin before downloading.
    {
        std::string linkErr;
        LinkBins(root, man, linkErr);
    }

    for (const auto& pkg : man["packages"])
    {
        if (g_progress.cancel.load())
        {
            err = "canceled";
            break;
        }
        const std::string id = pkg.value("id", "");
        Resolve r = ResolvePackage(root, pkg);
        LOG_INFO("[Dependency] {}", id);
        LOG_INFO("[Dependency] Existing installation: {}", r.filesPresent ? "found" : "missing");
        LOG_INFO("[Dependency] Resolved path: {}", r.primary.string());

        auto isPkgReady = [](const Resolve& rr, const std::string& pkgId) {
            if (pkgId == "python_embed" || pkgId == "cricodecs") return rr.filesPresent;
            return rr.linkedPresent;
        };

        if (r.filesPresent)
        {
            // Authoritative: on-disk primary exists. Repair LinkBins; never wipe a working
            // package (force only reinstalls packages that still fail readiness).
            WriteReadyMarker(r.installDir, pkg);
            if (id == "python_embed") EnableEmbedSite(r.installDir);
            std::string linkErr;
            LinkBins(root, man, linkErr);
            r = ResolvePackage(root, pkg);
            if (isPkgReady(r, id))
            {
                LOG_INFO("[Dependency] Version check: skipped redownload (files present)");
                LOG_INFO("[Dependency] Final status: READY");
                continue;
            }
            if (!force)
            {
                LOG_INFO("[Dependency] Version check: skipped redownload (files present)");
                if ((id == "vgmstream" || id == "ffmpeg_lgpl") && !r.linkedPresent)
                {
                    LOG_WARN("[Dependency] Repair: LinkBins did not publish {}", id);
                    if (!linkErr.empty()) LOG_WARN("[Dependency] Failure reason: {}", linkErr);
                    LOG_WARN("[Dependency] Final status: REPAIR_REQUIRED");
                }
                continue;
            }
            LOG_WARN("[Dependency] Force reinstall: files present but not ready — {}", r.detail);
        }

        if (!InstallOne(pkg, root, err)) break;
        std::string linkErr;
        LinkBins(root, man, linkErr);
        g_progress.phase = 5;
        r = ResolvePackage(root, pkg);
        const bool ok =
            (id == "python_embed" || id == "cricodecs") ? r.filesPresent : (r.linkedPresent || r.filesPresent);
        if (!ok)
        {
            err = id + ": post-install verification failed; expected " + r.primary.string();
            LOG_WARN("[Dependency] Verification: failed — {}", err);
            break;
        }
        if ((id == "vgmstream" || id == "ffmpeg_lgpl") && !r.linkedPresent)
            LOG_WARN("[Dependency] {} installed but managed bin link incomplete: {}", id, linkErr);
        LOG_INFO("[Dependency] Final status: READY");
    }

    {
        std::string linkErr;
        LinkBins(root, man, linkErr);
    }
    MusicDeps_ApplyEnv();

    // Final readiness: python/cricodecs need extract files; CLI tools need managed bin publish.
    bool ready = true;
    json pkgs = json::array();
    for (const auto& pkg : man["packages"])
    {
        Resolve r = ResolvePackage(root, pkg);
        const std::string id = pkg.value("id", "");
        const bool ok =
            (id == "python_embed" || id == "cricodecs") ? r.filesPresent : r.linkedPresent;
        if (!ok)
        {
            ready = false;
            if (err.empty())
            {
                if (r.filesPresent && !r.linkedPresent)
                    err = id + ": installed under " + r.installDir.string() +
                          " but not published to " + (root / "current" / "bin").string();
                else
                    err = id + ": missing " + r.primary.string();
            }
        }
        pkgs.push_back({{"id", id},
                        {"version", pkg.value("version", "")},
                        {"usable", ok},
                        {"path", r.linkedPresent ? r.linked.string() : r.primary.string()}});
    }
    if (ready) err.clear();

    json state = {{"schema", 2}, {"ready", ready}, {"error", err}, {"packages", pkgs}};
    WriteState(root, state);

    std::lock_guard lk(g_mu);
    g_lastError = err;
    g_progress.phase = ready ? 6 : 7;
    g_progress.running = false;
    g_threadRunning = false;
    RefreshStatusLocked();
    if (ready)
        LOG_INFO("[MUSIC] Managed dependencies ready under {}", root.string());
    else
        LOG_WARN("[MUSIC] Managed dependency setup incomplete: {}", err);
}

} // namespace

fs::path MusicDeps_Root()
{
    if (!kWinMusicDeps) return {};
    const fs::path la = LocalAppData();
    if (la.empty()) return {};
    return la / "SonicColorsRecomp" / "dependencies";
}

fs::path MusicDeps_ManagedBin()
{
    const fs::path r = MusicDeps_Root();
    return r.empty() ? fs::path{} : r / "current" / "bin";
}

fs::path MusicDeps_Python()
{
    const fs::path r = MusicDeps_Root();
    return r.empty() ? fs::path{} : r / "python" / "3.12.8" / "python.exe";
}

void MusicDeps_ApplyEnv()
{
#ifdef _WIN32
    const fs::path bin = MusicDeps_ManagedBin();
    const fs::path py = MusicDeps_Python();
    if (!py.empty() && FileOk(py))
    {
        // Repair embed ._pth so Lib\site-packages (cricodecs) is importable.
        EnableEmbedSite(py.parent_path());
        SetEnvironmentVariableW(L"SCR_MUSIC_PYTHON", py.wstring().c_str());
        _putenv_s("SCR_MUSIC_PYTHON", py.string().c_str());
    }
    if (!bin.empty())
    {
        SetEnvironmentVariableW(L"SCR_MUSIC_MANAGED_BIN", bin.wstring().c_str());
        _putenv_s("SCR_MUSIC_MANAGED_BIN", bin.string().c_str());
    }
#endif
}

MusicDepsStatus MusicDeps_Status()
{
    std::lock_guard lk(g_mu);
    RefreshStatusLocked();
    return g_status;
}

MusicDepsProgress& MusicDeps_Progress() { return g_progress; }

bool MusicDeps_EnsureAsync(bool force)
{
    if (!kWinMusicDeps) return false;
    std::lock_guard lk(g_mu);
    if (g_threadRunning) return true;
    if (!force)
    {
        RefreshStatusLocked();
        if (g_status.ready) return false;
        // After a finished attempt, do not auto-restart when files are already on disk
        // (avoids download/repair loops). User can force via "Install Music tools".
        if (g_progress.phase.load() == 7 && !g_status.packages.empty())
        {
            bool allInstalled = true;
            for (const auto& p : g_status.packages)
                if (!p.installed) allInstalled = false;
            if (allInstalled) return false;
        }
    }
    g_threadRunning = true;
    g_progress.running = true;
    g_progress.cancel = false;
    g_progress.phase = 1;
    g_progress.bytesDone = 0;
    g_progress.bytesTotal = 0;
    g_lastError.clear();
    std::thread([force] { EnsureWorker(force); }).detach();
    return true;
}

void MusicDeps_Cancel() { g_progress.cancel = true; }

bool MusicDeps_BlocksHostOps()
{
    if (!kWinMusicDeps) return false;
    std::lock_guard lk(g_mu);
    RefreshStatusLocked();
    return !g_status.ready;
}
