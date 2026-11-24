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

    const float* xfmem = nullptr; // 0x1000 floats (g_gx.xfmem as float*)
    uint32_t xfFloatCount = 0x1000;
    bool xfDirty = true;

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
    bool blendEnable = false;
    // GX BP blend approx: src_alpha / inv_src_alpha when blendEnable.
};
