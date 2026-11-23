#include "shader_cache.h"
#include "shadergen.h"
#include "../log.h"
#include "../platform/platform.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{

std::mutex s_mu;
ShaderCacheStats s_st{};
std::atomic<int> s_collect{ -1 };
std::string s_collectArea;

bool EnvOn(const char* name)
{
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

}  // namespace

void ShaderCache_GetStats(ShaderCacheStats& out)
{
    std::lock_guard lk(s_mu);
    out = s_st;
}

void ShaderCache_ResetStats()
{
    std::lock_guard lk(s_mu);
    s_st = {};
}

void ShaderCache_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    ShaderCacheStats s;
    ShaderCache_GetStats(s);
    std::snprintf(buf, bufSize,
                  "ShCache vs=%u/%llu+%llu ps=%u/%llu+%llu disk=%llu/%llu cmp=%llu/%llu "
                  "man=%u",
                  s.vsInMemory, (unsigned long long)s.vsMemHit, (unsigned long long)s.vsMemMiss,
                  s.psInMemory, (unsigned long long)s.psMemHit, (unsigned long long)s.psMemMiss,
                  (unsigned long long)s.blobDiskHit, (unsigned long long)s.blobDiskMiss,
                  (unsigned long long)s.vsCompile, (unsigned long long)s.psCompile,
                  s.manifestOk);
}

bool ShaderCache_CollectEnabled()
{
    int v = s_collect.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvOn("SCR_SHADER_COLLECT") ? 1 : 0;
        s_collect.store(v, std::memory_order_relaxed);
        if (v)
        {
            const char* a = std::getenv("SCR_SHADER_COLLECT_AREA");
            s_collectArea = (a && a[0]) ? a : "default";
            LOG_INFO("A3w shader collect on (area={})", s_collectArea);
        }
    }
    return v != 0;
}

const char* ShaderCache_CollectArea()
{
    (void)ShaderCache_CollectEnabled();
    return s_collectArea.c_str();
}

// Internal hooks used by renderer.cpp (same TU linkage via these helpers).
namespace scr_shader_cache_detail
{

void NoteVsMem(bool hit)
{
    std::lock_guard lk(s_mu);
    if (hit) s_st.vsMemHit++;
    else s_st.vsMemMiss++;
}
void NotePsMem(bool hit)
{
    std::lock_guard lk(s_mu);
    if (hit) s_st.psMemHit++;
    else s_st.psMemMiss++;
}
void NoteBlobDisk(bool hit)
{
    std::lock_guard lk(s_mu);
    if (hit) s_st.blobDiskHit++;
    else s_st.blobDiskMiss++;
}
void NoteCompile(bool vs, bool ok)
{
    std::lock_guard lk(s_mu);
    if (!ok) return;  // failures logged; do not inflate create-fail
    if (vs) s_st.vsCompile++;
    else s_st.psCompile++;
}
void NoteCreate(bool vs, bool ok)
{
    std::lock_guard lk(s_mu);
    if (vs)
    {
        if (ok) s_st.vsCreateOk++;
        else s_st.vsCreateFail++;
    }
    else
    {
        if (ok) s_st.psCreateOk++;
        else s_st.psCreateFail++;
    }
}
void NoteQueued(bool vs)
{
    std::lock_guard lk(s_mu);
    if (vs) s_st.vsQueued++;
    else s_st.psQueued++;
}
void NoteKey(bool vs)
{
    std::lock_guard lk(s_mu);
    if (vs) s_st.keysRememberedVs++;
    else s_st.keysRememberedPs++;
}
void NotePreload(uint32_t vs, uint32_t ps)
{
    std::lock_guard lk(s_mu);
    s_st.preloadVs = vs;
    s_st.preloadPs = ps;
}
void NoteInMemory(uint32_t vs, uint32_t ps)
{
    std::lock_guard lk(s_mu);
    s_st.vsInMemory = vs;
    s_st.psInMemory = ps;
}
void NoteManifest(bool ok, bool mismatch)
{
    std::lock_guard lk(s_mu);
    s_st.manifestOk = ok ? 1 : 0;
    s_st.manifestMismatch = mismatch ? 1 : 0;
}
void NoteBlend(bool hit)
{
    std::lock_guard lk(s_mu);
    if (hit) s_st.blendHit++;
    else s_st.blendCreate++;
}
void NoteDepth(bool hit)
{
    std::lock_guard lk(s_mu);
    if (hit) s_st.depthHit++;
    else s_st.depthCreate++;
}
void NoteRaster(bool hit)
{
    std::lock_guard lk(s_mu);
    if (hit) s_st.rasterHit++;
    else s_st.rasterCreate++;
}

}  // namespace scr_shader_cache_detail

bool ShaderCache_WriteManifest(const std::string& dir, const ShaderCacheManifest& m)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(dir, ec);
    const fs::path path = fs::path(dir) / "manifest.json";
    FILE* f = std::fopen(path.string().c_str(), "wb");
    if (!f) return false;
    std::fprintf(f,
                 "{\n"
                 "  \"schemaVersion\": %u,\n"
                 "  \"gameId\": \"%s\",\n"
                 "  \"gameRev\": \"%s\",\n"
                 "  \"generatorId\": \"%s\",\n"
                 "  \"api\": \"%s\",\n"
                 "  \"vsTarget\": \"%s\",\n"
                 "  \"psTarget\": \"%s\",\n"
                 "  \"vsKeyBytes\": %u,\n"
                 "  \"psKeyBytes\": %u,\n"
                 "  \"vsKeyCount\": %u,\n"
                 "  \"psKeyCount\": %u,\n"
                 "  \"csoCount\": %u,\n"
                 "  \"portableDxbc\": %u,\n"
                 "  \"deviceSpecificPipelines\": %u,\n"
                 "  \"note\": \"DXBC .cso blobs are portable across D3D11; DXVK/Vulkan PSO "
                 "caches are device/driver-specific and are not shipped as universal.\"\n"
                 "}\n",
                 m.schemaVersion, m.gameId, m.gameRev, m.generatorId, m.api, m.vsTarget,
                 m.psTarget, m.vsKeyBytes, m.psKeyBytes, m.vsKeyCount, m.psKeyCount, m.csoCount,
                 m.portableDxbc, m.deviceSpecificPipelines);
    std::fclose(f);
    return true;
}

bool ShaderCache_ReadManifest(const std::string& dir, ShaderCacheManifest& out)
{
    out = {};
    const std::filesystem::path path = std::filesystem::path(dir) / "manifest.json";
    FILE* f = std::fopen(path.string().c_str(), "rb");
    if (!f) return false;
    char buf[2048]{};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    if (n == 0) return false;
    auto grabU = [&](const char* key, uint32_t& dst) {
        const char* p = std::strstr(buf, key);
        if (!p) return;
        p = std::strchr(p, ':');
        if (p) dst = (uint32_t)std::strtoul(p + 1, nullptr, 10);
    };
    auto grabS = [&](const char* key, char* dst, size_t dstN) {
        // key includes quotes, e.g. "\"gameId\"". Value is the next "..." after ':'.
        const char* p = std::strstr(buf, key);
        if (!p) return;
        p += std::strlen(key);
        p = std::strchr(p, ':');
        if (!p) return;
        p = std::strchr(p, '"');
        if (!p) return;
        ++p;
        const char* e = std::strchr(p, '"');
        if (!e) return;
        const size_t len = (size_t)(e - p);
        if (len >= dstN) return;
        std::memcpy(dst, p, len);
        dst[len] = 0;
    };
    grabU("\"schemaVersion\"", out.schemaVersion);
    grabS("\"gameId\"", out.gameId, sizeof(out.gameId));
    grabS("\"gameRev\"", out.gameRev, sizeof(out.gameRev));
    grabS("\"generatorId\"", out.generatorId, sizeof(out.generatorId));
    grabS("\"api\"", out.api, sizeof(out.api));
    grabS("\"vsTarget\"", out.vsTarget, sizeof(out.vsTarget));
    grabS("\"psTarget\"", out.psTarget, sizeof(out.psTarget));
    grabU("\"vsKeyBytes\"", out.vsKeyBytes);
    grabU("\"psKeyBytes\"", out.psKeyBytes);
    grabU("\"vsKeyCount\"", out.vsKeyCount);
    grabU("\"psKeyCount\"", out.psKeyCount);
    grabU("\"csoCount\"", out.csoCount);
    {
        uint32_t portable = 0, devicePs = 0;
        grabU("\"portableDxbc\"", portable);
        grabU("\"deviceSpecificPipelines\"", devicePs);
        out.portableDxbc = (uint8_t)portable;
        out.deviceSpecificPipelines = (uint8_t)devicePs;
    }
    return out.schemaVersion != 0;
}

bool ShaderCache_ManifestCompatible(const ShaderCacheManifest& m, uint32_t vsKeyBytes,
                                    uint32_t psKeyBytes)
{
    if (m.schemaVersion != kScrShaderCacheSchema) return false;
    if (std::strcmp(m.generatorId, kScrShaderGeneratorId) != 0) return false;
    if (std::strcmp(m.gameId, kScrShaderGameId) != 0) return false;
    if (m.vsKeyBytes && m.vsKeyBytes != vsKeyBytes) return false;
    if (m.psKeyBytes && m.psKeyBytes != psKeyBytes) return false;
    if (m.api[0] && std::strcmp(m.api, "d3d11") != 0) return false;
    return true;
}

int ShaderCache_ReportDir(const std::string& dir)
{
    namespace fs = std::filesystem;
    const fs::path root(dir);
    if (!fs::is_directory(root))
    {
        std::fprintf(stderr, "shader-cache-report: not a directory: %s\n", dir.c_str());
        return 1;
    }

    ShaderCacheManifest man{};
    const bool haveMan = ShaderCache_ReadManifest(dir, man);
    const size_t vsKeySz = sizeof(VSKey);
    const size_t psKeySz = sizeof(PSKey);

    auto countKeys = [&](const char* name, size_t keyBytes, uint32_t& uniqueOut) -> uint32_t {
        uniqueOut = 0;
        FILE* f = std::fopen((root / name).string().c_str(), "rb");
        if (!f) return 0;
        std::vector<uint8_t> buf(keyBytes);
        uint32_t total = 0;
        std::unordered_set<std::string> uniq;
        while (std::fread(buf.data(), 1, keyBytes, f) == keyBytes)
        {
            total++;
            uniq.emplace(reinterpret_cast<const char*>(buf.data()), keyBytes);
        }
        std::fclose(f);
        uniqueOut = (uint32_t)uniq.size();
        return total;
    };

    uint32_t vsUniq = 0, psUniq = 0;
    const uint32_t vsTotal = countKeys("vs_keys.bin", vsKeySz, vsUniq);
    const uint32_t psTotal = countKeys("ps_keys.bin", psKeySz, psUniq);

    uint32_t cso = 0, csoBytes = 0;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(root, ec))
    {
        if (!e.is_regular_file()) continue;
        if (e.path().extension() == ".cso")
        {
            cso++;
            csoBytes += (uint32_t)e.file_size();
        }
    }

    std::printf("A3w shader cache report: %s\n", dir.c_str());
    std::printf("  manifest: %s\n", haveMan ? "present" : "missing");
    if (haveMan)
    {
        const bool ok = ShaderCache_ManifestCompatible(man, (uint32_t)vsKeySz, (uint32_t)psKeySz);
        std::printf("  schema=%u gen=%s game=%s/%s api=%s compatible=%s\n", man.schemaVersion,
                    man.generatorId, man.gameId, man.gameRev, man.api, ok ? "yes" : "NO");
        std::printf("  portableDxbc=%u deviceSpecificPipelines=%u\n", man.portableDxbc,
                    man.deviceSpecificPipelines);
    }
    std::printf("  VSKey %zu B: entries=%u unique=%u\n", vsKeySz, vsTotal, vsUniq);
    std::printf("  PSKey %zu B: entries=%u unique=%u\n", psKeySz, psTotal, psUniq);
    std::printf("  DXBC .cso: %u files (%.2f MiB)\n", cso, csoBytes / (1024.0 * 1024.0));
    std::printf("  levels: (1) GX key→HLSL generation  (2) HLSL→DXBC compile  "
                "(3) D3D11 Create*Shader  (4) DXVK/Vulkan PSO = NOT shipped\n");
    return 0;
}
