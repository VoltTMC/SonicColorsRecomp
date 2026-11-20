#pragma once
#include <cstdint>
#include <string>

// Non-installing GitHub Releases tag check (shared by Settings + Play tab).
// force=false: one automatic check per process. force=true: Settings button recheck.
void UpdateCheck_Start(bool force = false);
bool UpdateCheck_Busy();
bool UpdateCheck_HasResult();
bool UpdateCheck_NewerAvailable();
std::string UpdateCheck_Message();
// Plain-text release notes excerpt when a newer tag exists (empty otherwise).
std::string UpdateCheck_Notes();
std::string UpdateCheck_ReleasesUrl();
void UpdateCheck_OpenReleases();

// Download the platform package into user_data/downloads/ (does not install/replace the running build).
bool UpdateCheck_CanDownload();
void UpdateCheck_StartDownload();
// Blocking: wait for any in-flight download to finish (CLI).
void UpdateCheck_WaitDownload();
bool UpdateCheck_DownloadBusy();
bool UpdateCheck_DownloadDone();
bool UpdateCheck_DownloadOk();
std::string UpdateCheck_DownloadMessage();
// 0..1 when total is known; -1 while unknown / idle.
float UpdateCheck_DownloadProgress();
std::string UpdateCheck_DownloadPath();
void UpdateCheck_OpenDownloadFolder();
// Opens the downloaded AppImage (Linux) or reveals the zip (Windows). No-op if none.
void UpdateCheck_OpenDownloadedPackage();
bool UpdateCheck_DownloadedPackageIsAppImage();
// True when a downloaded AppImage can replace this running AppImage ($APPIMAGE writable).
bool UpdateCheck_CanApplyAppImage();
// Copy downloaded AppImage over $APPIMAGE and relaunch (never returns on success).
bool UpdateCheck_ApplyAppImageAndRelaunch(std::string& err);
