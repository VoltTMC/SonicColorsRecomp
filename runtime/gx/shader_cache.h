#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

// A3w — versioned shader-cache metadata + runtime/offline stats.
// Builds on the existing BlackKnight-derived disk cache:
//   shadercache/vs_keys.bin, ps_keys.bin, {vs,ps}_5_0_<hash>_<len>.cso
// Portable layer = HLSL→DXBC blobs (D3D11). Device-specific Vulkan pipelines
 // (DXVK) are NOT claimed as a shippable universal cache.

inline constexpr uint32_t kScrShaderCacheSchema = 1;
inline constexpr const char* kScrShaderGeneratorId = "scr-shadergen-1";
inline constexpr const char* kScrShaderGameId = "SNCP8P";
inline constexpr const char* kScrShaderGameRev = "00";

struct ShaderCacheStats
{
    uint64_t vsMemHit = 0, vsMemMiss = 0;
    uint64_t psMemHit = 0, psMemMiss = 0;
    uint64_t blobDiskHit = 0, blobDiskMiss = 0;
    uint64_t vsCompile = 0, psCompile = 0;
    uint64_t vsCreateOk = 0, psCreateOk = 0;
    uint64_t vsCreateFail = 0, psCreateFail = 0;
    uint64_t vsQueued = 0, psQueued = 0;
    uint64_t keysRememberedVs = 0, keysRememberedPs = 0;
    uint64_t blendCreate = 0, depthCreate = 0, rasterCreate = 0;
    uint64_t blendHit = 0, depthHit = 0, rasterHit = 0;
    uint32_t vsInMemory = 0, psInMemory = 0;
    uint32_t preloadVs = 0, preloadPs = 0;
    uint8_t manifestOk = 0;
    uint8_t manifestMismatch = 0;
};

struct ShaderCacheManifest
{
    uint32_t schemaVersion = kScrShaderCacheSchema;
    char gameId[16]{};
    char gameRev[8]{};
    char generatorId[32]{};
    char api[16]{};           // "d3d11"
    char vsTarget[16]{};      // "vs_5_0"
    char psTarget[16]{};      // "ps_5_0"
    uint32_t vsKeyBytes = 0;
    uint32_t psKeyBytes = 0;
    uint32_t vsKeyCount = 0;
    uint32_t psKeyCount = 0;
    uint32_t csoCount = 0;
    uint8_t portableDxbc = 1;
    uint8_t deviceSpecificPipelines = 0;  // always 0 for shipped data
};

void ShaderCache_GetStats(ShaderCacheStats& out);
void ShaderCache_ResetStats();
void ShaderCache_OverlayLine(char* buf, size_t bufSize);

// Opt-in collection: SCR_SHADER_COLLECT=1, optional SCR_SHADER_COLLECT_AREA=<tag>
bool ShaderCache_CollectEnabled();
const char* ShaderCache_CollectArea();  // "" if unset

// Write/validate manifest.json beside keys (precompile + startup).
bool ShaderCache_WriteManifest(const std::string& dir, const ShaderCacheManifest& m);
bool ShaderCache_ReadManifest(const std::string& dir, ShaderCacheManifest& out);
bool ShaderCache_ManifestCompatible(const ShaderCacheManifest& m, uint32_t vsKeyBytes,
                                    uint32_t psKeyBytes);

// CLI: --shader-cache-report <dir>
int ShaderCache_ReportDir(const std::string& dir);

// Called from renderer.cpp hot paths (keep cheap).
namespace scr_shader_cache_detail
{
void NoteVsMem(bool hit);
void NotePsMem(bool hit);
void NoteBlobDisk(bool hit);
void NoteCompile(bool vs, bool ok);
void NoteCreate(bool vs, bool ok);
void NoteQueued(bool vs);
void NoteKey(bool vs);
void NotePreload(uint32_t vs, uint32_t ps);
void NoteInMemory(uint32_t vs, uint32_t ps);
void NoteManifest(bool ok, bool mismatch);
void NoteBlend(bool hit);
void NoteDepth(bool hit);
void NoteRaster(bool hit);
}
