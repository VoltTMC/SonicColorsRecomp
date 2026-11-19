#pragma once
#include "settings_ui.h"

// Launcher / overlay "Music" tab: manage stage WAV replacements (manifest.toml).

void MusicUI_Draw(SettingsHost host, const char*& help);
void MusicUI_Shutdown();   // stop preview before the launcher exits
