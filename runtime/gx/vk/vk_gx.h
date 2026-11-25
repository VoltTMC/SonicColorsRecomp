#pragma once
// A4e TEV: native Vulkan GX draw path (generated TEV shaders, multi-texture).
#include "../gfx_draw.h"
#include "vk_device.h"

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

// A4f: EFB → guest-addressed texture (GX copy command, non-XFB).
struct GfxEfbCopyRequest
{
    int sx = 0, sy = 0, sw = 0, sh = 0; // native GX pixels
    uint32_t destAddr = 0;              // guest EA (EfbCopyKey in renderer)
    uint32_t fmt = 0;                   // GX copy texture format (ctl-derived)
    bool intensity = false;
    bool depthSrc = false;
    bool half = false; // 2×2 box filter / half resolution
    uint32_t gxFrame = ~0u; // renderer s_frame at copy time (RAM fingerprint freshness)
};
bool VkGx_CopyEfbToTexture(const GfxEfbCopyRequest& req, std::string& error);
// gxFrame: GX s_frame for same-frame freshness (D3D hashedFrame parity). When the guest
// overwrites the copy destination in RAM, Lookup returns false so the caller can decode
// the CPU texture (A4i — fixes stale EFB copies shadowing sky/background textures).
bool VkGx_LookupEfbCopy(uint32_t destAddr, VkImageView& outView, uint32_t& outW, uint32_t& outH,
                        uint32_t gxFrame = ~0u);
void VkGx_InvalidateEfbCopy(uint32_t destAddr);

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
    uint64_t efbCopies = 0;
    uint64_t efbCopyBytes = 0;
    uint64_t texMipUploads = 0;
    uint64_t unsupportedOps = 0;
    // A4h: XF SSBO ring diagnostics
    uint64_t xfUploads = 0;
    uint64_t xfRingWraps = 0;
    uint64_t xfRingHighWater = 0; // bytes
    uint64_t xfRingBytes = 0;     // capacity
    // A4k/A4l: inline = recorded into the live draw CB (GPU-ordered, no per-copy CPU wait).
    // sync = legacy path that DrainFrameSlot + submit + CPU WaitForFences per copy
    // (SCR_A4L_EFB_SYNC=1, or soft-flush budget drain counted separately below).
    uint64_t efbCopyInline = 0;
    uint64_t efbCopySynced = 0;
    // A4l profiling (always counted; detailed logs via SCR_A4L_EFB_PROF=1)
    uint64_t efbCopySoftFlushes = 0;     // budget-driven DrainFrameSlot during copy path
    uint64_t efbCopyFenceWaits = 0;      // CPU WaitForFences attributed to EFB-copy path
    uint64_t efbCopyFenceWaitNs = 0;     // total CPU ns blocked on those waits
    uint64_t efbCopyRecordNs = 0;        // CPU ns recording copy commands (excl. waits)
    uint64_t efbCopySubmitNs = 0;        // CPU ns in queue submit for sync copies
};
VkGxStats VkGx_Stats();
void VkGx_ResetStats();

// Rate-limited diagnostic for ops not yet on Vulkan.
void VkGx_NoteUnsupported(const char* op);
