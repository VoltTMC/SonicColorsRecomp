#pragma once
// Backend-independent GX draw batch. Shared Flush builds this; D3D11/Vulkan consume it.
// A4e: carries full VSKey/PSKey TEV IR + multi-texture binds (not Stage-B tex0-only).
#include "shadergen.h"
#include "vertex_loader.h"

#include <cstdint>

enum class GfxTopo : uint8_t
{
    TriList = 0,
    LineList = 1,
    PointList = 2,
};

enum class GfxCull : uint8_t
{
    None = 0,
    Front = 1,
    Back = 2,
};

// GX GEN_MODE cull (bits 14..15): 0=none, 1=cull back, 2=cull front, 3=all.
// Map to GfxCull for backends. D3D11 and Vulkan both treat GX fronts as clockwise
// in screen space (D3D FrontCounterClockwise=FALSE / VK_FRONT_FACE_CLOCKWISE).
// Do not invert cull or front-face to "compensate" for GLSL clip.y = -clip.y —
// that made cull-back discard intended fronts (A4j TR1 sky/background).
inline GfxCull GfxCullFromGx(uint32_t gxCull)
{
    if (gxCull == 1) return GfxCull::Back;
    if (gxCull == 2) return GfxCull::Front;
    return GfxCull::None;
}

// Vulkan / D3D cull bit to apply for a GfxCull value when front faces are CW.
// 0=none, 1=cull front faces, 2=cull back faces (matches D3D11_CULL_* numbering).
inline uint8_t GfxCullHwMode(GfxCull c)
{
    if (c == GfxCull::Back) return 2;  // cull back
    if (c == GfxCull::Front) return 1; // cull front
    return 0;
}

struct GfxViewport
{
    float x = 0, y = 0, w = 0, h = 0;
    float minDepth = 0, maxDepth = 1;
};

struct GfxScissor
{
    int32_t x = 0, y = 0, w = 0, h = 0;
};

// CPU-side texture bind for the Vulkan path (RGBA8, ownership stays with caller).
struct GfxTexBind
{
    const uint32_t* rgba = nullptr; // nullptr → unbound / white
    uint32_t w = 1, h = 1;
    uint64_t key = 0;               // cache key (addr|fmt|w|h|…)
    bool use = false;
    // GX TEX_MODE0 wrap/filter (A4e). wrap: 0=clamp, 1=repeat, 2=mirror, 3=repeat.
    uint8_t wrapU = 0, wrapV = 0;
    uint8_t magLinear = 0, minLinear = 0;
    uint32_t levels = 1;
    const uint32_t* mipRgba[16]{}; // level 0..levels-1 RGBA8
    uint32_t mipW[16]{}, mipH[16]{};
    uint32_t efbCopyAddr = 0; // non-zero → UploadTex uses VkGx EFB-copy cache (rgba optional)
    uint8_t mipFilter = 0;    // 0=nearest-mip, 1=linear-mip when levels>1
    float minLod = 0, maxLod = 0;
};

struct GfxDrawBatch
{
    GfxTopo topo = GfxTopo::TriList;
    GfxCull cull = GfxCull::None;
    GfxViewport viewport{};
    GfxScissor scissor{};

    const HostVertex* verts = nullptr;
    uint32_t vertCount = 0;
    const uint32_t* indices = nullptr;
    uint32_t indexCount = 0;

    const float* xfmem = nullptr; // g_gx.xfmem as float*; GPU path uses [0, xfFloatCount)
    uint32_t xfFloatCount = 0x680; // A4n: match D3D SSBO/SRV size (matrices/lights); xfregs are separate
    bool xfDirty = true;
    // Inclusive/exclusive float indices into xfmem for partial uploads (0/0 ⇒ full xfFloatCount).
    uint32_t xfDirtyBegin = 0;
    uint32_t xfDirtyEnd = 0;
    // A5n: monotonic XF mutation generation (bumped in MarkXfDirtyRange). Slot records the
    // generation it actually uploaded; equality alone is not used as a byte-equality proof
    // when dirty-range metadata is absent (0/0 ⇒ full hash/memcmp fallback).
    uint64_t xfGeneration = 0;

    VSConstants vs{};
    PSConstants ps{};
    VSKey vsKey{};
    PSKey psKey{};
    bool hasShaderKeys = false;

    // Up to 8 GX texture maps. Stages select maps via PSKey tevOrder.
    GfxTexBind tex[8]{};
    uint32_t usedMaps = 0; // bitmask of maps referenced by TEV / ind stages

    bool depthTest = true;
    bool depthWrite = true;
    uint8_t depthCompare = 3; // GX Z mode bits 1..3; 3 = <= (matches D3D11 funcs[])
    bool blendEnable = false;
    // A4j: full GX BP 0x41 blend (low 16) + color write mask. Vulkan previously forced
    // SRC_ALPHA/INV_SRC_ALPHA and always wrote RGBA, diverging from D3D11 GetBlendState.
    uint16_t blendMode = 0;   // BP(0x41) & 0xFFFF
    uint8_t colorWriteMask = 0xF; // RGBA bits; 0 = no color update
    uint8_t pixelFormat = 0;  // GX pixel format (1 = RGBA6 has dest alpha)
};
