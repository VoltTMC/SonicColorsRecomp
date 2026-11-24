#pragma once
// A4c: backend-independent GX draw batch. Shared Flush builds this; D3D11/Vulkan consume it.
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

// CPU-side texture bind for the Vulkan Stage-B path (RGBA8, ownership stays with caller).
struct GfxTexBind
{
    const uint32_t* rgba = nullptr; // nullptr → unbound / white
    uint32_t w = 1, h = 1;
    uint64_t key = 0;               // cache key (addr|fmt|w|h|…)
    bool use = false;
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
    // Stage B: single texture slot (map 0) + modulate with vertex colour.
    GfxTexBind tex0{};
    bool depthTest = true;
    bool depthWrite = true;
    bool blendEnable = false;
    // GX BP blend approx: src_alpha / inv_src_alpha when blendEnable.
};
