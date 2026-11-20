#include "update_check.h"
#include "../net/http.h"
#include "../platform/platform.h"
#include "../platform/ui.h"
#include "../version.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <format>
#include <mutex>
#include <string>
#include <thread>

namespace
{
std::mutex s_mu;
std::string s_msg;
std::string s_url = "https://github.com/VoltTMC/SonicColorsRecomp/releases";
std::atomic<bool> s_busy{ false };
std::atomic<bool> s_done{ false };
std::atomic<bool> s_newer{ false };
std::atomic<bool> s_started{ false };

std::string NormTag(std::string s)
{
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s.erase(0, 1);
    const auto dash = s.find('-');
    if (dash != std::string::npos) s.resize(dash);
    return s;
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
    }
    std::thread([] {
        std::string body, err, msg;
        std::string url = "https://github.com/VoltTMC/SonicColorsRecomp/releases";
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
                const std::string local(g_version);
                if (tag.empty())
                    msg = "No release tag in the API response.";
                else if (NormTag(tag) == NormTag(local))
                    msg = std::format("Up to date (latest {}).", tag);
                else
                {
                    newer = true;
                    msg = std::format("Latest release is {} (this build: {}).", tag, local);
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
