#include "music_deps.h"
#include "../log.h"
#include "../net/http.h"
#include "../platform/platform.h"

#include <nlohmann/json.hpp>

#include <archive.h>
#include <archive_entry.h>

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
    for (int i = 0; i < 8; i++)
        std::snprintf(out + i * 8, 9, "%08x", h[i]);
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

bool CopyFileOverwrite(const fs::path& from, const fs::path& to)
{
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

void EnableEmbedSite(const fs::path& pyDir)
{
    // python3xx._pth — enable site-packages for cricodecs wheel.
    std::error_code ec;
    for (auto& ent : fs::directory_iterator(pyDir, ec))
    {
        if (!ent.is_regular_file()) continue;
        const auto name = ent.path().filename().string();
        if (name.size() < 5 || name.substr(name.size() - 4) != "._pth") continue;
        std::ofstream out(ent.path(), std::ios::binary | std::ios::trunc);
        out << "python312.zip\n.\nLib\\\\site-packages\nimport site\n";
        break;
    }
    fs::create_directories(pyDir / "Lib" / "site-packages", ec);
}

bool PackageInstalled(const json& pkg, const fs::path& root)
{
    const std::string id = pkg.value("id", "");
    const std::string ver = pkg.value("version", "");
    const fs::path marker = root / id / ver / ".ready";
    std::error_code ec;
    return fs::is_regular_file(marker, ec);
}

bool ProbeExe(const fs::path& exe)
{
    std::error_code ec;
    return fs::is_regular_file(exe, ec) && fs::file_size(exe, ec) > 0;
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

void LinkBins(const fs::path& root)
{
    const fs::path bin = root / "current" / "bin";
    std::error_code ec;
    fs::create_directories(bin, ec);

    // vgmstream
    const fs::path vg = root / "vgmstream" / "r2168";
    if (fs::is_directory(vg, ec))
    {
        for (auto& ent : fs::directory_iterator(vg, ec))
        {
            if (!ent.is_regular_file()) continue;
            const auto n = ent.path().filename().string();
            if (MatchGlob(n, "vgmstream-cli.exe") || MatchGlob(n, "*.dll"))
                CopyFileOverwrite(ent.path(), bin / n);
        }
    }

    // ffmpeg bin subtree
    const fs::path ffRoot = root / "ffmpeg" / "n8.1.3-39-g660b5aa7db";
    fs::path ffBin;
    if (fs::is_directory(ffRoot, ec))
    {
        for (auto it = fs::recursive_directory_iterator(ffRoot, ec); it != fs::recursive_directory_iterator(); ++it)
        {
            if (!it->is_regular_file()) continue;
            if (IeEq(it->path().filename().string(), "ffmpeg.exe"))
            {
                ffBin = it->path().parent_path();
                break;
            }
        }
    }
    if (!ffBin.empty())
    {
        for (auto& ent : fs::directory_iterator(ffBin, ec))
        {
            if (!ent.is_regular_file()) continue;
            const auto n = ent.path().filename().string();
            if (MatchGlob(n, "ffmpeg.exe") || MatchGlob(n, "*.dll"))
                CopyFileOverwrite(ent.path(), bin / n);
        }
    }
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
    const std::string extractTo = pkg.value("extract_to", id + "/" + ver);
    if (id.empty() || url.empty() || wantSha.size() != 64)
    {
        err = "invalid package entry: " + id;
        return false;
    }

    const fs::path destDir = root / extractTo;
    const fs::path dlDir = root / ".download";
    std::error_code ec;
    fs::create_directories(dlDir, ec);
    const fs::path archivePath = dlDir / (id + "-" + ver + (archive == "wheel" ? ".whl" : ".zip"));

    g_progress.phase = 1;
    g_progress.bytesDone = 0;
    g_progress.bytesTotal = 0;
    HttpProgress httpProg;
    // Mirror cancel
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
        return false;
    }

    const std::string got = Sha256File(archivePath);
    if (!IeEq(got, wantSha))
    {
        err = id + ": SHA-256 mismatch (got " + got + ")";
        fs::remove(archivePath, ec);
        return false;
    }

    g_progress.phase = 3;
    // Fresh extract dir
    fs::remove_all(destDir, ec);
    fs::create_directories(destDir, ec);
    if (id == "cricodecs")
    {
        // Wheel → site-packages (destDir is already .../Lib/site-packages)
        if (!ExtractZip(archivePath, destDir, err, &g_progress.cancel)) return false;
    }
    else
    {
        if (!ExtractZip(archivePath, destDir, err, &g_progress.cancel)) return false;
    }

    g_progress.phase = 4;
    if (id == "python_embed") EnableEmbedSite(destDir);

    const fs::path markerDir = root / id / ver;
    fs::create_directories(markerDir, ec);
    {
        std::ofstream m(markerDir / ".ready");
        m << "ok\nsha256=" << wantSha << "\n";
    }
    fs::remove(archivePath, ec);
    return true;
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

    bool allOk = true;
    for (const auto& pkg : man["packages"])
    {
        MusicDepsPackageStatus st;
        st.id = pkg.value("id", "");
        st.version = pkg.value("version", "");
        st.purpose = pkg.value("purpose", "");
        const fs::path root = MusicDeps_Root();
        st.installed = PackageInstalled(pkg, root);
        if (st.id == "python_embed")
        {
            const fs::path py = root / "python" / "3.12.8" / "python.exe";
            st.path = py.string();
            st.usable = ProbeExe(py);
        }
        else if (st.id == "cricodecs")
        {
            const fs::path tip =
                root / "python" / "3.12.8" / "Lib" / "site-packages" / "cricodecs" / "__init__.py";
            st.path = tip.string();
            st.usable = ProbeExe(tip) || fs::is_regular_file(tip);
        }
        else if (st.id == "vgmstream")
        {
            const fs::path exe = MusicDeps_ManagedBin() / "vgmstream-cli.exe";
            st.path = exe.string();
            st.usable = ProbeExe(exe);
        }
        else if (st.id == "ffmpeg_lgpl")
        {
            const fs::path exe = MusicDeps_ManagedBin() / "ffmpeg.exe";
            st.path = exe.string();
            st.usable = ProbeExe(exe);
        }
        if (!st.usable) allOk = false;
        if (!st.usable && st.installed) st.error = "installed but not usable";
        if (!st.installed) st.error = "not installed";
        g_status.packages.push_back(std::move(st));
    }
    g_status.ready = allOk;
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
        g_progress.phase = 6;
        g_progress.running = false;
        g_threadRunning = false;
        RefreshStatusLocked();
        return;
    }

    const fs::path root = MusicDeps_Root();
    std::error_code ec;
    fs::create_directories(root, ec);

    // Install order: python → cricodecs → vgmstream → ffmpeg
    for (const auto& pkg : man["packages"])
    {
        if (g_progress.cancel.load())
        {
            err = "canceled";
            break;
        }
        const bool have = PackageInstalled(pkg, root);
        bool usable = false;
        const std::string id = pkg.value("id", "");
        if (id == "python_embed") usable = ProbeExe(root / "python" / "3.12.8" / "python.exe");
        else if (id == "cricodecs")
            usable = fs::is_regular_file(root / "python" / "3.12.8" / "Lib" / "site-packages" / "cricodecs" / "__init__.py",
                                         ec);
        else if (id == "vgmstream") usable = ProbeExe(MusicDeps_ManagedBin() / "vgmstream-cli.exe");
        else if (id == "ffmpeg_lgpl") usable = ProbeExe(MusicDeps_ManagedBin() / "ffmpeg.exe");

        if (!force && have && usable) continue;
        if (!InstallOne(pkg, root, err)) break;
        LinkBins(root);
    }

    LinkBins(root);
    MusicDeps_ApplyEnv();

    json state = {{"schema", 1}, {"ready", err.empty()}, {"error", err}};
    WriteState(root, state);

    std::lock_guard lk(g_mu);
    g_lastError = err;
    g_progress.phase = err.empty() ? 5 : 6;
    g_progress.running = false;
    g_threadRunning = false;
    RefreshStatusLocked();
    if (err.empty())
        LOG_INFO("[MUSIC] Managed dependencies ready under {}", root.string());
    else
        LOG_WARN("[MUSIC] Managed dependency setup failed: {}", err);
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
    if (!bin.empty()) SetEnvironmentVariableW(L"SCR_MUSIC_MANAGED_BIN", bin.wstring().c_str());
    if (!py.empty() && ProbeExe(py)) SetEnvironmentVariableW(L"SCR_MUSIC_PYTHON", py.wstring().c_str());
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
    // Soft signal for UI: managed tools not ready. Do not hard-block preview/prepare —
    // user drop-ins / PATH may already satisfy doctor.
    if (!kWinMusicDeps) return false;
    std::lock_guard lk(g_mu);
    RefreshStatusLocked();
    return !g_status.ready;
}
