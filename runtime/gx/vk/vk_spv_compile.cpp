// A4e: runtime GLSL→SPIR-V via glslangValidator (existing toolchain). Separate from DXBC cache.
#include "vk_spv_compile.h"

#include "../../log.h"
#include "../../platform/platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>

#ifdef _WIN32
#define SCR_POPEN _popen
#define SCR_PCLOSE _pclose
#else
#define SCR_POPEN popen
#define SCR_PCLOSE pclose
#endif

namespace fs = std::filesystem;

namespace
{

std::mutex s_mu;
VkSpvCompileStats s_stats{};

static uint64_t Hash64(const void* data, size_t n, uint64_t seed = 0xcbf29ce484222325ull)
{
    const uint8_t* p = (const uint8_t*)data;
    uint64_t h = seed;
    for (size_t i = 0; i < n; i++)
    {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

static fs::path CacheDir()
{
    static fs::path dir;
    if (dir.empty())
    {
        dir = Platform_UserDir() / "shadercache_vk";
        std::error_code ec;
        fs::create_directories(dir, ec);
    }
    return dir;
}

static fs::path CachePath(const std::string& glsl, const char* stage)
{
    const uint64_t h = Hash64(glsl.data(), glsl.size(), 0xA4E50001ull ^ Hash64(stage, std::strlen(stage)));
    return CacheDir() / std::format("{}_{:016x}_{:x}.spv", stage, h, glsl.size());
}

static bool ReadSpvFile(const fs::path& path, std::vector<uint32_t>& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const auto n = (size_t)f.tellg();
    if (n < 20 || (n % 4) != 0) return false;
    f.seekg(0);
    out.resize(n / 4);
    if (!f.read(reinterpret_cast<char*>(out.data()), (std::streamsize)n)) return false;
    if (out[0] != 0x07230203u) return false; // SPIR-V magic
    return true;
}

static bool WriteSpvFile(const fs::path& path, const std::vector<uint32_t>& spv)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(spv.data()), (std::streamsize)(spv.size() * 4));
    return (bool)f;
}

static bool RunGlslang(const fs::path& srcPath, const fs::path& outPath, const char* stage, std::string& errOut)
{
    // Prefer PATH lookup; SCR_GLSLANG overrides.
    const char* bin = std::getenv("SCR_GLSLANG");
    std::string cmd;
    if (bin && bin[0])
        cmd = std::format("{} -V -S {} -o \"{}\" \"{}\" 2>&1", bin, stage, outPath.string(), srcPath.string());
    else
        cmd = std::format("glslangValidator -V -S {} -o \"{}\" \"{}\" 2>&1", stage, outPath.string(),
                          srcPath.string());

    FILE* pipe = SCR_POPEN(cmd.c_str(), "r");
    if (!pipe)
    {
        errOut = "failed to spawn glslangValidator (is it on PATH? set SCR_GLSLANG)";
        return false;
    }
    std::string output;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    const int rc = SCR_PCLOSE(pipe);
    if (rc != 0)
    {
        errOut = output.empty() ? std::format("glslangValidator exit {}", rc) : output;
        return false;
    }
    return fs::exists(outPath);
}

} // namespace

bool VkSpv_CompileGlsl(const std::string& glsl, const char* stage, std::vector<uint32_t>& outSpv,
                       std::string& error)
{
    std::lock_guard lk(s_mu);
    outSpv.clear();
    if (!stage || (std::strcmp(stage, "vert") != 0 && std::strcmp(stage, "frag") != 0))
    {
        error = "VkSpv_CompileGlsl: stage must be vert or frag";
        return false;
    }

    const fs::path cache = CachePath(glsl, stage);
    if (ReadSpvFile(cache, outSpv))
    {
        s_stats.cacheHits++;
        return true;
    }
    s_stats.cacheMisses++;

    const fs::path tmpDir = CacheDir() / "tmp";
    std::error_code ec;
    fs::create_directories(tmpDir, ec);
    const uint64_t tag = Hash64(glsl.data(), glsl.size());
    const fs::path srcPath = tmpDir / std::format("{}_{}.glsl", stage, tag);
    const fs::path outTmp = tmpDir / std::format("{}_{}.spv", stage, tag);

    {
        std::ofstream f(srcPath, std::ios::binary | std::ios::trunc);
        if (!f)
        {
            error = "cannot write temp GLSL for glslangValidator";
            s_stats.compileFail++;
            return false;
        }
        f.write(glsl.data(), (std::streamsize)glsl.size());
    }

    std::string glslErr;
    if (!RunGlslang(srcPath, outTmp, stage, glslErr))
    {
        s_stats.compileFail++;
        // Truncate huge dumps; keep enough for TEV diagnosis.
        if (glslErr.size() > 4000) glslErr.resize(4000);
        error = std::format("glslangValidator failed ({}):\n{}\n---- GLSL head ----\n{}", stage, glslErr,
                            glsl.substr(0, std::min<size_t>(glsl.size(), 1500)));
        std::error_code ec2;
        fs::remove(srcPath, ec2);
        fs::remove(outTmp, ec2);
        return false;
    }

    if (!ReadSpvFile(outTmp, outSpv))
    {
        s_stats.compileFail++;
        error = "glslangValidator produced unreadable SPIR-V";
        return false;
    }
    (void)WriteSpvFile(cache, outSpv);
    std::error_code ec3;
    fs::remove(srcPath, ec3);
    fs::remove(outTmp, ec3);
    s_stats.compileOk++;
    return true;
}

VkSpvCompileStats VkSpv_Stats()
{
    std::lock_guard lk(s_mu);
    return s_stats;
}

void VkSpv_ResetStats()
{
    std::lock_guard lk(s_mu);
    s_stats = {};
}
