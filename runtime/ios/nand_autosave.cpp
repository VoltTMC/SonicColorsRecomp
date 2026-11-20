#include "nand_autosave.h"
#include "nand_paths.h"
#include "../config.h"
#include "../log.h"
#include "../platform/platform.h"
#include "../ui/overlay.h"

#include <atomic>
#include <cstdint>

namespace
{
std::atomic<bool> s_pending{ false };
std::atomic<uint64_t> s_readyAtMs{ 0 };   // earliest time to run after a flush burst
std::atomic<uint64_t> s_lastBackupMs{ 0 };

constexpr uint64_t kDebounceMs = 2000;    // coalesce multi-file saves
constexpr uint64_t kCooldownMs = 60000;   // at most one write-triggered backup per minute
}   // namespace

void NandAutosave_OnFlush()
{
    if (!g_config.autoBackupSavesOnWrite) return;
    const uint64_t now = Platform_TickMs();
    s_readyAtMs.store(now + kDebounceMs, std::memory_order_relaxed);
    s_pending.store(true, std::memory_order_release);
}

void NandAutosave_Tick()
{
    if (!s_pending.load(std::memory_order_acquire)) return;
    if (!g_config.autoBackupSavesOnWrite)
    {
        s_pending.store(false, std::memory_order_relaxed);
        return;
    }
    const uint64_t now = Platform_TickMs();
    if (now < s_readyAtMs.load(std::memory_order_relaxed)) return;
    const uint64_t last = s_lastBackupMs.load(std::memory_order_relaxed);
    if (last && now - last < kCooldownMs) return;

    s_pending.store(false, std::memory_order_relaxed);
    std::filesystem::path dest;
    std::string err;
    if (!nand_paths::BackupNow(dest, err))
    {
        LOG_INFO("auto save backup (after write) skipped: {}", err);
        return;
    }
    s_lastBackupMs.store(now, std::memory_order_relaxed);
    LOG_INFO("auto save backup (after write): {}", dest.string());
    Overlay_Toast("Save backed up");
    if (g_config.saveBackupKeep > 0)
    {
        size_t n = nand_backup::PruneBackupsIn(nand_paths::ActiveRoot(), g_config.saveBackupKeep);
        if (n) LOG_INFO("pruned {} old save backup(s); keeping {}", n, g_config.saveBackupKeep);
    }
}
