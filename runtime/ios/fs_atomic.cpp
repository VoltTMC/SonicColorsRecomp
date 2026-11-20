#include "fs_atomic.h"
#include "../log.h"

#include <fstream>

bool Fs_WriteFileAtomic(const std::filesystem::path& dest, const void* data, size_t size)
{
    namespace fs = std::filesystem;
    fs::path tmp = dest;
    tmp += ".tmp";
    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f)
        {
            LOG_WARN("FS: failed to open temp {}", tmp.string());
            fs::remove(tmp, ec);
            return false;
        }
        if (size)
            f.write(reinterpret_cast<const char*>(data), (std::streamsize)size);
        if (!f)
        {
            LOG_WARN("FS: failed to write {}", tmp.string());
            fs::remove(tmp, ec);
            return false;
        }
    }
    fs::rename(tmp, dest, ec);
    if (ec)
    {
        LOG_WARN("FS: failed to replace {}: {}", dest.string(), ec.message());
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}
