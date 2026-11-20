#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

// Helpers shared by the updater and unit tests (no network / no process exit).

// PowerShell single-quoted string literal (`'` → `''`).
std::string UpdateApply_PsSingleQuote(const std::string& s);

// Body of apply_update.ps1 (UTF-8). Writer may prepend a UTF-8 BOM for Windows PowerShell 5.1.
std::string UpdateApply_WindowsPs1(const std::string& zipUtf8,
                                   const std::string& destUtf8,
                                   const std::string& exeName,
                                   uint32_t waitPid,
                                   const std::string& stageDirUtf8,
                                   const std::string& logPathUtf8);

bool UpdateApply_LooksLikeZip(const std::filesystem::path& path);
bool UpdateApply_LooksLikeAppImage(const std::filesystem::path& path);

// When expected==0, only checks the file exists and is non-empty.
bool UpdateApply_FileSizeOk(const std::filesystem::path& path, uint64_t expected, std::string& err);

// Lists the zip (no extract). Requires a basename match for exeName and rejects `..` entries.
bool UpdateApply_ZipContainsExe(const std::filesystem::path& zip, const std::string& exeName, std::string& err);
