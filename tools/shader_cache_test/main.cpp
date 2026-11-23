// A3w unit tests: manifest R/W, compatibility, stats, corrupt rejection.
#include "../../runtime/gx/shader_cache.h"
#include "../../runtime/gx/shadergen.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static int g_fail = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            g_fail++;                                                                                                  \
        }                                                                                                              \
    } while (0)

// Stubs for shader_cache.cpp
void Log_Write(int, const std::string&) {}

static ShaderCacheManifest MakeGood()
{
    ShaderCacheManifest m{};
    m.schemaVersion = kScrShaderCacheSchema;
    std::snprintf(m.gameId, sizeof(m.gameId), "%s", kScrShaderGameId);
    std::snprintf(m.gameRev, sizeof(m.gameRev), "%s", kScrShaderGameRev);
    std::snprintf(m.generatorId, sizeof(m.generatorId), "%s", kScrShaderGeneratorId);
    std::snprintf(m.api, sizeof(m.api), "d3d11");
    std::snprintf(m.vsTarget, sizeof(m.vsTarget), "vs_5_0");
    std::snprintf(m.psTarget, sizeof(m.psTarget), "ps_5_0");
    m.vsKeyBytes = (uint32_t)sizeof(VSKey);
    m.psKeyBytes = (uint32_t)sizeof(PSKey);
    m.vsKeyCount = 3;
    m.psKeyCount = 5;
    m.csoCount = 8;
    m.portableDxbc = 1;
    m.deviceSpecificPipelines = 0;
    return m;
}

int main()
{
    const fs::path tmp = fs::temp_directory_path() / "scr_shader_cache_test";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);

    // --- deterministic key sizes / identity ---
    CHECK(sizeof(VSKey) > 0);
    CHECK(sizeof(PSKey) > 0);
    VSKey a{}, b{};
    CHECK(a == b);
    a.numTexGens = 2;
    CHECK(!(a == b));
    PSKey p0{}, p1{};
    p1.numTevStages = 4;
    CHECK(!(p0 == p1));

    // --- write / read roundtrip ---
    ShaderCacheManifest good = MakeGood();
    CHECK(ShaderCache_WriteManifest(tmp.string(), good));
    ShaderCacheManifest got{};
    CHECK(ShaderCache_ReadManifest(tmp.string(), got));
    CHECK(got.schemaVersion == kScrShaderCacheSchema);
    CHECK(std::strcmp(got.generatorId, kScrShaderGeneratorId) == 0);
    CHECK(std::strcmp(got.gameId, kScrShaderGameId) == 0);
    CHECK(got.vsKeyBytes == sizeof(VSKey));
    CHECK(got.psKeyBytes == sizeof(PSKey));
    CHECK(got.portableDxbc == 1);
    CHECK(got.deviceSpecificPipelines == 0);
    CHECK(ShaderCache_ManifestCompatible(got, (uint32_t)sizeof(VSKey), (uint32_t)sizeof(PSKey)));

    // --- version mismatch invalidation ---
    {
        ShaderCacheManifest bad = good;
        bad.schemaVersion = 999;
        CHECK(!ShaderCache_ManifestCompatible(bad, (uint32_t)sizeof(VSKey), (uint32_t)sizeof(PSKey)));
    }
    {
        ShaderCacheManifest bad = good;
        std::snprintf(bad.generatorId, sizeof(bad.generatorId), "other-gen");
        CHECK(!ShaderCache_ManifestCompatible(bad, (uint32_t)sizeof(VSKey), (uint32_t)sizeof(PSKey)));
    }
    {
        ShaderCacheManifest bad = good;
        bad.vsKeyBytes = 4;
        CHECK(!ShaderCache_ManifestCompatible(bad, (uint32_t)sizeof(VSKey), (uint32_t)sizeof(PSKey)));
    }
    {
        ShaderCacheManifest bad = good;
        std::snprintf(bad.api, sizeof(bad.api), "vulkan");
        CHECK(!ShaderCache_ManifestCompatible(bad, (uint32_t)sizeof(VSKey), (uint32_t)sizeof(PSKey)));
    }

    // --- corrupt / empty rejection ---
    {
        const fs::path corrupt = tmp / "corrupt";
        fs::create_directories(corrupt, ec);
        {
            std::ofstream f(corrupt / "manifest.json");
            f << "{ not json at all";
        }
        ShaderCacheManifest c{};
        // Read may parse nothing → schema 0 → false
        const bool ok = ShaderCache_ReadManifest(corrupt.string(), c);
        CHECK(!ok || c.schemaVersion == 0 ||
              !ShaderCache_ManifestCompatible(c, (uint32_t)sizeof(VSKey), (uint32_t)sizeof(PSKey)));
    }
    {
        const fs::path empty = tmp / "empty";
        fs::create_directories(empty, ec);
        ShaderCacheManifest c{};
        CHECK(!ShaderCache_ReadManifest(empty.string(), c));
    }

    // --- stats hit/miss bookkeeping ---
    ShaderCache_ResetStats();
    scr_shader_cache_detail::NoteVsMem(true);
    scr_shader_cache_detail::NoteVsMem(false);
    scr_shader_cache_detail::NotePsMem(true);
    scr_shader_cache_detail::NoteBlobDisk(true);
    scr_shader_cache_detail::NoteBlobDisk(false);
    scr_shader_cache_detail::NoteCompile(true, true);
    scr_shader_cache_detail::NoteCompile(false, false);  // ignored
    scr_shader_cache_detail::NoteCreate(true, true);
    scr_shader_cache_detail::NoteCreate(false, false);
    scr_shader_cache_detail::NoteBlend(true);
    scr_shader_cache_detail::NoteBlend(false);
    scr_shader_cache_detail::NoteManifest(true, false);
    ShaderCacheStats st{};
    ShaderCache_GetStats(st);
    CHECK(st.vsMemHit == 1 && st.vsMemMiss == 1);
    CHECK(st.psMemHit == 1);
    CHECK(st.blobDiskHit == 1 && st.blobDiskMiss == 1);
    CHECK(st.vsCompile == 1 && st.psCompile == 0);
    CHECK(st.vsCreateOk == 1 && st.psCreateFail == 1);
    CHECK(st.blendHit == 1 && st.blendCreate == 1);
    CHECK(st.manifestOk == 1);

    char line[192];
    ShaderCache_OverlayLine(line, sizeof(line));
    CHECK(std::strstr(line, "ShCache") != nullptr);

    // --- report on directory with keys ---
    {
        const fs::path rep = tmp / "report";
        fs::create_directories(rep, ec);
        CHECK(ShaderCache_WriteManifest(rep.string(), good));
        {
            std::ofstream vs(rep / "vs_keys.bin", std::ios::binary);
            VSKey k{};
            vs.write(reinterpret_cast<const char*>(&k), sizeof(k));
            vs.write(reinterpret_cast<const char*>(&k), sizeof(k));  // duplicate
            k.numTexGens = 1;
            vs.write(reinterpret_cast<const char*>(&k), sizeof(k));
        }
        CHECK(ShaderCache_ReportDir(rep.string()) == 0);
    }

    fs::remove_all(tmp, ec);
    if (g_fail)
    {
        fprintf(stderr, "shader_cache_test: %d failures\n", g_fail);
        return 1;
    }
    printf("shader_cache_test OK\n");
    return 0;
}
