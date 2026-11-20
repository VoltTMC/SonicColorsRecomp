#pragma once
#include <string>

// Non-installing GitHub Releases tag check (shared by Settings + Play tab).
// force=false: one automatic check per process. force=true: Settings button recheck.
void UpdateCheck_Start(bool force = false);
bool UpdateCheck_Busy();
bool UpdateCheck_HasResult();
bool UpdateCheck_NewerAvailable();
std::string UpdateCheck_Message();
std::string UpdateCheck_ReleasesUrl();
void UpdateCheck_OpenReleases();
