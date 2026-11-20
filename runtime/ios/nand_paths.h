#pragma once
// Host NAND location + save backup wrappers (game / settings).
#include "ios.h"
#include "nand_backup.h"
#include "../platform/platform.h"

#include <filesystem>
#include <string>
#include <vector>

namespace nand_paths
{
namespace fs = std::filesystem;

inline fs::path DefaultRoot()
{
    fs::path root = Platform_ExeDir();
    fs::path ud = (root / ".." / "userdata").lexically_normal();
    fs::path legacy = root / "nand";
    std::error_code ec;
    if (fs::exists(ud, ec) || !fs::exists(legacy, ec)) return ud;
    return legacy;
}

inline fs::path ActiveRoot()
{
    const std::string& live = IOS_NandRoot();
    if (!live.empty()) return fs::path(live);
    return DefaultRoot();
}

inline bool BackupNow(fs::path& outDir, std::string& error)
{
    return nand_backup::BackupTree(ActiveRoot(), outDir, error);
}

inline bool LatestBackup(fs::path& out)
{
    return nand_backup::LatestBackupIn(ActiveRoot(), out);
}

inline std::vector<fs::path> ListBackups()
{
    return nand_backup::ListBackupsIn(ActiveRoot());
}

inline bool RestoreFrom(const fs::path& backup, std::string& error)
{
    return nand_backup::RestoreTree(ActiveRoot(), backup, error);
}
}   // namespace nand_paths
