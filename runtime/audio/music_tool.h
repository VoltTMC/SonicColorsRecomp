#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Locate tools/music_asset(.exe|.cmd) next to the exe, cwd, or SCR_MUSIC_ASSET.
std::filesystem::path MusicTool_Find();

// Run the music_asset CLI with argv (excluding the tool path). Captures stdout/stderr
// to the given files when non-empty. Returns false if the process could not start.
bool MusicTool_Run(const std::vector<std::string>& args,
                   const std::filesystem::path& stdoutPath,
                   const std::filesystem::path& stderrPath,
                   int& exitCode);
