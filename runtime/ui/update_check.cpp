#include "update_check.h"
#include "../net/http.h"
#include "../platform/platform.h"
#include "../platform/ui.h"
#include "../version.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <format>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace
{
std::mutex s_mu;
std::string s_msg;
std::string s_url = "https://github.com/VoltTMC/SonicColorsRecomp/releases";
std::string s_assetUrl;
std::string s_assetName;
uint64_t s_assetSize = 0;
std::atomic<bool> s_busy{ false };
std::atomic<bool> s_done{ false };
std::atomic<bool> s_newer{ false };
std::atomic<bool> s_started{ false };

std::mutex s_dlMu;
std::string s_dlMsg;
std::string s_dlPath;
HttpProgress s_dlProgress;
std::atomic<bool> s_dlBusy{ false };
std::atomic<bool> s_dlDone{ false };
std::atomic<bool> s_dlOk{ false };

std::string NormTag(std::string s)
{
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s.erase(0, 1);
    const auto dash = s.find('-');
    if (dash != std::string::npos) s.resize(dash);
    return s;
}

const char* WantedAssetNeedle()
{
#ifdef _WIN32
    return "windows-x86_64.zip";
#else
    return "linux-x86_64.AppImage";
#endif
}

void PickAsset(const nlohmann::json& j, std::string& url, std::string& name, uint64_t& size)
{
    url.clear();
    name.clear();
    size = 0;
    if (!j.contains("assets") || !j["assets"].is_array()) return;
    const char* needle = WantedAssetNeedle();
    for (const auto& a : j["assets"])
    {
        const std::string n = a.value("name", "");
        if (n.find(needle) == std::string::npos) continue;
        name = n;
        url = a.value("browser_download_url", "");
        size = a.value("size", (uint64_t)0);
        break;
    }
}

constexpr int kDownloadKeep = 3;

void PruneDownloads(const fs::path& dir)
{
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;
    struct Entry
    {
        fs::path path;
        fs::file_time_type mtime;
    };
    std::vector<Entry> files;
    for (const auto& e : fs::directory_iterator(dir, ec))
    {
        if (ec || !e.is_regular_file(ec)) continue;
        const auto ext = e.path().extension().string();
        if (ext != ".AppImage" && ext != ".zip" && ext != ".part") continue;
        files.push_back({ e.path(), e.last_write_time(ec) });
    }
    if ((int)files.size() <= kDownloadKeep) return;
    std::sort(files.begin(), files.end(), [](const Entry& a, const Entry& b) { return a.mtime > b.mtime; });
    for (size_t i = (size_t)kDownloadKeep; i < files.size(); i++)
    {
        fs::remove(files[i].path, ec);
        ec.clear();
    }
}

void MakeExecutableIfAppImage(const fs::path& path)
{
#ifndef _WIN32
    const auto name = path.filename().string();
    if (name.find(".AppImage") == std::string::npos) return;
    std::error_code ec;
    fs::permissions(path,
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add, ec);
#endif
}
}   // namespace

void UpdateCheck_Start(bool force)
{
    if (!force)
    {
        bool expected = false;
        if (!s_started.compare_exchange_strong(expected, true)) return;
    }
    else
        s_started.store(true, std::memory_order_relaxed);
    if (s_busy.exchange(true)) return;
    s_done.store(false, std::memory_order_relaxed);
    s_newer.store(false, std::memory_order_relaxed);
    {
        std::lock_guard lk(s_mu);
        s_msg = "Checking GitHub Releases…";
        s_assetUrl.clear();
        s_assetName.clear();
        s_assetSize = 0;
    }
    std::thread([] {
        std::string body, err, msg;
        std::string url = "https://github.com/VoltTMC/SonicColorsRecomp/releases";
        std::string assetUrl, assetName;
        uint64_t assetSize = 0;
        bool newer = false;
        const bool ok = Http_Get(
            "https://api.github.com/repos/VoltTMC/SonicColorsRecomp/releases/latest", body, err);
        if (!ok)
            msg = "Update check failed: " + (err.empty() ? "network error" : err);
        else
        {
            try
            {
                auto j = nlohmann::json::parse(body);
                const std::string tag = j.value("tag_name", "");
                url = j.value("html_url", url);
                PickAsset(j, assetUrl, assetName, assetSize);
                const std::string local(g_version);
                if (tag.empty())
                    msg = "No release tag in the API response.";
                else if (NormTag(tag) == NormTag(local))
                    msg = std::format("Up to date (latest {}).", tag);
                else
                {
                    newer = true;
                    msg = std::format("Latest release is {} (this build: {}).", tag, local);
                    if (!assetName.empty())
                        msg += std::format(" Package: {}.", assetName);
                }
            }
            catch (...)
            {
                msg = "Could not parse GitHub release info.";
            }
        }
        {
            std::lock_guard lk(s_mu);
            s_msg = std::move(msg);
            s_url = std::move(url);
            s_assetUrl = std::move(assetUrl);
            s_assetName = std::move(assetName);
            s_assetSize = assetSize;
        }
        s_newer.store(newer, std::memory_order_relaxed);
        s_done.store(true, std::memory_order_release);
        s_busy.store(false, std::memory_order_relaxed);
    }).detach();
}

bool UpdateCheck_Busy() { return s_busy.load(std::memory_order_relaxed); }
bool UpdateCheck_HasResult() { return s_done.load(std::memory_order_acquire); }
bool UpdateCheck_NewerAvailable() { return s_newer.load(std::memory_order_relaxed); }

std::string UpdateCheck_Message()
{
    std::lock_guard lk(s_mu);
    return s_msg;
}

std::string UpdateCheck_ReleasesUrl()
{
    std::lock_guard lk(s_mu);
    return s_url;
}

void UpdateCheck_OpenReleases()
{
    Shell_OpenUrl(UpdateCheck_ReleasesUrl());
}

bool UpdateCheck_CanDownload()
{
    std::lock_guard lk(s_mu);
    return s_newer.load(std::memory_order_relaxed) && !s_assetUrl.empty() && !s_assetName.empty();
}

void UpdateCheck_StartDownload()
{
    if (!UpdateCheck_CanDownload()) return;
    if (s_dlBusy.exchange(true)) return;
    s_dlDone.store(false, std::memory_order_relaxed);
    s_dlOk.store(false, std::memory_order_relaxed);
    s_dlProgress.done.store(0);
    s_dlProgress.total.store(0);
    s_dlProgress.cancel.store(false);
    std::string url, name;
    uint64_t size = 0;
    {
        std::lock_guard lk(s_mu);
        url = s_assetUrl;
        name = s_assetName;
        size = s_assetSize;
    }
    {
        std::lock_guard lk(s_dlMu);
        s_dlPath.clear();
        s_dlMsg = std::format("Downloading {}…", name);
    }
    std::thread([url, name, size] {
        std::error_code ec;
        fs::path dir = Platform_UserDir() / "downloads";
        fs::create_directories(dir, ec);
        std::string safe;
        for (char c : name)
            safe += std::isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-' ? c : '_';
        if (safe.empty()) safe = "update.bin";
        fs::path dest = dir / safe;
        std::string err;
        uint64_t maxBytes =
            size ? std::min<uint64_t>(kHttpMaxDownload, size * 2 + (uint64_t(1) << 20)) : kHttpMaxDownload;
        const bool ok = Http_Download(url, dest, err, &s_dlProgress, maxBytes);
        if (ok)
        {
            MakeExecutableIfAppImage(dest);
            PruneDownloads(dir);
        }
        {
            std::lock_guard lk(s_dlMu);
            if (ok)
            {
                s_dlPath = dest.string();
                s_dlMsg = std::format("Downloaded {} — replace your install manually, then relaunch.", safe);
            }
            else
            {
                s_dlPath.clear();
                s_dlMsg = "Download failed: " + (err.empty() ? "network error" : err);
            }
        }
        s_dlOk.store(ok, std::memory_order_relaxed);
        s_dlDone.store(true, std::memory_order_release);
        s_dlBusy.store(false, std::memory_order_relaxed);
    }).detach();
}

void UpdateCheck_WaitDownload()
{
    while (s_dlBusy.load(std::memory_order_relaxed))
        Platform_SleepMs(50);
}

bool UpdateCheck_DownloadBusy() { return s_dlBusy.load(std::memory_order_relaxed); }
bool UpdateCheck_DownloadDone() { return s_dlDone.load(std::memory_order_acquire); }
bool UpdateCheck_DownloadOk() { return s_dlOk.load(std::memory_order_relaxed); }

std::string UpdateCheck_DownloadMessage()
{
    std::lock_guard lk(s_dlMu);
    return s_dlMsg;
}

float UpdateCheck_DownloadProgress()
{
    if (!s_dlBusy.load(std::memory_order_relaxed) && !s_dlDone.load(std::memory_order_relaxed))
        return -1.f;
    const uint64_t total = s_dlProgress.total.load(std::memory_order_relaxed);
    if (!total) return -1.f;
    const uint64_t done = s_dlProgress.done.load(std::memory_order_relaxed);
    return (float)((double)done / (double)total);
}

std::string UpdateCheck_DownloadPath()
{
    std::lock_guard lk(s_dlMu);
    return s_dlPath;
}

void UpdateCheck_OpenDownloadFolder()
{
    fs::path dir = Platform_UserDir() / "downloads";
    std::error_code ec;
    fs::create_directories(dir, ec);
    const std::string path = UpdateCheck_DownloadPath();
    if (!path.empty())
    {
        fs::path p(path);
        Shell_OpenFolder(p.has_parent_path() ? p.parent_path() : dir);
    }
    else
        Shell_OpenFolder(dir);
}
