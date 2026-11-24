#pragma once
// A4e TEV: native Vulkan GX draw path (generated TEV shaders, multi-texture).
#include "../gfx_draw.h"

#include <cstdint>
#include <string>

struct VkGxInitOptions
{
    int efbScale = 1; // internal_scale
    bool enableValidation = false;
};

bool VkGx_Init(const VkGxInitOptions& opt, std::string& error);
void VkGx_Shutdown();
bool VkGx_IsActive();

// Record one GX Flush batch into the current EFB command buffer (no per-draw submit).
bool VkGx_Draw(const GfxDrawBatch& batch, std::string& error);

// Clear a rectangle of the EFB (native GX pixels × scale).
bool VkGx_ClearEfb(int x0, int y0, int x1, int y1, float r, float g, float b, float a, float depth,
                   bool clearColor, bool clearDepth, std::string& error);

// Copy EFB region to swapchain and present (XFB path). Coordinates in native GX pixels.
bool VkGx_PresentXfb(int sx, int sy, int sw, int sh, std::string& error);

// A4e: synchronous EFB readback to a 24-bit BMP (native GX coords × scale). For --shots / comparisons.
bool VkGx_SaveEfbBmp(int sx, int sy, int sw, int sh, const char* path, std::string& error);

struct VkGxStats
{
    uint64_t draws = 0;
    uint64_t vertices = 0;
    uint64_t indices = 0;
    uint64_t uploadBytes = 0;
    uint64_t bufferAllocs = 0;
    uint64_t pipelineHits = 0;
    uint64_t pipelineMisses = 0;
    uint64_t shaderModules = 0;
    uint64_t shaderCacheHits = 0;
    uint64_t shaderCacheMisses = 0;
    uint64_t spvCompileOk = 0;
    uint64_t spvCompileFail = 0;
    uint64_t texUploads = 0;
    uint64_t unsupportedOps = 0;
};
VkGxStats VkGx_Stats();
void VkGx_ResetStats();

// Rate-limited diagnostic for ops not yet on Vulkan.
void VkGx_NoteUnsupported(const char* op);
