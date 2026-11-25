// A4e TEV: GX Flush → Vulkan DrawIndexed (generated TEV shaders, 8× texture).
#include "vk_gx.h"
#include "vk_device.h"
#include "vk_spv_embed.h"
#include "vk_spv_compile.h"

#include "../../log.h"
#include "../../kernel/memory.h"
#include "../shadergen_glsl.h"
#include "../vertex_loader.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

constexpr int kEfbW = 640;
constexpr int kEfbH = 528;
constexpr int kFrames = 3;
constexpr VkDeviceSize kArenaBytes = 16u * 1024u * 1024u;

#define L(name) PFN_##name pfn_##name = nullptr
L(vkCreateBuffer); L(vkDestroyBuffer); L(vkGetBufferMemoryRequirements);
L(vkAllocateMemory); L(vkFreeMemory); L(vkBindBufferMemory); L(vkMapMemory); L(vkUnmapMemory);
L(vkCreateImage); L(vkDestroyImage); L(vkGetImageMemoryRequirements); L(vkBindImageMemory);
L(vkCreateImageView); L(vkDestroyImageView);
L(vkCreateSampler); L(vkDestroySampler);
L(vkCreateShaderModule); L(vkDestroyShaderModule);
L(vkCreateDescriptorSetLayout); L(vkDestroyDescriptorSetLayout);
L(vkCreatePipelineLayout); L(vkDestroyPipelineLayout);
L(vkCreateGraphicsPipelines); L(vkDestroyPipeline);
L(vkCreateRenderPass); L(vkDestroyRenderPass);
L(vkCreateFramebuffer); L(vkDestroyFramebuffer);
L(vkCreateDescriptorPool); L(vkDestroyDescriptorPool);
L(vkAllocateDescriptorSets); L(vkUpdateDescriptorSets);
L(vkCreateCommandPool); L(vkDestroyCommandPool);
L(vkAllocateCommandBuffers); L(vkBeginCommandBuffer); L(vkEndCommandBuffer); L(vkResetCommandBuffer);
L(vkCmdBeginRenderPass); L(vkCmdEndRenderPass);
L(vkCmdBindPipeline); L(vkCmdBindVertexBuffers); L(vkCmdBindIndexBuffer);
L(vkCmdBindDescriptorSets); L(vkCmdSetViewport); L(vkCmdSetScissor);
L(vkCmdDrawIndexed); L(vkCmdClearAttachments);
L(vkCmdPipelineBarrier); L(vkCmdCopyBuffer); L(vkCmdCopyBufferToImage); L(vkCmdCopyImageToBuffer);
L(vkCmdCopyImage); L(vkCmdBlitImage); L(vkCmdDraw); L(vkCmdPushConstants);
L(vkCreateFence); L(vkDestroyFence); L(vkWaitForFences); L(vkResetFences);
L(vkResetDescriptorPool);
L(vkGetPhysicalDeviceMemoryProperties);
#undef L

template <typename T>
static T LoadDev(const char* n)
{
    return reinterpret_cast<T>(Vk_GetDeviceProc(n));
}
template <typename T>
static T LoadInst(const char* n)
{
    return reinterpret_cast<T>(Vk_GetInstanceProc(n));
}

static bool LoadFns()
{
#define LD(name) pfn_##name = LoadDev<PFN_##name>(#name); if (!pfn_##name) { LOG_ERROR("vulkan gx: missing {}", #name); return false; }
    LD(vkCreateBuffer); LD(vkDestroyBuffer); LD(vkGetBufferMemoryRequirements);
    LD(vkAllocateMemory); LD(vkFreeMemory); LD(vkBindBufferMemory); LD(vkMapMemory); LD(vkUnmapMemory);
    LD(vkCreateImage); LD(vkDestroyImage); LD(vkGetImageMemoryRequirements); LD(vkBindImageMemory);
    LD(vkCreateImageView); LD(vkDestroyImageView);
    LD(vkCreateSampler); LD(vkDestroySampler);
    LD(vkCreateShaderModule); LD(vkDestroyShaderModule);
    LD(vkCreateDescriptorSetLayout); LD(vkDestroyDescriptorSetLayout);
    LD(vkCreatePipelineLayout); LD(vkDestroyPipelineLayout);
    LD(vkCreateGraphicsPipelines); LD(vkDestroyPipeline);
    LD(vkCreateRenderPass); LD(vkDestroyRenderPass);
    LD(vkCreateFramebuffer); LD(vkDestroyFramebuffer);
    LD(vkCreateDescriptorPool); LD(vkDestroyDescriptorPool);
    LD(vkAllocateDescriptorSets); LD(vkUpdateDescriptorSets);
    LD(vkCreateCommandPool); LD(vkDestroyCommandPool);
    LD(vkAllocateCommandBuffers); LD(vkBeginCommandBuffer); LD(vkEndCommandBuffer); LD(vkResetCommandBuffer);
    LD(vkCmdBeginRenderPass); LD(vkCmdEndRenderPass);
    LD(vkCmdBindPipeline); LD(vkCmdBindVertexBuffers); LD(vkCmdBindIndexBuffer);
    LD(vkCmdBindDescriptorSets); LD(vkCmdSetViewport); LD(vkCmdSetScissor);
    LD(vkCmdDrawIndexed); LD(vkCmdClearAttachments);
    LD(vkCmdPipelineBarrier); LD(vkCmdCopyBuffer); LD(vkCmdCopyBufferToImage); LD(vkCmdCopyImageToBuffer);
    LD(vkCmdCopyImage); LD(vkCmdBlitImage); LD(vkCmdDraw); LD(vkCmdPushConstants);
    LD(vkCreateFence); LD(vkDestroyFence); LD(vkWaitForFences); LD(vkResetFences);
    LD(vkResetDescriptorPool);
#undef LD
    pfn_vkGetPhysicalDeviceMemoryProperties =
        LoadInst<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties");
    if (!pfn_vkGetPhysicalDeviceMemoryProperties)
    {
        LOG_ERROR("vulkan gx: missing vkGetPhysicalDeviceMemoryProperties");
        return false;
    }
    return true;
}

struct Buf
{
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;
};

struct Img
{
    VkImage img = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    uint32_t w = 0, h = 0;
};

struct PipeKey
{
    uint8_t topo = 0, cull = 0, depthWrite = 1, depthTest = 1, depthCompare = 3;
    uint8_t colorWriteMask = 0xF;
    uint8_t hasDestAlpha = 0; // GX pixel format RGBA6
    uint16_t blendMode = 0;   // GX BP 0x41 & 0xFFFF (enable + factors + subtract)
    uint64_t vsHash = 0, psHash = 0;
    bool operator==(const PipeKey& o) const
    {
        return topo == o.topo && cull == o.cull && depthWrite == o.depthWrite && depthTest == o.depthTest &&
               depthCompare == o.depthCompare && colorWriteMask == o.colorWriteMask &&
               hasDestAlpha == o.hasDestAlpha && blendMode == o.blendMode && vsHash == o.vsHash &&
               psHash == o.psHash;
    }
};
struct PipeKeyHash
{
    size_t operator()(const PipeKey& k) const
    {
        size_t h = (size_t)k.topo | ((size_t)k.cull << 8) | ((size_t)k.depthWrite << 16) |
                   ((size_t)k.depthTest << 17) | ((size_t)k.depthCompare << 18) |
                   ((size_t)k.colorWriteMask << 21) | ((size_t)k.hasDestAlpha << 25);
        h ^= (size_t)k.blendMode + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= k.vsHash + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h ^= k.psHash + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        return h;
    }
};

static VkBlendFactor GxBlendSrcToVk(uint32_t f, bool hasAlpha)
{
    switch (f & 7)
    {
    case 0: return VK_BLEND_FACTOR_ZERO;
    case 1: return VK_BLEND_FACTOR_ONE;
    case 2: return VK_BLEND_FACTOR_DST_COLOR;
    case 3: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case 4: return VK_BLEND_FACTOR_SRC_ALPHA;
    case 5: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case 6: return hasAlpha ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_ONE;
    default: return hasAlpha ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ZERO;
    }
}
static VkBlendFactor GxBlendDstToVk(uint32_t f, bool hasAlpha)
{
    switch (f & 7)
    {
    case 0: return VK_BLEND_FACTOR_ZERO;
    case 1: return VK_BLEND_FACTOR_ONE;
    case 2: return VK_BLEND_FACTOR_SRC_COLOR;
    case 3: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case 4: return VK_BLEND_FACTOR_SRC_ALPHA;
    case 5: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case 6: return hasAlpha ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_ONE;
    default: return hasAlpha ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ZERO;
    }
}
static VkBlendFactor GxBlendToAlphaFactor(VkBlendFactor c)
{
    switch (c)
    {
    case VK_BLEND_FACTOR_SRC_COLOR: return VK_BLEND_FACTOR_SRC_ALPHA;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case VK_BLEND_FACTOR_DST_COLOR: return VK_BLEND_FACTOR_DST_ALPHA;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    default: return c;
    }
}

static VkCompareOp GxDepthCompareToVk(uint8_t cmp)
{
    static const VkCompareOp kOps[8] = {
        VK_COMPARE_OP_NEVER,           VK_COMPARE_OP_LESS,           VK_COMPARE_OP_EQUAL,
        VK_COMPARE_OP_LESS_OR_EQUAL,   VK_COMPARE_OP_GREATER,        VK_COMPARE_OP_NOT_EQUAL,
        VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS,
    };
    return kOps[cmp & 7];
}

struct FrameSlot
{
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    Buf vb{}, ib{};
    VkDeviceSize vbPos = 0, ibPos = 0;
    bool recording = false;
    bool inEfbPass = false;
};

struct TexCacheEntry
{
    Img img{};
    uint64_t key = 0;
    uint32_t w = 0, h = 0;
};

std::mutex s_mu;
bool s_active = false;
int s_scale = 1;
uint32_t s_efbW = 640, s_efbH = 528;
VkGxStats s_stats{};

VkRenderPass s_efbPass = VK_NULL_HANDLE;
VkFramebuffer s_efbFb = VK_NULL_HANDLE;
Img s_efbColor{}, s_efbDepth{};
VkSampler s_sampNearest = VK_NULL_HANDLE;
VkSampler s_sampLinear = VK_NULL_HANDLE;

VkShaderModule s_blitVs = VK_NULL_HANDLE, s_blitFs = VK_NULL_HANDLE;
std::unordered_map<uint64_t, VkShaderModule> s_vsModuleCache;
std::unordered_map<uint64_t, VkShaderModule> s_fsModuleCache;
std::unordered_map<uint32_t, VkSampler> s_samplerCache;
VkDescriptorSetLayout s_dsl = VK_NULL_HANDLE;
VkPipelineLayout s_pipeLayout = VK_NULL_HANDLE;
VkDescriptorPool s_descPool = VK_NULL_HANDLE;
std::vector<VkDescriptorSet> s_frameDescSets;
uint32_t s_descCursor = 0;

Buf s_xfBuf{}, s_vsUbo{}, s_psUbo{}, s_dynUbo{};
VkDeviceSize s_dynUboPos = 0;
// A4g: versioned XF SSBO ring — a single live XF buffer is overwritten while prior
// draws in the same CB still reference it (TR1 skeletal/env → vertex explosion).
constexpr VkDeviceSize kXfSlotBytes = 0x1000u * sizeof(float); // 16 KiB
constexpr VkDeviceSize kXfRingBytes = 16u * 1024u * 1024u;
VkDeviceSize s_xfRingPos = 0;
VkDeviceSize s_xfBindOffset = 0;
Buf s_staging{}; // texture uploads
VkDeviceSize s_stagingPos = 0;
Img s_whiteTex{};
std::unordered_map<uint64_t, TexCacheEntry> s_texCache;

VkCommandPool s_pool = VK_NULL_HANDLE;
FrameSlot s_frames[kFrames];
int s_frameIdx = 0;
bool s_xfUploaded = false;
bool s_deviceLost = false;
VkCommandBuffer s_presentCmd = VK_NULL_HANDLE;

std::unordered_map<PipeKey, VkPipeline, PipeKeyHash> s_pipes;
VkPipeline s_blitPipe = VK_NULL_HANDLE;
VkRenderPass s_blitPass = VK_NULL_HANDLE; // unused — blit via barrier+blit
VkDescriptorSetLayout s_blitDsl = VK_NULL_HANDLE;
VkPipelineLayout s_blitLayout = VK_NULL_HANDLE;
VkDescriptorPool s_blitPool = VK_NULL_HANDLE;
VkDescriptorSet s_blitSet = VK_NULL_HANDLE;

// A4f: EFB→texture util copy + cache
struct CopyUtilUbo
{
    float rect[4];
    float uv[4];
    float color[4];
    float params[4];
    int32_t iparams[4];
};

struct EfbCopyCacheEntry
{
    Img img{};
    VkFramebuffer fb = VK_NULL_HANDLE;
    uint32_t w = 0, h = 0;   // scaled (GPU)
    uint32_t tw = 0, th = 0; // native GX texel dims
    uint64_t genId = 0;
    // D3D TexEntry parity: invalidate when guest RAM at destAddr changes.
    uint64_t ramHash = 0;
    uint32_t ramSize = 0;
    uint32_t hashedFrame = ~0u;
};

static uint64_t Hash64Bytes(const void* data, size_t n, uint64_t seed = 0x9E3779B97F4A7C15ull)
{
    const uint8_t* p = (const uint8_t*)data;
    const uint64_t M = 0x9FB21C651E98DF25ull;
    uint64_t h0 = seed ^ (n * 0xff51afd7ed558ccdull), h1 = h0 ^ 0x2545F4914F6CDD1Dull,
             h2 = h0 ^ 0x632BE59BD9B4E019ull, h3 = h0 ^ 0x85EBCA77C2B2AE63ull;
    size_t i = 0;
    for (; i + 32 <= n; i += 32)
    {
        uint64_t v0, v1, v2, v3;
        memcpy(&v0, p + i, 8);
        memcpy(&v1, p + i + 8, 8);
        memcpy(&v2, p + i + 16, 8);
        memcpy(&v3, p + i + 24, 8);
        h0 = (h0 ^ v0) * M;
        h0 ^= h0 >> 29;
        h1 = (h1 ^ v1) * M;
        h1 ^= h1 >> 29;
        h2 = (h2 ^ v2) * M;
        h2 ^= h2 >> 29;
        h3 = (h3 ^ v3) * M;
        h3 ^= h3 >> 29;
    }
    uint64_t h = h0 ^ (h1 * 3) ^ (h2 * 5) ^ (h3 * 7);
    for (; i + 8 <= n; i += 8)
    {
        uint64_t v;
        memcpy(&v, p + i, 8);
        h = (h ^ v) * M;
        h ^= h >> 29;
    }
    for (; i < n; i++)
    {
        h = (h ^ p[i]) * M;
        h ^= h >> 29;
    }
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdull;
    h ^= h >> 33;
    return h;
}

VkShaderModule s_copyVs = VK_NULL_HANDLE, s_copyFs = VK_NULL_HANDLE;
VkRenderPass s_copyPass = VK_NULL_HANDLE;
VkPipeline s_copyPipe = VK_NULL_HANDLE;
VkPipelineLayout s_copyLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout s_copyDsl = VK_NULL_HANDLE;
VkDescriptorPool s_copyPool = VK_NULL_HANDLE;
VkDescriptorSet s_copySet = VK_NULL_HANDLE;
Img s_efbColorSnap{}, s_efbDepthSnap{};
std::unordered_map<uint32_t, EfbCopyCacheEntry> s_efbCopyCache;
std::vector<EfbCopyCacheEntry> s_efbCopyRetire; // destroyed after next GPU wait
uint64_t s_efbCopyGen = 1;
// A4l: bound inline copies per CB so we never reintroduce A4h's unbounded full-EFB TDR.
// Soft-flush submits+waits once when the budget trips (still far fewer waits than per-copy sync).
uint32_t s_inlineCopiesSinceSubmit = 0;
uint64_t s_inlineCopyPixelsSinceSubmit = 0;
constexpr uint32_t kMaxInlineCopiesPerCb = 256;
// Bound transfer work per CB. A4h TDR was thousands of full-EFB blits; 64 full-EFB
// equivalents (region snaps) has been stable on RTX 3080 in A4l benches.
constexpr uint64_t kMaxInlineCopyPixelsPerCb = 640ull * 528ull * 64ull;

static uint64_t MonoNs()
{
    using clock = std::chrono::steady_clock;
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch()).count();
}

static void ResetInlineCopyBudget()
{
    s_inlineCopiesSinceSubmit = 0;
    s_inlineCopyPixelsSinceSubmit = 0;
}

static int EnvFlag(const char* name)
{
    const char* e = std::getenv(name);
    return (e && e[0] && e[0] != '0') ? 1 : 0;
}

std::unordered_map<std::string, uint32_t> s_unsupCounts;

static const char* kCopyVertGlsl = R"(#version 450
layout(location = 0) out vec2 vUV;
layout(push_constant) uniform PC {
    vec4 rect;
    vec4 uv;
    vec4 color;
    vec4 params;
    ivec4 iparams;
} u;
void main() {
    vec2 t = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
    float x = mix(u.rect.x, u.rect.z, t.x);
    float y = mix(u.rect.y, u.rect.w, t.y);
    gl_Position = vec4(x, y, u.params.x, 1.0);
    vUV = mix(u.uv.xy, u.uv.zw, t);
}
)";

static const char* kCopyFragGlsl = R"(#version 450
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D src;
layout(set = 0, binding = 1) uniform sampler2D srcDepth;
layout(push_constant) uniform PC {
    vec4 rect;
    vec4 uv;
    vec4 color;
    vec4 params;
    ivec4 iparams;
} u;
void main() {
    vec4 c;
    if (u.iparams.z != 0) {
        float d = texture(srcDepth, vUV).r;
        uint z = uint(clamp(d, 0.0, 1.0) * 16777215.0);
        float h = float((z >> 16) & 0xFFu) / 255.0;
        float m = float((z >> 8) & 0xFFu) / 255.0;
        float l = float(z & 0xFFu) / 255.0;
        c = vec4(h, m, l, h);
        switch (u.iparams.x) {
        case 0: case 1: case 8: c = vec4(h); break;
        case 9: c = vec4(m); break;
        case 10: c = vec4(l); break;
        case 3: c = vec4(m, m, m, h); break;
        case 11: case 12: c = vec4(m, l, l, m); break;
        default: c = vec4(h, m, l, 1.0); break;
        }
        outColor = c;
        return;
    }
    c = texture(src, vUV);
    if (u.iparams.w != 0) {
        vec2 d = u.params.zw * 0.25;
        c = (texture(src, vUV + vec2(-d.x, -d.y)) + texture(src, vUV + vec2(d.x, -d.y)) +
             texture(src, vUV + vec2(-d.x, d.y)) + texture(src, vUV + vec2(d.x, d.y))) * 0.25;
    }
    if (u.iparams.y != 0) {
        float y = dot(c.rgb, vec3(0.257, 0.504, 0.098)) + 16.0 / 255.0;
        c.rgb = vec3(y);
    }
    switch (u.iparams.x) {
    case 0: case 1: c = vec4(c.r); break;
    case 2: case 3: c = vec4(c.r, c.r, c.r, c.a); break;
    case 4: c.a = 1.0; break;
    case 7: c = vec4(c.a); break;
    case 8: c = vec4(c.r); break;
    case 9: c = vec4(c.g); break;
    case 10: c = vec4(c.b); break;
    case 11: c = vec4(c.r, c.r, c.r, c.g); break;
    case 12: c = vec4(c.g, c.g, c.g, c.b); break;
    default: break;
    }
    outColor = c;
}
)";

static uint32_t EfbCopyKey(uint32_t ea)
{
    if (!ea) return 0;
    const uint32_t hi = ea >> 28;
    if (hi == 0x8 || hi == 0x9 || hi == 0xC || hi == 0xD)
        return ea;
    return 0x80000000u | (ea & 0x1FFFFFFFu);
}

static void DestroyImg(Img& im);

static void DestroyEfbCopyEntry(EfbCopyCacheEntry& e)
{
    VkDevice dev = Vk_GetDevice();
    if (dev && e.fb)
        pfn_vkDestroyFramebuffer(dev, e.fb, nullptr);
    e.fb = VK_NULL_HANDLE;
    DestroyImg(e.img);
    e = {};
}

static void RetireEfbCopyEntries()
{
    for (auto& e : s_efbCopyRetire)
        DestroyEfbCopyEntry(e);
    s_efbCopyRetire.clear();
}

static uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mp{};
    pfn_vkGetPhysicalDeviceMemoryProperties(Vk_GetPhysicalDevice(), &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return 0;
}

static bool CreateBuf(Buf& b, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props)
{
    VkDevice dev = Vk_GetDevice();
    VkBufferCreateInfo ci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (pfn_vkCreateBuffer(dev, &ci, nullptr, &b.buf) != VK_SUCCESS) return false;
    VkMemoryRequirements req{};
    pfn_vkGetBufferMemoryRequirements(dev, b.buf, &req);
    VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, props);
    if (pfn_vkAllocateMemory(dev, &ai, nullptr, &b.mem) != VK_SUCCESS) return false;
    pfn_vkBindBufferMemory(dev, b.buf, b.mem, 0);
    b.size = size;
    if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        if (pfn_vkMapMemory(dev, b.mem, 0, size, 0, &b.mapped) != VK_SUCCESS) return false;
    }
    s_stats.bufferAllocs++;
    return true;
}

static void DestroyBuf(Buf& b)
{
    VkDevice dev = Vk_GetDevice();
    if (!dev) return;
    if (b.mapped) { pfn_vkUnmapMemory(dev, b.mem); b.mapped = nullptr; }
    if (b.buf) pfn_vkDestroyBuffer(dev, b.buf, nullptr);
    if (b.mem) pfn_vkFreeMemory(dev, b.mem, nullptr);
    b = {};
}

static bool CreateImg2D(Img& im, uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage,
                        VkImageAspectFlags aspect, uint32_t mipLevels = 1)
{
    VkDevice dev = Vk_GetDevice();
    mipLevels = std::max(1u, mipLevels);
    VkImageCreateInfo ci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = fmt;
    ci.extent = { w, h, 1 };
    ci.mipLevels = mipLevels;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (pfn_vkCreateImage(dev, &ci, nullptr, &im.img) != VK_SUCCESS) return false;
    VkMemoryRequirements req{};
    pfn_vkGetImageMemoryRequirements(dev, im.img, &req);
    VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (pfn_vkAllocateMemory(dev, &ai, nullptr, &im.mem) != VK_SUCCESS) return false;
    pfn_vkBindImageMemory(dev, im.img, im.mem, 0);
    VkImageViewCreateInfo vi{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vi.image = im.img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt;
    vi.subresourceRange.aspectMask = aspect;
    vi.subresourceRange.levelCount = mipLevels;
    vi.subresourceRange.layerCount = 1;
    if (pfn_vkCreateImageView(dev, &vi, nullptr, &im.view) != VK_SUCCESS) return false;
    im.w = w;
    im.h = h;
    return true;
}

static void DestroyImg(Img& im)
{
    VkDevice dev = Vk_GetDevice();
    if (!dev) return;
    if (im.view) pfn_vkDestroyImageView(dev, im.view, nullptr);
    if (im.img) pfn_vkDestroyImage(dev, im.img, nullptr);
    if (im.mem) pfn_vkFreeMemory(dev, im.mem, nullptr);
    im = {};
}

static VkShaderModule MakeModule(const uint32_t* words, uint32_t count)
{
    VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = (size_t)count * 4;
    ci.pCode = words;
    VkShaderModule m = VK_NULL_HANDLE;
    if (pfn_vkCreateShaderModule(Vk_GetDevice(), &ci, nullptr, &m) != VK_SUCCESS) return VK_NULL_HANDLE;
    s_stats.shaderModules++;
    return m;
}

static uint64_t HashKeyBytes(const void* p, size_t n)
{
    const uint8_t* b = static_cast<const uint8_t*>(p);
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < n; i++)
    {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}

static VkSamplerAddressMode WrapToVk(uint8_t wrap)
{
    switch (wrap & 3u)
    {
    case 0: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case 2: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

static VkSampler GetOrCreateSampler(const GfxTexBind& t)
{
    const uint32_t key = (uint32_t)t.wrapU | ((uint32_t)t.wrapV << 2) | ((uint32_t)t.magLinear << 4) |
                         ((uint32_t)t.minLinear << 5) | ((uint32_t)t.mipFilter << 6) |
                         ((uint32_t)(t.levels > 1 ? 1 : 0) << 7);
    auto it = s_samplerCache.find(key);
    if (it != s_samplerCache.end()) return it->second;
    VkSamplerCreateInfo si{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    si.magFilter = t.magLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.minFilter = t.minLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.addressModeU = WrapToVk(t.wrapU);
    si.addressModeV = WrapToVk(t.wrapV);
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (t.levels > 1)
    {
        // GX mip filters: nearest-mip vs linear-mip; always expose uploaded levels.
        si.mipmapMode = t.mipFilter ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.minLod = t.minLod;
        si.maxLod = t.maxLod > 0.0f ? t.maxLod : (float)(t.levels - 1);
    }
    else
    {
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.maxLod = 0.0f;
    }
    VkSampler samp = VK_NULL_HANDLE;
    if (pfn_vkCreateSampler(Vk_GetDevice(), &si, nullptr, &samp) != VK_SUCCESS)
        return s_sampNearest;
    s_samplerCache[key] = samp;
    return samp;
}

static bool GetOrCreateShaderModules(const VSKey& vsKey, const PSKey& psKey, VkShaderModule& vsMod,
                                     VkShaderModule& fsMod, std::string& err)
{
    const uint64_t vh = HashKeyBytes(&vsKey, sizeof(vsKey));
    const uint64_t ph = HashKeyBytes(&psKey, sizeof(psKey));

    {
        auto it = s_vsModuleCache.find(vh);
        if (it != s_vsModuleCache.end())
        {
            s_stats.shaderCacheHits++;
            vsMod = it->second;
        }
        else
        {
            s_stats.shaderCacheMisses++;
            std::string glsl = GenerateVertexShaderGLSL(vsKey);
            std::vector<uint32_t> spv;
            if (!VkSpv_CompileGlsl(glsl, "vert", spv, err))
            {
                LOG_ERROR("vulkan gx: VS SPIR-V failed numTexGens={} numColorChans={} dualTex={}: {}", vsKey.numTexGens,
                          vsKey.numColorChans, vsKey.dualTex, err);
                return false;
            }
            vsMod = MakeModule(spv.data(), (uint32_t)spv.size());
            if (!vsMod)
            {
                err = "vkCreateShaderModule (VS) failed";
                return false;
            }
            s_vsModuleCache[vh] = vsMod;
        }
    }

    {
        auto it = s_fsModuleCache.find(ph);
        if (it != s_fsModuleCache.end())
        {
            s_stats.shaderCacheHits++;
            fsMod = it->second;
        }
        else
        {
            s_stats.shaderCacheMisses++;
            std::string glsl = GeneratePixelShaderGLSL(psKey);
            std::vector<uint32_t> spv;
            if (!VkSpv_CompileGlsl(glsl, "frag", spv, err))
            {
                LOG_ERROR("vulkan gx: PS SPIR-V failed numTevStages={} numTexGens={} numIndStages={}: {}",
                          psKey.numTevStages, psKey.numTexGens, psKey.numIndStages, err);
                return false;
            }
            fsMod = MakeModule(spv.data(), (uint32_t)spv.size());
            if (!fsMod)
            {
                err = "vkCreateShaderModule (FS) failed";
                return false;
            }
            s_fsModuleCache[ph] = fsMod;
        }
    }
    return true;
}

static void Barrier(VkCommandBuffer cmd, VkImage img, VkImageLayout oldL, VkImageLayout newL,
                    VkImageAspectFlags aspect, VkPipelineStageFlags srcSt, VkPipelineStageFlags dstSt,
                    VkAccessFlags srcA, VkAccessFlags dstA, uint32_t mipLevels = 1)
{
    VkImageMemoryBarrier b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.oldLayout = oldL;
    b.newLayout = newL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = aspect;
    b.subresourceRange.levelCount = mipLevels;
    b.subresourceRange.layerCount = 1;
    b.srcAccessMask = srcA;
    b.dstAccessMask = dstA;
    pfn_vkCmdPipelineBarrier(cmd, srcSt, dstSt, 0, 0, nullptr, 0, nullptr, 1, &b);
}

static bool EnsureFrameRecording(FrameSlot& fr, std::string& error)
{
    if (fr.recording) return true;
    pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
    pfn_vkResetFences(Vk_GetDevice(), 1, &fr.fence);
    pfn_vkResetCommandBuffer(fr.cmd, 0);
    fr.vbPos = 0;
    fr.ibPos = 0;
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (pfn_vkBeginCommandBuffer(fr.cmd, &bi) != VK_SUCCESS)
    {
        error = "pfn_vkBeginCommandBuffer failed";
        return false;
    }
    fr.recording = true;
    fr.inEfbPass = false;
    return true;
}

static void ResetFrameDescriptors();

static bool BeginEfbPass(FrameSlot& fr, bool clear, const float* clearCol, float clearDepth)
{
    if (fr.inEfbPass) return true;
    VkClearValue cvs[2]{};
    if (clearCol)
    {
        cvs[0].color.float32[0] = clearCol[0];
        cvs[0].color.float32[1] = clearCol[1];
        cvs[0].color.float32[2] = clearCol[2];
        cvs[0].color.float32[3] = clearCol[3];
    }
    cvs[1].depthStencil.depth = clearDepth;
    VkRenderPassBeginInfo rp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rp.renderPass = s_efbPass;
    rp.framebuffer = s_efbFb;
    rp.renderArea.extent = { s_efbW, s_efbH };
    // Use secondary clear via attachments when requested; pass loadOp is LOAD for continue.
    // We created two render pass variants? Simpler: always LOAD, use CmdClearAttachments for clears.
    rp.clearValueCount = 0;
    rp.pClearValues = nullptr;
    (void)clear;
    (void)cvs;
    pfn_vkCmdBeginRenderPass(fr.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    fr.inEfbPass = true;
    return true;
}

static void EndEfbPass(FrameSlot& fr)
{
    if (!fr.inEfbPass) return;
    pfn_vkCmdEndRenderPass(fr.cmd);
    fr.inEfbPass = false;
}

static bool DrainFrameSlot(FrameSlot& fr, std::string& error, bool accountEfbCopy = false)
{
    if (!fr.recording) return true;
    EndEfbPass(fr);
    pfn_vkEndCommandBuffer(fr.cmd);
    fr.recording = false;
    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers = &fr.cmd;
    const uint64_t t0 = accountEfbCopy ? MonoNs() : 0;
    VkResult sr = LoadDev<PFN_vkQueueSubmit>("vkQueueSubmit")(Vk_GetGraphicsQueue(), 1, &si, fr.fence);
    const uint64_t t1 = accountEfbCopy ? MonoNs() : 0;
    if (sr != VK_SUCCESS)
    {
        if (sr == VK_ERROR_DEVICE_LOST) s_deviceLost = true;
        error = "vkQueueSubmit (drain) failed";
        return false;
    }
    const uint64_t tw0 = accountEfbCopy ? MonoNs() : 0;
    pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
    if (accountEfbCopy)
    {
        s_stats.efbCopySubmitNs += (t1 - t0);
        s_stats.efbCopyFenceWaits++;
        s_stats.efbCopyFenceWaitNs += MonoNs() - tw0;
    }
    RetireEfbCopyEntries();
    // GPU idle for this slot — recycle host-visible arenas (do not wrap into live ranges).
    ResetFrameDescriptors();
    s_dynUboPos = 0;
    s_xfRingPos = 0;
    s_xfUploaded = false;
    s_stats.xfRingWraps++;
    fr.vbPos = 0;
    fr.ibPos = 0;
    ResetInlineCopyBudget();
    return true;
}

// A4l: bound CB size without discarding XF/descriptor/VB arenas (unlike DrainFrameSlot).
// Used when the inline EFB-copy budget trips mid-frame — avoids A4g-class XF disruption.
static bool SoftSubmitContinue(FrameSlot& fr, std::string& error, bool accountEfbCopy)
{
    if (!fr.recording) return true;
    EndEfbPass(fr);
    pfn_vkEndCommandBuffer(fr.cmd);
    fr.recording = false;
    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers = &fr.cmd;
    const uint64_t t0 = accountEfbCopy ? MonoNs() : 0;
    VkResult sr = LoadDev<PFN_vkQueueSubmit>("vkQueueSubmit")(Vk_GetGraphicsQueue(), 1, &si, fr.fence);
    const uint64_t t1 = accountEfbCopy ? MonoNs() : 0;
    if (sr != VK_SUCCESS)
    {
        if (sr == VK_ERROR_DEVICE_LOST) s_deviceLost = true;
        error = "vkQueueSubmit (soft) failed";
        return false;
    }
    const uint64_t tw0 = accountEfbCopy ? MonoNs() : 0;
    pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
    if (accountEfbCopy)
    {
        s_stats.efbCopySubmitNs += (t1 - t0);
        s_stats.efbCopyFenceWaits++;
        s_stats.efbCopyFenceWaitNs += MonoNs() - tw0;
        s_stats.efbCopySoftFlushes++;
    }
    RetireEfbCopyEntries();
    ResetInlineCopyBudget();
    // Re-open the same slot; keep vb/ib/xf/dynUbo positions (GPU retired prior ranges).
    pfn_vkResetFences(Vk_GetDevice(), 1, &fr.fence);
    pfn_vkResetCommandBuffer(fr.cmd, 0);
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (pfn_vkBeginCommandBuffer(fr.cmd, &bi) != VK_SUCCESS)
    {
        error = "soft BeginCommandBuffer failed";
        return false;
    }
    fr.recording = true;
    fr.inEfbPass = false;
    return true;
}

static bool LookupEfbCopyLocked(uint32_t destAddr, VkImageView& outView, uint32_t& outW, uint32_t& outH,
                                uint32_t gxFrame)
{
    const uint32_t key = EfbCopyKey(destAddr);
    if (!key) return false;
    auto it = s_efbCopyCache.find(key);
    if (it == s_efbCopyCache.end() || !it->second.img.view) return false;
    EfbCopyCacheEntry& e = it->second;
    // Same-frame: always trust the GPU copy (matches D3D hashedFrame == s_frame).
    // Later frames: keep only while guest RAM at the dest still matches the fingerprint
    // taken at copy time — otherwise the game replaced it with a CPU texture.
    // Do not DestroyImg here (may still be referenced by the in-flight draw CB); treat as
    // a miss so the Flush path decodes from RAM. A later copy to the same EA refreshes.
    if (gxFrame != ~0u && e.ramSize != 0 && e.hashedFrame != gxFrame)
    {
        const uint64_t now = Hash64Bytes(PhysPtr(destAddr), e.ramSize);
        if (now != e.ramHash)
            return false;
    }
    outView = e.img.view;
    outW = e.w;
    outH = e.h;
    return true;
}

static VkPipeline GetOrCreatePipe(const PipeKey& key, std::string& error)
{
    auto it = s_pipes.find(key);
    if (it != s_pipes.end())
    {
        s_stats.pipelineHits++;
        return it->second;
    }
    s_stats.pipelineMisses++;

    VkVertexInputBindingDescription bind{};
    bind.binding = 0;
    bind.stride = sizeof(HostVertex);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[] = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(HostVertex, pos) },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(HostVertex, nrm) },
        { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(HostVertex, bin) },
        { 3, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(HostVertex, tan) },
        { 4, 0, VK_FORMAT_R8G8B8A8_UNORM, (uint32_t)offsetof(HostVertex, col[0]) },
        { 5, 0, VK_FORMAT_R8G8B8A8_UNORM, (uint32_t)offsetof(HostVertex, col[1]) },
        { 6, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(HostVertex, tex[0]) },
        { 7, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(HostVertex, tex[1]) },
        { 8, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(HostVertex, tex[2]) },
        { 9, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(HostVertex, tex[3]) },
        { 10, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(HostVertex, tex[4]) },
        { 11, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(HostVertex, tex[5]) },
        { 12, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(HostVertex, tex[6]) },
        { 13, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(HostVertex, tex[7]) },
        { 14, 0, VK_FORMAT_R32G32B32_UINT, (uint32_t)offsetof(HostVertex, mtx) },
    };

    VkPipelineVertexInputStateCreateInfo vi{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = (uint32_t)(sizeof(attrs) / sizeof(attrs[0]));
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = key.topo == 1   ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                  : key.topo == 2 ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST
                                  : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    // Match D3D11: GX front faces are clockwise in screen space (FrontCounterClockwise=FALSE).
    // A4g used CCW to "compensate" for clip.y = -clip.y; that made cull-back discard the
    // intended front faces (sky/background and solid meshes). Keep clip.y negate for NDC Y
    // position parity; do not invert winding/cull on top of it. See GfxCullHwMode.
    {
        const uint8_t hw = GfxCullHwMode(static_cast<GfxCull>(key.cull));
        rs.cullMode = hw == 2   ? VK_CULL_MODE_BACK_BIT
                      : hw == 1 ? VK_CULL_MODE_FRONT_BIT
                                : VK_CULL_MODE_NONE;
    }
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds.depthTestEnable = key.depthTest ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = key.depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = GxDepthCompareToVk(key.depthCompare);

    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = key.colorWriteMask & 0xF;
    const uint32_t bm = key.blendMode;
    const bool blendEn = (bm & 1) != 0;
    const bool hasAlpha = key.hasDestAlpha != 0;
    if (blendEn)
    {
        ba.blendEnable = VK_TRUE;
        const bool sub = (bm >> 11) & 1;
        if (sub)
        {
            ba.srcColorBlendFactor = ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            ba.colorBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
            ba.srcAlphaBlendFactor = ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            ba.alphaBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
        }
        else
        {
            const uint32_t dstF = (bm >> 5) & 7, srcF = (bm >> 8) & 7;
            ba.srcColorBlendFactor = GxBlendSrcToVk(srcF, hasAlpha);
            ba.dstColorBlendFactor = GxBlendDstToVk(dstF, hasAlpha);
            ba.colorBlendOp = VK_BLEND_OP_ADD;
            ba.srcAlphaBlendFactor = GxBlendToAlphaFactor(ba.srcColorBlendFactor);
            ba.dstAlphaBlendFactor = GxBlendToAlphaFactor(ba.dstColorBlendFactor);
            ba.alphaBlendOp = VK_BLEND_OP_ADD;
        }
    }
    VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;

    VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dy{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;

    auto vit = s_vsModuleCache.find(key.vsHash);
    auto fit = s_fsModuleCache.find(key.psHash);
    if (vit == s_vsModuleCache.end() || fit == s_fsModuleCache.end())
    {
        error = "shader modules missing for pipeline";
        return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vit->second;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fit->second;
    stages[1].pName = "main";

    VkGraphicsPipelineCreateInfo pci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pci.stageCount = 2;
    pci.pStages = stages;
    pci.pVertexInputState = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pDepthStencilState = &ds;
    pci.pColorBlendState = &cb;
    pci.pDynamicState = &dy;
    pci.layout = s_pipeLayout;
    pci.renderPass = s_efbPass;

    VkPipeline pipe = VK_NULL_HANDLE;
    if (pfn_vkCreateGraphicsPipelines(Vk_GetDevice(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipe) != VK_SUCCESS)
    {
        error = "pfn_vkCreateGraphicsPipelines (GX) failed";
        return VK_NULL_HANDLE;
    }
    s_pipes[key] = pipe;
    LOG_INFO("vulkan gx: created pipeline topo={} cull={} blend={:04x} cwm={:x} depth={}/{}", (int)key.topo,
             (int)key.cull, (unsigned)key.blendMode, (unsigned)key.colorWriteMask, (int)key.depthTest,
             (int)key.depthWrite);
    return pipe;
}

static bool UploadTex(const GfxTexBind& t, VkImageView& outView, std::string& error)
{
    if (!t.use)
    {
        outView = s_whiteTex.view;
        return true;
    }
    if (t.efbCopyAddr)
    {
        uint32_t ew = 0, eh = 0;
        // Frame already validated in renderer Flush; ~0u skips re-hash under draw lock.
        if (LookupEfbCopyLocked(t.efbCopyAddr, outView, ew, eh, ~0u)) return true;
        outView = s_whiteTex.view;
        return true;
    }
    if (!t.rgba || t.w == 0 || t.h == 0)
    {
        outView = s_whiteTex.view;
        return true;
    }
    const uint32_t levels = std::max(1u, std::min(t.levels, 16u));
    auto it = s_texCache.find(t.key);
    if (it != s_texCache.end() && it->second.img.view)
    {
        outView = it->second.img.view;
        return true;
    }
    FrameSlot& fr = s_frames[s_frameIdx];
    if (!EnsureFrameRecording(fr, error)) return false;
    EndEfbPass(fr);

    auto align4 = [](VkDeviceSize v) { return (v + 3) & ~VkDeviceSize(3); };
    VkDeviceSize stagingOff = align4(s_stagingPos);
    std::vector<VkBufferImageCopy> copies;
    copies.reserve(levels);
    uint32_t lw = t.w, lh = t.h;
    VkDeviceSize uploaded = 0;
    for (uint32_t lv = 0; lv < levels; lv++)
    {
        const uint32_t* px = lv == 0 ? t.rgba : (t.mipRgba[lv] ? t.mipRgba[lv] : nullptr);
        if (!px)
        {
            error = "mip level missing rgba";
            return false;
        }
        const uint32_t mw = lv == 0 ? t.w : (t.mipW[lv] ? t.mipW[lv] : std::max(1u, lw / 2));
        const uint32_t mh = lv == 0 ? t.h : (t.mipH[lv] ? t.mipH[lv] : std::max(1u, lh / 2));
        const VkDeviceSize bytes = (VkDeviceSize)mw * mh * 4;
        if (bytes > s_staging.size)
        {
            error = "texture larger than staging buffer";
            return false;
        }
        if (stagingOff + bytes > s_staging.size)
        {
            if (!DrainFrameSlot(fr, error)) return false;
            s_stagingPos = 0;
            stagingOff = 0;
            if (!EnsureFrameRecording(fr, error)) return false;
        }
        memcpy((uint8_t*)s_staging.mapped + stagingOff, px, (size_t)bytes);
        VkBufferImageCopy region{};
        region.bufferOffset = stagingOff;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = lv;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = { mw, mh, 1 };
        copies.push_back(region);
        uploaded += bytes;
        stagingOff += bytes;
        stagingOff = align4(stagingOff);
        lw = mw;
        lh = mh;
    }
    s_stagingPos = stagingOff;

    TexCacheEntry e{};
    e.key = t.key;
    e.w = t.w;
    e.h = t.h;
    if (!CreateImg2D(e.img, t.w, t.h, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, levels))
    {
        error = "CreateImg2D tex failed";
        return false;
    }
    Barrier(fr.cmd, e.img.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            VK_ACCESS_TRANSFER_WRITE_BIT, levels);
    pfn_vkCmdCopyBufferToImage(fr.cmd, s_staging.buf, e.img.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               (uint32_t)copies.size(), copies.data());
    Barrier(fr.cmd, e.img.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, levels);

    s_texCache[t.key] = std::move(e);
    outView = s_texCache[t.key].img.view;
    s_stats.texUploads++;
    if (levels > 1) s_stats.texMipUploads++;
    s_stats.uploadBytes += uploaded;
    return true;
}

// A4d: never update a descriptor set after it has been bound in the current CB
// (Vulkan forbids that without UPDATE_AFTER_BIND). Allocate a fresh set per draw.
static bool s_allocNeedFlush = false;

static bool AllocDrawDescriptors(const GfxDrawBatch& batch, VkImageView views[8], VkSampler samps[8],
                                 VkDescriptorSet& outSet, std::string& error)
{
    // Never reset the pool while the current CB still references allocated sets.
    // Exhaustion → signal caller to flushFrame (submit + wait + reset), then retry.
    s_allocNeedFlush = false;
    constexpr VkDeviceSize kUboAlign = 256;
    auto alignUbo = [](VkDeviceSize v) { return (v + kUboAlign - 1) / kUboAlign * kUboAlign; };
    const VkDeviceSize vsOff = alignUbo(s_dynUboPos);
    const VkDeviceSize psOff = alignUbo(vsOff + sizeof(VSConstants));
    const VkDeviceSize slotEnd = alignUbo(psOff + sizeof(PSConstants));
    if (s_descCursor >= s_frameDescSets.size() || slotEnd > s_dynUbo.size)
    {
        s_allocNeedFlush = true;
        error = "per-frame descriptor/dynUbo exhausted (flush required)";
        return false;
    }
    memcpy((uint8_t*)s_dynUbo.mapped + vsOff, &batch.vs, sizeof(VSConstants));
    memcpy((uint8_t*)s_dynUbo.mapped + psOff, &batch.ps, sizeof(PSConstants));
    s_dynUboPos = slotEnd;

    VkDescriptorSet set = s_frameDescSets[s_descCursor++];

    VkDescriptorBufferInfo xf{};
    xf.buffer = s_xfBuf.buf;
    xf.offset = s_xfBindOffset;
    xf.range = kXfSlotBytes;
    VkDescriptorBufferInfo vs{};
    vs.buffer = s_dynUbo.buf;
    vs.offset = vsOff;
    vs.range = sizeof(VSConstants);
    VkDescriptorBufferInfo ps{};
    ps.buffer = s_dynUbo.buf;
    ps.offset = psOff;
    ps.range = sizeof(PSConstants);
    VkDescriptorImageInfo imgs[8]{};
    for (int i = 0; i < 8; i++)
    {
        imgs[i].sampler = samps[i];
        imgs[i].imageView = views[i];
        imgs[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet writes[4]{};
    for (int i = 0; i < 4; i++)
    {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = (uint32_t)i;
    }
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &xf;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &vs;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].descriptorCount = 1;
    writes[2].pBufferInfo = &ps;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].descriptorCount = 8;
    writes[3].pImageInfo = imgs;
    pfn_vkUpdateDescriptorSets(Vk_GetDevice(), 4, writes, 0, nullptr);
    outSet = set;
    return true;
}

static void ResetFrameDescriptors()
{
    // Only safe when no CB referencing these sets is in flight (caller waited).
    pfn_vkResetDescriptorPool(Vk_GetDevice(), s_descPool, 0);
    std::vector<VkDescriptorSetLayout> layouts(s_frameDescSets.size(), s_dsl);
    VkDescriptorSetAllocateInfo dai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dai.descriptorPool = s_descPool;
    dai.descriptorSetCount = (uint32_t)layouts.size();
    dai.pSetLayouts = layouts.data();
    pfn_vkAllocateDescriptorSets(Vk_GetDevice(), &dai, s_frameDescSets.data());
    s_descCursor = 0;
}

static void DestroyCopyUtil()
{
    VkDevice dev = Vk_GetDevice();
    if (!dev) return;
    for (auto& kv : s_efbCopyCache)
        DestroyEfbCopyEntry(kv.second);
    s_efbCopyCache.clear();
    RetireEfbCopyEntries();
    DestroyImg(s_efbColorSnap);
    DestroyImg(s_efbDepthSnap);
    if (s_copyPipe) pfn_vkDestroyPipeline(dev, s_copyPipe, nullptr);
    if (s_copyPass) pfn_vkDestroyRenderPass(dev, s_copyPass, nullptr);
    if (s_copyLayout) pfn_vkDestroyPipelineLayout(dev, s_copyLayout, nullptr);
    if (s_copyPool) pfn_vkDestroyDescriptorPool(dev, s_copyPool, nullptr);
    if (s_copyDsl) pfn_vkDestroyDescriptorSetLayout(dev, s_copyDsl, nullptr);
    if (s_copyVs) pfn_vkDestroyShaderModule(dev, s_copyVs, nullptr);
    if (s_copyFs) pfn_vkDestroyShaderModule(dev, s_copyFs, nullptr);
    s_copyPipe = VK_NULL_HANDLE;
    s_copyPass = VK_NULL_HANDLE;
    s_copyLayout = VK_NULL_HANDLE;
    s_copyPool = VK_NULL_HANDLE;
    s_copyDsl = VK_NULL_HANDLE;
    s_copySet = VK_NULL_HANDLE;
    s_copyVs = s_copyFs = VK_NULL_HANDLE;
}

static bool InitCopyUtil(std::string& error)
{
    VkDevice dev = Vk_GetDevice();
    if (!CreateImg2D(s_efbColorSnap, s_efbW, s_efbH, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT))
    {
        error = "efb color snap failed";
        return false;
    }
    if (!CreateImg2D(s_efbDepthSnap, s_efbW, s_efbH, VK_FORMAT_D32_SFLOAT,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT))
    {
        error = "efb depth snap failed";
        return false;
    }

    std::vector<uint32_t> vspv, fspv;
    if (!VkSpv_CompileGlsl(kCopyVertGlsl, "vert", vspv, error) ||
        !VkSpv_CompileGlsl(kCopyFragGlsl, "frag", fspv, error))
    {
        error = "efb-copy SPIR-V compile failed: " + error;
        return false;
    }
    s_copyVs = MakeModule(vspv.data(), (uint32_t)vspv.size());
    s_copyFs = MakeModule(fspv.data(), (uint32_t)fspv.size());
    if (!s_copyVs || !s_copyFs)
    {
        error = "efb-copy shader modules failed";
        return false;
    }

    {
        VkAttachmentDescription att{};
        att.format = VK_FORMAT_R8G8B8A8_UNORM;
        att.samples = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference cref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &cref;
        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 1;
        rpci.pAttachments = &att;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &sub;
        if (pfn_vkCreateRenderPass(dev, &rpci, nullptr, &s_copyPass) != VK_SUCCESS)
        {
            error = "efb-copy render pass failed";
            return false;
        }
    }

    {
        VkDescriptorSetLayoutBinding binds[2]{};
        binds[0] = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        binds[1] = { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        VkDescriptorSetLayoutCreateInfo lci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        lci.bindingCount = 2;
        lci.pBindings = binds;
        if (pfn_vkCreateDescriptorSetLayout(dev, &lci, nullptr, &s_copyDsl) != VK_SUCCESS)
        {
            error = "efb-copy dsl failed";
            return false;
        }
        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcr.offset = 0;
        pcr.size = sizeof(CopyUtilUbo);
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &s_copyDsl;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
        if (pfn_vkCreatePipelineLayout(dev, &plci, nullptr, &s_copyLayout) != VK_SUCCESS)
        {
            error = "efb-copy pipeline layout failed";
            return false;
        }
        VkDescriptorPoolSize ps{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 };
        VkDescriptorPoolCreateInfo dpi{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpi.maxSets = 1;
        dpi.poolSizeCount = 1;
        dpi.pPoolSizes = &ps;
        if (pfn_vkCreateDescriptorPool(dev, &dpi, nullptr, &s_copyPool) != VK_SUCCESS)
        {
            error = "efb-copy descriptor pool failed";
            return false;
        }
        VkDescriptorSetAllocateInfo dai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        dai.descriptorPool = s_copyPool;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &s_copyDsl;
        if (pfn_vkAllocateDescriptorSets(dev, &dai, &s_copySet) != VK_SUCCESS)
        {
            error = "efb-copy descriptor set failed";
            return false;
        }
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = s_copyVs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = s_copyFs;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;
    VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dy{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo pci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pci.stageCount = 2;
    pci.pStages = stages;
    pci.pVertexInputState = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pDepthStencilState = &ds;
    pci.pColorBlendState = &cb;
    pci.pDynamicState = &dy;
    pci.layout = s_copyLayout;
    pci.renderPass = s_copyPass;
    if (pfn_vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pci, nullptr, &s_copyPipe) != VK_SUCCESS)
    {
        error = "efb-copy pipeline failed";
        return false;
    }

    VkDescriptorImageInfo snapColor{};
    snapColor.sampler = s_sampLinear;
    snapColor.imageView = s_efbColorSnap.view;
    snapColor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo snapDepth{};
    snapDepth.sampler = s_sampLinear;
    snapDepth.imageView = s_efbDepthSnap.view;
    snapDepth.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet writes[2]{};
    for (int i = 0; i < 2; i++)
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = s_copySet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &snapColor;
    writes[1].dstSet = s_copySet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &snapDepth;
    pfn_vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);
    return true;
}

static bool EnsureEfbCopyDest(EfbCopyCacheEntry& e, uint32_t rw, uint32_t rh, uint32_t tw, uint32_t th)
{
    VkDevice dev = Vk_GetDevice();
    if (e.img.view && e.w == rw && e.h == rh) return true;
    // A4l: defer destroy until after a GPU wait — size churn must not force a mid-CB sync.
    if (e.img.view || e.fb)
    {
        s_efbCopyRetire.push_back(std::move(e));
        e = {};
    }
    if (!CreateImg2D(e.img, rw, rh, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT))
        return false;
    e.w = rw;
    e.h = rh;
    e.tw = tw;
    e.th = th;
    e.genId = 0; // new image → UNDEFINED layout path in RecordEfbCopyCommands
    VkFramebufferCreateInfo fi{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    fi.renderPass = s_copyPass;
    fi.attachmentCount = 1;
    fi.pAttachments = &e.img.view;
    fi.width = rw;
    fi.height = rh;
    fi.layers = 1;
    if (pfn_vkCreateFramebuffer(dev, &fi, nullptr, &e.fb) != VK_SUCCESS)
        return false;
    return true;
}

} // namespace

bool VkGx_Init(const VkGxInitOptions& opt, std::string& error)
{
    std::lock_guard lk(s_mu);
    if (s_active)
    {
        error = "VkGx already initialised";
        return false;
    }
    if (!Vk_IsReady())
    {
        error = "Vk_Init must succeed before VkGx_Init";
        return false;
    }
    if (!LoadFns())
    {
        error = "failed to load Vulkan device functions for GX";
        return false;
    }
    s_scale = std::max(1, opt.efbScale);
    s_efbW = (uint32_t)(kEfbW * s_scale);
    s_efbH = (uint32_t)(kEfbH * s_scale);
    s_stats = {};
    s_unsupCounts.clear();

    VkDevice dev = Vk_GetDevice();

    // EFB render pass: color + depth, LOAD/STORE (clears via CmdClearAttachments).
    {
        VkAttachmentDescription atts[2]{};
        // Match common swapchain UNORM (B8G8R8A8) so vkCmdBlitImage is portable (RADV + NVIDIA).
        atts[0].format = VK_FORMAT_B8G8R8A8_UNORM;
        atts[0].samples = VK_SAMPLE_COUNT_1_BIT;
        atts[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        atts[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        atts[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atts[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        atts[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        atts[1].format = VK_FORMAT_D32_SFLOAT;
        atts[1].samples = VK_SAMPLE_COUNT_1_BIT;
        atts[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        atts[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        atts[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atts[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        atts[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &colorRef;
        sub.pDepthStencilAttachment = &depthRef;
        VkRenderPassCreateInfo rp{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rp.attachmentCount = 2;
        rp.pAttachments = atts;
        rp.subpassCount = 1;
        rp.pSubpasses = &sub;
        if (pfn_vkCreateRenderPass(dev, &rp, nullptr, &s_efbPass) != VK_SUCCESS)
        {
            error = "efb render pass failed";
            return false;
        }
    }

    if (!CreateImg2D(s_efbColor, s_efbW, s_efbH, VK_FORMAT_B8G8R8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT))
    {
        error = "efb color create failed";
        return false;
    }
    if (!CreateImg2D(s_efbDepth, s_efbW, s_efbH, VK_FORMAT_D32_SFLOAT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                     VK_IMAGE_ASPECT_DEPTH_BIT))
    {
        error = "efb depth create failed";
        return false;
    }

    {
        VkImageView views[2] = { s_efbColor.view, s_efbDepth.view };
        VkFramebufferCreateInfo fi{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fi.renderPass = s_efbPass;
        fi.attachmentCount = 2;
        fi.pAttachments = views;
        fi.width = s_efbW;
        fi.height = s_efbH;
        fi.layers = 1;
        if (pfn_vkCreateFramebuffer(dev, &fi, nullptr, &s_efbFb) != VK_SUCCESS)
        {
            error = "efb framebuffer failed";
            return false;
        }
    }

    // Transition EFB to attachment layouts + clear once.
    {
        VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pci.queueFamilyIndex = Vk_GetGraphicsQueueFamily();
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (pfn_vkCreateCommandPool(dev, &pci, nullptr, &s_pool) != VK_SUCCESS)
        {
            error = "gx command pool failed";
            return false;
        }
        VkCommandBufferAllocateInfo cai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cai.commandPool = s_pool;
        cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = kFrames + 1;
        VkCommandBuffer cmds[kFrames + 1]{};
        if (pfn_vkAllocateCommandBuffers(dev, &cai, cmds) != VK_SUCCESS)
        {
            error = "gx cmd alloc failed";
            return false;
        }
        for (int i = 0; i < kFrames; i++)
        {
            s_frames[i].cmd = cmds[i];
            VkFenceCreateInfo fi{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            if (pfn_vkCreateFence(dev, &fi, nullptr, &s_frames[i].fence) != VK_SUCCESS)
            {
                error = "gx fence failed";
                return false;
            }
            if (!CreateBuf(s_frames[i].vb, kArenaBytes,
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ||
                !CreateBuf(s_frames[i].ib, kArenaBytes,
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                error = "gx ring buffer alloc failed";
                return false;
            }
        }
        // Dedicated present CB — never shared with the GX draw thread's in-flight recording.
        s_presentCmd = cmds[kFrames];

        VkCommandBuffer setup = cmds[0];
        pfn_vkResetFences(dev, 1, &s_frames[0].fence);
        pfn_vkResetCommandBuffer(setup, 0);
        VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        pfn_vkBeginCommandBuffer(setup, &bi);
        Barrier(setup, s_efbColor.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                VK_ACCESS_TRANSFER_WRITE_BIT);
        Barrier(setup, s_efbDepth.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_ASPECT_DEPTH_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                VK_ACCESS_TRANSFER_WRITE_BIT);
        VkClearColorValue cc{};
        VkImageSubresourceRange cr{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        auto pfnClearColor =
            LoadDev<PFN_vkCmdClearColorImage>("vkCmdClearColorImage");
        auto pfnClearDepth =
            LoadDev<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage");
        if (pfnClearColor) pfnClearColor(setup, s_efbColor.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cc, 1, &cr);
        VkClearDepthStencilValue cd{ 1.0f, 0 };
        VkImageSubresourceRange dr{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        if (pfnClearDepth)
            pfnClearDepth(setup, s_efbDepth.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cd, 1, &dr);
        Barrier(setup, s_efbColor.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        Barrier(setup, s_efbDepth.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        pfn_vkEndCommandBuffer(setup);
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &setup;
        auto qsubmit = LoadDev<PFN_vkQueueSubmit>("vkQueueSubmit");
        qsubmit(Vk_GetGraphicsQueue(), 1, &si, s_frames[0].fence);
        pfn_vkWaitForFences(dev, 1, &s_frames[0].fence, VK_TRUE, UINT64_MAX);
    }

    if (!CreateBuf(s_xfBuf, kXfRingBytes,
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ||
        !CreateBuf(s_vsUbo, sizeof(VSConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ||
        !CreateBuf(s_psUbo, 64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ||
        !CreateBuf(s_staging, 16u * 1024u * 1024u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ||
        !CreateBuf(s_dynUbo, 8u * 1024u * 1024u, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        error = "gx uniform/ssbo alloc failed";
        return false;
    }
    s_dynUboPos = 0;
    s_xfRingPos = 0;
    s_xfBindOffset = 0;
    s_xfUploaded = false;
    s_stats.xfRingBytes = kXfRingBytes;

    // White 1x1
    {
        if (!CreateImg2D(s_whiteTex, 1, 1, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT))
        {
            error = "white tex failed";
            return false;
        }
        uint32_t white = 0xFFFFFFFFu;
        memcpy(s_staging.mapped, &white, 4);
        VkCommandBuffer cmd = s_frames[0].cmd;
        pfn_vkResetFences(dev, 1, &s_frames[0].fence);
        pfn_vkResetCommandBuffer(cmd, 0);
        VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        pfn_vkBeginCommandBuffer(cmd, &bi);
        Barrier(cmd, s_whiteTex.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = { 1, 1, 1 };
        pfn_vkCmdCopyBufferToImage(cmd, s_staging.buf, s_whiteTex.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        Barrier(cmd, s_whiteTex.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        pfn_vkEndCommandBuffer(cmd);
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        LoadDev<PFN_vkQueueSubmit>("vkQueueSubmit")(Vk_GetGraphicsQueue(), 1, &si, s_frames[0].fence);
        pfn_vkWaitForFences(dev, 1, &s_frames[0].fence, VK_TRUE, UINT64_MAX);
    }

    {
        VkSamplerCreateInfo si{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        si.magFilter = VK_FILTER_NEAREST;
        si.minFilter = VK_FILTER_NEAREST;
        si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        pfn_vkCreateSampler(dev, &si, nullptr, &s_sampNearest);
        si.magFilter = si.minFilter = VK_FILTER_LINEAR;
        pfn_vkCreateSampler(dev, &si, nullptr, &s_sampLinear);
    }

    if (!InitCopyUtil(error))
    {
        DestroyCopyUtil();
        return false;
    }

    s_blitVs = MakeModule(k_blit_vert_spv, k_blit_vert_spv_words);
    s_blitFs = MakeModule(k_blit_frag_spv, k_blit_frag_spv_words);
    if (!s_blitVs || !s_blitFs)
    {
        error = "blit shader module creation failed";
        return false;
    }

    {
        VkDescriptorSetLayoutBinding binds[4]{};
        binds[0].binding = 0;
        binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binds[0].descriptorCount = 1;
        binds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        binds[1].binding = 1;
        binds[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binds[1].descriptorCount = 1;
        binds[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        binds[2].binding = 2;
        binds[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binds[2].descriptorCount = 1;
        binds[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        binds[3].binding = 3;
        binds[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binds[3].descriptorCount = 8;
        binds[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo lci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        lci.bindingCount = 4;
        lci.pBindings = binds;
        if (pfn_vkCreateDescriptorSetLayout(dev, &lci, nullptr, &s_dsl) != VK_SUCCESS)
        {
            error = "descriptor set layout failed";
            return false;
        }
        VkPipelineLayoutCreateInfo pl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        pl.setLayoutCount = 1;
        pl.pSetLayouts = &s_dsl;
        if (pfn_vkCreatePipelineLayout(dev, &pl, nullptr, &s_pipeLayout) != VK_SUCCESS)
        {
            error = "pipeline layout failed";
            return false;
        }
        constexpr uint32_t kDescSets = 32768; // A4d: heavy title frames can exceed 4k draws
        VkDescriptorPoolSize sizes[3] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kDescSets },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kDescSets * 2 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kDescSets * 8 },
        };
        VkDescriptorPoolCreateInfo dpi{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        dpi.maxSets = kDescSets;
        dpi.poolSizeCount = 3;
        dpi.pPoolSizes = sizes;
        if (pfn_vkCreateDescriptorPool(dev, &dpi, nullptr, &s_descPool) != VK_SUCCESS)
        {
            error = "descriptor pool failed";
            return false;
        }
        s_frameDescSets.resize(kDescSets);
        ResetFrameDescriptors();
    }

    s_active = true;
    s_deviceLost = false;
    s_frameIdx = 0;
    s_xfUploaded = false;
    s_stagingPos = 0;
    LOG_INFO("vulkan gx: A4e TEV init EFB {}x{} (scale {}) on '{}' — {} embedded SPIR-V (blit)", s_efbW, s_efbH,
             s_scale, Vk_SelectedDeviceName(), s_stats.shaderModules);
    return true;
}

void VkGx_Shutdown()
{
    std::lock_guard lk(s_mu);
    if (!s_active) return;
    Vk_DeviceWaitIdle();
    DestroyCopyUtil();
    VkDevice dev = Vk_GetDevice();
    for (auto& kv : s_pipes)
        if (kv.second) pfn_vkDestroyPipeline(dev, kv.second, nullptr);
    s_pipes.clear();
    if (s_blitPipe) pfn_vkDestroyPipeline(dev, s_blitPipe, nullptr);
    s_blitPipe = VK_NULL_HANDLE;
    for (auto& kv : s_texCache)
        DestroyImg(kv.second.img);
    s_texCache.clear();
    DestroyImg(s_whiteTex);
    DestroyImg(s_efbColor);
    DestroyImg(s_efbDepth);
    DestroyBuf(s_xfBuf);
    DestroyBuf(s_vsUbo);
    DestroyBuf(s_psUbo);
    DestroyBuf(s_dynUbo);
    DestroyBuf(s_staging);
    for (int i = 0; i < kFrames; i++)
    {
        DestroyBuf(s_frames[i].vb);
        DestroyBuf(s_frames[i].ib);
        if (s_frames[i].fence) pfn_vkDestroyFence(dev, s_frames[i].fence, nullptr);
        s_frames[i] = {};
    }
    if (s_efbFb) pfn_vkDestroyFramebuffer(dev, s_efbFb, nullptr);
    if (s_efbPass) pfn_vkDestroyRenderPass(dev, s_efbPass, nullptr);
    if (s_descPool) pfn_vkDestroyDescriptorPool(dev, s_descPool, nullptr);
    if (s_pipeLayout) pfn_vkDestroyPipelineLayout(dev, s_pipeLayout, nullptr);
    if (s_dsl) pfn_vkDestroyDescriptorSetLayout(dev, s_dsl, nullptr);
    for (auto& kv : s_vsModuleCache)
        if (kv.second) pfn_vkDestroyShaderModule(dev, kv.second, nullptr);
    s_vsModuleCache.clear();
    for (auto& kv : s_fsModuleCache)
        if (kv.second) pfn_vkDestroyShaderModule(dev, kv.second, nullptr);
    s_fsModuleCache.clear();
    if (s_blitVs) pfn_vkDestroyShaderModule(dev, s_blitVs, nullptr);
    if (s_blitFs) pfn_vkDestroyShaderModule(dev, s_blitFs, nullptr);
    for (auto& kv : s_samplerCache)
        if (kv.second) pfn_vkDestroySampler(dev, kv.second, nullptr);
    s_samplerCache.clear();
    if (s_sampNearest) pfn_vkDestroySampler(dev, s_sampNearest, nullptr);
    if (s_sampLinear) pfn_vkDestroySampler(dev, s_sampLinear, nullptr);
    if (s_pool) pfn_vkDestroyCommandPool(dev, s_pool, nullptr);
    s_efbFb = VK_NULL_HANDLE;
    s_efbPass = VK_NULL_HANDLE;
    s_descPool = VK_NULL_HANDLE;
    s_pipeLayout = VK_NULL_HANDLE;
    s_dsl = VK_NULL_HANDLE;
    s_blitVs = s_blitFs = VK_NULL_HANDLE;
    s_sampNearest = s_sampLinear = VK_NULL_HANDLE;
    s_pool = VK_NULL_HANDLE;
    s_presentCmd = VK_NULL_HANDLE;
    s_frameDescSets.clear();
    s_descCursor = 0;
    s_dynUboPos = 0;
    s_xfRingPos = 0;
    s_xfBindOffset = 0;
    s_xfUploaded = false;
    s_deviceLost = false;
    s_active = false;
}

bool VkGx_IsActive() { return s_active; }

bool VkGx_Draw(const GfxDrawBatch& batch, std::string& error)
{
    std::lock_guard lk(s_mu);
    if (!s_active || !batch.indices || batch.indexCount == 0) return true;
    if (!batch.hasShaderKeys)
    {
        VkGx_NoteUnsupported("missing-shader-keys");
        return true;
    }
    if (s_deviceLost)
    {
        error = "vulkan device previously lost";
        return false; // caller may log; latch prevents further GPU work
    }

    const VkDeviceSize vbytes = (VkDeviceSize)batch.vertCount * sizeof(HostVertex);
    const VkDeviceSize ibytes = (VkDeviceSize)batch.indexCount * sizeof(uint32_t);
    if (vbytes > kArenaBytes || ibytes > kArenaBytes)
    {
        error = "draw exceeds per-frame ring capacity";
        return false;
    }

    const uint64_t vsHash = HashKeyBytes(&batch.vsKey, sizeof(batch.vsKey));
    const uint64_t psHash = HashKeyBytes(&batch.psKey, sizeof(batch.psKey));
    VkShaderModule vsMod = VK_NULL_HANDLE, fsMod = VK_NULL_HANDLE;
    if (!GetOrCreateShaderModules(batch.vsKey, batch.psKey, vsMod, fsMod, error))
        return true; // skip draw on compile failure
    (void)vsMod;
    (void)fsMod;

    auto align = [](VkDeviceSize v, VkDeviceSize a) { return (v + a - 1) / a * a; };
    auto uploadXfSlot = [&](const GfxDrawBatch& b, std::string& err) -> bool {
        // Storage-buffer offset alignment is typically ≤256; keep slots 16 KiB-aligned.
        const VkDeviceSize off = align(s_xfRingPos, kXfSlotBytes);
        if (off + kXfSlotBytes > s_xfBuf.size)
        {
            err = "xf ring exhausted (flush required)";
            return false;
        }
        const size_t n = (size_t)std::min(b.xfFloatCount, 0x1000u) * sizeof(float);
        if (b.xfmem) memcpy((uint8_t*)s_xfBuf.mapped + off, b.xfmem, n);
        s_xfBindOffset = off;
        s_xfRingPos = off + kXfSlotBytes;
        s_xfUploaded = true;
        s_stats.uploadBytes += n;
        s_stats.xfUploads++;
        if (s_xfRingPos > s_stats.xfRingHighWater) s_stats.xfRingHighWater = s_xfRingPos;
        return true;
    };
    auto flushFrame = [&](FrameSlot& fr) {
        // A4d: do not mid-frame submit while sharing host-visible UBO/SSBO/descriptors.
        // Drain, then reuse the same frame arena from offset 0.
        if (!fr.recording) return;
        EndEfbPass(fr);
        pfn_vkEndCommandBuffer(fr.cmd);
        fr.recording = false;
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &fr.cmd;
        LoadDev<PFN_vkQueueSubmit>("vkQueueSubmit")(Vk_GetGraphicsQueue(), 1, &si, fr.fence);
        // Fence wait retires this CB; full device idle is unnecessary here.
        pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
        ResetFrameDescriptors();
        s_dynUboPos = 0;
        s_xfRingPos = 0;
        s_xfUploaded = false;
        fr.vbPos = 0;
        fr.ibPos = 0;
        ResetInlineCopyBudget();
    };

    FrameSlot* f = &s_frames[s_frameIdx];
    if (!EnsureFrameRecording(*f, error)) return false;

    VkDeviceSize vbOff = align(f->vbPos, sizeof(HostVertex));
    if (vbOff + vbytes > f->vb.size || f->ibPos + ibytes > f->ib.size)
    {
        flushFrame(*f);
        f = &s_frames[s_frameIdx];
        if (!EnsureFrameRecording(*f, error)) return false;
        vbOff = 0;
    }

    VkImageView texViews[8]{};
    VkSampler texSamps[8]{};
    for (int i = 0; i < 8; i++)
    {
        if (batch.usedMaps & (1u << i))
        {
            if (!UploadTex(batch.tex[i], texViews[i], error)) return false;
            texSamps[i] = GetOrCreateSampler(batch.tex[i]);
        }
        else
        {
            texViews[i] = s_whiteTex.view;
            texSamps[i] = s_sampNearest;
        }
    }
    // UploadTex may have ended the EFB pass / switched recording state on same frame.
    f = &s_frames[s_frameIdx];
    if (!EnsureFrameRecording(*f, error)) return false;
    vbOff = align(f->vbPos, sizeof(HostVertex));
    if (vbOff + vbytes > f->vb.size || f->ibPos + ibytes > f->ib.size)
    {
        flushFrame(*f);
        f = &s_frames[s_frameIdx];
        if (!EnsureFrameRecording(*f, error)) return false;
        vbOff = 0;
    }

    if (batch.xfDirty || !s_xfUploaded)
    {
        std::string xfErr;
        if (!uploadXfSlot(batch, xfErr))
        {
            flushFrame(*f);
            f = &s_frames[s_frameIdx];
            if (!EnsureFrameRecording(*f, error)) return false;
            vbOff = 0;
            if (!uploadXfSlot(batch, error)) return false;
        }
    }
    VkDescriptorSet drawSet = VK_NULL_HANDLE;
    if (!AllocDrawDescriptors(batch, texViews, texSamps, drawSet, error))
    {
        if (!s_allocNeedFlush) return false;
        flushFrame(*f);
        s_dynUboPos = 0;
        f = &s_frames[s_frameIdx];
        if (!EnsureFrameRecording(*f, error)) return false;
        vbOff = align(f->vbPos, sizeof(HostVertex));
        if (vbOff + vbytes > f->vb.size || f->ibPos + ibytes > f->ib.size)
        {
            error = "draw exceeds ring after flush";
            return false;
        }
        if (!uploadXfSlot(batch, error)) return false;
        if (!AllocDrawDescriptors(batch, texViews, texSamps, drawSet, error))
            return false;
    }

    memcpy((uint8_t*)f->vb.mapped + vbOff, batch.verts, (size_t)vbytes);
    memcpy((uint8_t*)f->ib.mapped + f->ibPos, batch.indices, (size_t)ibytes);
    const VkDeviceSize ibOff = f->ibPos;
    f->vbPos = vbOff + vbytes;
    f->ibPos = ibOff + ibytes;
    s_stats.uploadBytes += vbytes + ibytes;

    if (!BeginEfbPass(*f, false, nullptr, 1.0f)) return false;

    PipeKey pk{};
    pk.topo = (uint8_t)batch.topo;
    pk.cull = (uint8_t)batch.cull;
    pk.blendMode = batch.blendMode;
    pk.colorWriteMask = batch.colorWriteMask;
    pk.hasDestAlpha = (batch.pixelFormat == 1) ? 1 : 0;
    pk.depthTest = batch.depthTest ? 1 : 0;
    pk.depthWrite = batch.depthWrite ? 1 : 0;
    pk.depthCompare = batch.depthCompare;
    pk.vsHash = vsHash;
    pk.psHash = psHash;
    VkPipeline pipe = GetOrCreatePipe(pk, error);
    if (!pipe) return false;

    pfn_vkCmdBindPipeline(f->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    VkDeviceSize off = vbOff;
    pfn_vkCmdBindVertexBuffers(f->cmd, 0, 1, &f->vb.buf, &off);
    pfn_vkCmdBindIndexBuffer(f->cmd, f->ib.buf, ibOff, VK_INDEX_TYPE_UINT32);
    pfn_vkCmdBindDescriptorSets(f->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pipeLayout, 0, 1, &drawSet, 0, nullptr);

    VkViewport vp{};
    vp.x = batch.viewport.x;
    vp.y = batch.viewport.y;
    vp.width = std::max(1.0f, batch.viewport.w);
    vp.height = std::max(1.0f, batch.viewport.h);
    vp.minDepth = batch.viewport.minDepth;
    vp.maxDepth = batch.viewport.maxDepth;
    pfn_vkCmdSetViewport(f->cmd, 0, 1, &vp);
    VkRect2D sc{};
    sc.offset = { batch.scissor.x, batch.scissor.y };
    sc.extent = { (uint32_t)std::max(1, batch.scissor.w), (uint32_t)std::max(1, batch.scissor.h) };
    pfn_vkCmdSetScissor(f->cmd, 0, 1, &sc);

    pfn_vkCmdDrawIndexed(f->cmd, batch.indexCount, 1, 0, 0, 0);
    s_stats.draws++;
    s_stats.vertices += batch.vertCount;
    s_stats.indices += batch.indexCount;
    return true;
}

bool VkGx_ClearEfb(int x0, int y0, int x1, int y1, float r, float g, float b, float a, float depth,
                   bool clearColor, bool clearDepth, std::string& error)
{
    std::lock_guard lk(s_mu);
    if (!s_active) return true;
    FrameSlot& fr = s_frames[s_frameIdx];
    if (!EnsureFrameRecording(fr, error)) return false;
    if (!BeginEfbPass(fr, false, nullptr, 1.0f)) return false;

    VkClearAttachment atts[2]{};
    uint32_t n = 0;
    if (clearColor)
    {
        atts[n].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        atts[n].colorAttachment = 0;
        atts[n].clearValue.color.float32[0] = r;
        atts[n].clearValue.color.float32[1] = g;
        atts[n].clearValue.color.float32[2] = b;
        atts[n].clearValue.color.float32[3] = a;
        n++;
    }
    if (clearDepth)
    {
        atts[n].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        atts[n].clearValue.depthStencil.depth = depth;
        n++;
    }
    if (!n) return true;
    VkClearRect rect{};
    rect.rect.offset = { x0 * s_scale, y0 * s_scale };
    rect.rect.extent = { (uint32_t)std::max(0, (x1 - x0) * s_scale), (uint32_t)std::max(0, (y1 - y0) * s_scale) };
    rect.layerCount = 1;
    pfn_vkCmdClearAttachments(fr.cmd, n, atts, 1, &rect);
    return true;
}

bool VkGx_PresentXfb(int sx, int sy, int sw, int sh, std::string& error)
{
    // Hold s_mu for the entire present. The GX draw path runs on a separate thread
    // ("GX: running on a separate thread"); unlocking around acquire/submit let Draw
    // mutate the same command buffer / frame slot concurrently → RADV DEVICE_LOST.
    std::lock_guard lk(s_mu);
    if (!s_active) return true;
    if (s_deviceLost)
    {
        error = "vulkan device previously lost";
        return false;
    }
    if (!Vk_SwapchainSupportsTransferDst())
    {
        error = "swapchain lacks TRANSFER_DST (cannot blit XFB); see A4d docs";
        return false;
    }
    if (!s_presentCmd)
    {
        error = "present command buffer missing";
        return false;
    }

    FrameSlot& fr = s_frames[s_frameIdx];
    // Finish any in-progress GX draws before present (separate submit).
    if (fr.recording)
    {
        EndEfbPass(fr);
        pfn_vkEndCommandBuffer(fr.cmd);
        fr.recording = false;
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &fr.cmd;
        auto qsubmit = LoadDev<PFN_vkQueueSubmit>("vkQueueSubmit");
        VkResult sr = qsubmit(Vk_GetGraphicsQueue(), 1, &si, fr.fence);
        if (sr != VK_SUCCESS)
        {
            if (sr == VK_ERROR_DEVICE_LOST) s_deviceLost = true;
            error = "vkQueueSubmit (draw flush) failed (" + std::to_string((int)sr) + ") serial=" +
                    std::to_string(Vk_PresentSerial());
            return false;
        }
        pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
    }

    // Present uses a dedicated CB so draw-frame recording state stays untouched.
    pfn_vkResetCommandBuffer(s_presentCmd, 0);
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (pfn_vkBeginCommandBuffer(s_presentCmd, &bi) != VK_SUCCESS)
    {
        error = "present BeginCommandBuffer failed";
        return false;
    }

    Barrier(s_presentCmd, s_efbColor.img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    uint32_t imageIndex = 0;
    std::string acqErr;
    if (!Vk_AcquireImage(imageIndex, acqErr))
    {
        pfn_vkEndCommandBuffer(s_presentCmd);
        error = acqErr;
        if (acqErr.find("(-4)") != std::string::npos) s_deviceLost = true;
        return false;
    }

    VkImage swapImg = Vk_GetSwapchainImage(imageIndex);
    VkExtent2D ext = Vk_GetSwapchainExtent();
    Barrier(s_presentCmd, swapImg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            VK_ACCESS_TRANSFER_WRITE_BIT);

    VkImageBlit blit{};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[0] = { sx * s_scale, sy * s_scale, 0 };
    blit.srcOffsets[1] = { (sx + sw) * s_scale, (sy + sh) * s_scale, 1 };
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;
    const float srcAspect = (float)sw / (float)std::max(1, sh);
    const float dstAspect = (float)ext.width / (float)std::max(1u, ext.height);
    int32_t dx0 = 0, dy0 = 0, dx1 = (int32_t)ext.width, dy1 = (int32_t)ext.height;
    if (srcAspect > dstAspect)
    {
        int32_t h = (int32_t)((float)ext.width / srcAspect);
        dy0 = ((int32_t)ext.height - h) / 2;
        dy1 = dy0 + h;
    }
    else
    {
        int32_t w = (int32_t)((float)ext.height * srcAspect);
        dx0 = ((int32_t)ext.width - w) / 2;
        dx1 = dx0 + w;
    }
    blit.dstOffsets[0] = { dx0, dy0, 0 };
    blit.dstOffsets[1] = { dx1, dy1, 1 };
    auto clampi = [](int v, int lo, int hi) { return std::max(lo, std::min(v, hi)); };
    blit.srcOffsets[0].x = clampi(blit.srcOffsets[0].x, 0, (int)s_efbW);
    blit.srcOffsets[0].y = clampi(blit.srcOffsets[0].y, 0, (int)s_efbH);
    blit.srcOffsets[1].x = clampi(blit.srcOffsets[1].x, 0, (int)s_efbW);
    blit.srcOffsets[1].y = clampi(blit.srcOffsets[1].y, 0, (int)s_efbH);
    if (blit.srcOffsets[1].x <= blit.srcOffsets[0].x) blit.srcOffsets[1].x = blit.srcOffsets[0].x + 1;
    if (blit.srcOffsets[1].y <= blit.srcOffsets[0].y) blit.srcOffsets[1].y = blit.srcOffsets[0].y + 1;

    pfn_vkCmdBlitImage(s_presentCmd, s_efbColor.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapImg,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

    Barrier(s_presentCmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, 0);
    Barrier(s_presentCmd, s_efbColor.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    pfn_vkEndCommandBuffer(s_presentCmd);

    std::string subErr;
    if (!Vk_SubmitPresent(s_presentCmd, imageIndex, subErr, VK_PIPELINE_STAGE_TRANSFER_BIT))
    {
        error = subErr;
        if (subErr.find("(-4)") != std::string::npos) s_deviceLost = true;
        return false;
    }
    // Retire present blit via its submission fence (not full device idle). Draw CBs were
    // already fence-waited above; descriptor/dynUbo/staging were only used by those CBs.
    Vk_WaitLastSubmit();
    RetireEfbCopyEntries();
    ResetFrameDescriptors();
    s_dynUboPos = 0;
    s_xfRingPos = 0;
    s_xfUploaded = false;
    s_stagingPos = 0;
    ResetInlineCopyBudget();
    // Present did not use fr.fence; keep it signaled for EnsureFrameRecording.
    if (!fr.recording)
    {
        pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
        // If unsignaled somehow, wait may hang — fence stays signaled from draw flush or init.
    }
    s_frameIdx = (s_frameIdx + 1) % kFrames;
    return true;
}

bool VkGx_SaveEfbBmp(int sx, int sy, int sw, int sh, const char* path, std::string& error)
{
    std::lock_guard lk(s_mu);
    if (!s_active || !path || !path[0])
    {
        error = "VkGx_SaveEfbBmp: inactive or empty path";
        return false;
    }
    const int x0 = sx * s_scale, y0 = sy * s_scale;
    const int w = std::max(1, sw * s_scale), h = std::max(1, sh * s_scale);
    if (x0 < 0 || y0 < 0 || x0 + w > (int)s_efbW || y0 + h > (int)s_efbH)
    {
        error = "VkGx_SaveEfbBmp: region out of EFB";
        return false;
    }
    const VkDeviceSize rowPitch = ((VkDeviceSize)w * 4 + 255) & ~VkDeviceSize(255);
    const VkDeviceSize bytes = rowPitch * (VkDeviceSize)h;
    Buf staging{};
    if (!CreateBuf(staging, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        error = "VkGx_SaveEfbBmp: staging alloc failed";
        return false;
    }
    FrameSlot& fr = s_frames[s_frameIdx];
    if (fr.recording)
    {
        EndEfbPass(fr);
        pfn_vkEndCommandBuffer(fr.cmd);
        fr.recording = false;
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &fr.cmd;
        LoadDev<PFN_vkQueueSubmit>("vkQueueSubmit")(Vk_GetGraphicsQueue(), 1, &si, fr.fence);
        pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
    }
    else
        pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);

    pfn_vkResetFences(Vk_GetDevice(), 1, &fr.fence);
    pfn_vkResetCommandBuffer(fr.cmd, 0);
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    pfn_vkBeginCommandBuffer(fr.cmd, &bi);
    Barrier(fr.cmd, s_efbColor.img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy region{};
    region.bufferRowLength = (uint32_t)(rowPitch / 4);
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { x0, y0, 0 };
    region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };
    pfn_vkCmdCopyImageToBuffer(fr.cmd, s_efbColor.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buf, 1, &region);
    Barrier(fr.cmd, s_efbColor.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    pfn_vkEndCommandBuffer(fr.cmd);
    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers = &fr.cmd;
    LoadDev<PFN_vkQueueSubmit>("vkQueueSubmit")(Vk_GetGraphicsQueue(), 1, &si, fr.fence);
    pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);

    // Write 24-bit BMP (BGR). EFB is B8G8R8A8_UNORM — byte order B,G,R,A on little-endian.
    const int rowBytes = ((w * 3 + 3) & ~3);
    std::vector<uint8_t> bmp(54 + (size_t)rowBytes * h);
    auto* hdr = bmp.data();
    hdr[0] = 'B';
    hdr[1] = 'M';
    const uint32_t fileSize = (uint32_t)bmp.size();
    memcpy(hdr + 2, &fileSize, 4);
    const uint32_t offBits = 54;
    memcpy(hdr + 10, &offBits, 4);
    const uint32_t dib = 40;
    memcpy(hdr + 14, &dib, 4);
    memcpy(hdr + 18, &w, 4);
    const int32_t hm = -h; // top-down
    memcpy(hdr + 22, &hm, 4);
    const uint16_t planes = 1, bpp = 24;
    memcpy(hdr + 26, &planes, 2);
    memcpy(hdr + 28, &bpp, 2);
    for (int y = 0; y < h; y++)
    {
        const uint8_t* src = (const uint8_t*)staging.mapped + (size_t)y * rowPitch;
        uint8_t* dst = bmp.data() + 54 + (size_t)y * rowBytes;
        for (int x = 0; x < w; x++)
        {
            dst[x * 3 + 0] = src[x * 4 + 0]; // B
            dst[x * 3 + 1] = src[x * 4 + 1]; // G
            dst[x * 3 + 2] = src[x * 4 + 2]; // R
        }
    }
    DestroyBuf(staging);
    FILE* f = fopen(path, "wb");
    if (!f)
    {
        error = "fopen failed";
        return false;
    }
    fwrite(bmp.data(), 1, bmp.size(), f);
    fclose(f);
    return true;
}

VkGxStats VkGx_Stats()
{
    VkGxStats st = s_stats;
    const VkSpvCompileStats spv = VkSpv_Stats();
    st.spvCompileOk = spv.compileOk;
    st.spvCompileFail = spv.compileFail;
    // Disk SPIR-V cache hits contribute to reported shader cache hits.
    st.shaderCacheHits += spv.cacheHits;
    st.shaderCacheMisses += spv.cacheMisses;
    return st;
}
void VkGx_ResetStats()
{
    s_stats = {};
    VkSpv_ResetStats();
}

void VkGx_NoteUnsupported(const char* op)
{
    if (!op) return;
    auto& c = s_unsupCounts[op];
    if (c < 8)
        LOG_WARN("vulkan gx: unsupported op '{}' (count {})", op, c + 1);
    c++;
    s_stats.unsupportedOps++;
}

// Record region snap + format convert into an already-begun command buffer.
// Caller owns EFB-pass state (must EndEfbPass first) and submission/retirement.
static void RecordEfbCopyCommands(FrameSlot& fr, const GfxEfbCopyRequest& req, EfbCopyCacheEntry& entry,
                                  uint32_t rw, uint32_t rh)
{
    const float u0 = (float)req.sx / (float)kEfbW, v0 = (float)req.sy / (float)kEfbH;
    const float u1 = (float)(req.sx + req.sw) / (float)kEfbW, v1 = (float)(req.sy + req.sh) / (float)kEfbH;
    const int32_t rx0 = (int32_t)req.sx * s_scale, ry0 = (int32_t)req.sy * s_scale;
    const int32_t rx1 = (int32_t)(req.sx + req.sw) * s_scale, ry1 = (int32_t)(req.sy + req.sh) * s_scale;

    Barrier(fr.cmd, s_efbColor.img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    Barrier(fr.cmd, s_efbColorSnap.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageBlit cblit{};
    cblit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cblit.srcSubresource.layerCount = 1;
    cblit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cblit.dstSubresource.layerCount = 1;
    cblit.srcOffsets[0] = { rx0, ry0, 0 };
    cblit.srcOffsets[1] = { rx1, ry1, 1 };
    cblit.dstOffsets[0] = { rx0, ry0, 0 };
    cblit.dstOffsets[1] = { rx1, ry1, 1 };
    pfn_vkCmdBlitImage(fr.cmd, s_efbColor.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s_efbColorSnap.img,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cblit, VK_FILTER_NEAREST);

    Barrier(fr.cmd, s_efbDepth.img, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    Barrier(fr.cmd, s_efbDepthSnap.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy dc{};
    dc.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    dc.srcSubresource.layerCount = 1;
    dc.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    dc.dstSubresource.layerCount = 1;
    dc.srcOffset = { rx0, ry0, 0 };
    dc.dstOffset = { rx0, ry0, 0 };
    dc.extent = { (uint32_t)std::max(1, rx1 - rx0), (uint32_t)std::max(1, ry1 - ry0), 1 };
    pfn_vkCmdCopyImage(fr.cmd, s_efbDepth.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s_efbDepthSnap.img,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &dc);

    Barrier(fr.cmd, s_efbColorSnap.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    Barrier(fr.cmd, s_efbDepthSnap.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    Barrier(fr.cmd, s_efbColor.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    Barrier(fr.cmd, s_efbDepth.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    const VkImageLayout destOld =
        entry.genId ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    const VkPipelineStageFlags destSrcSt =
        entry.genId ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    const VkAccessFlags destSrcA = entry.genId ? VK_ACCESS_SHADER_READ_BIT : 0;
    Barrier(fr.cmd, entry.img.img, destOld, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
            destSrcSt, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, destSrcA,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    CopyUtilUbo pc{};
    // Vulkan NDC Y+ is down; D3D util uses {-1,1,1,-1} (Y+ up). Match D3D texture
    // orientation so EFB-sampled meshes are not vertically flipped vs the HUD.
    pc.rect[0] = -1.0f;
    pc.rect[1] = -1.0f;
    pc.rect[2] = 1.0f;
    pc.rect[3] = 1.0f;
    pc.uv[0] = u0;
    pc.uv[1] = v0;
    pc.uv[2] = u1;
    pc.uv[3] = v1;
    pc.params[2] = 1.0f / (float)kEfbW;
    pc.params[3] = 1.0f / (float)kEfbH;
    pc.iparams[0] = (int32_t)req.fmt;
    pc.iparams[1] = (req.intensity && req.fmt < 4) ? 1 : 0;
    pc.iparams[2] = req.depthSrc ? 1 : 0;
    pc.iparams[3] = req.half ? 1 : 0;

    VkRenderPassBeginInfo rp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rp.renderPass = s_copyPass;
    rp.framebuffer = entry.fb;
    rp.renderArea.extent = { rw, rh };
    pfn_vkCmdBeginRenderPass(fr.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    pfn_vkCmdBindPipeline(fr.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_copyPipe);
    pfn_vkCmdBindDescriptorSets(fr.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_copyLayout, 0, 1, &s_copySet, 0, nullptr);
    pfn_vkCmdPushConstants(fr.cmd, s_copyLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(pc), &pc);
    VkViewport vp{ 0, 0, (float)rw, (float)rh, 0, 1 };
    pfn_vkCmdSetViewport(fr.cmd, 0, 1, &vp);
    VkRect2D sc{ { 0, 0 }, { rw, rh } };
    pfn_vkCmdSetScissor(fr.cmd, 0, 1, &sc);
    pfn_vkCmdDraw(fr.cmd, 4, 1, 0, 0);
    pfn_vkCmdEndRenderPass(fr.cmd);

    Barrier(fr.cmd, entry.img.img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
}

bool VkGx_CopyEfbToTexture(const GfxEfbCopyRequest& req, std::string& error)
{
    std::lock_guard lk(s_mu);
    if (!s_active)
    {
        error = "VkGx inactive";
        return false;
    }
    if (s_deviceLost)
    {
        error = "vulkan device previously lost";
        return false;
    }
    if (req.sw <= 0 || req.sh <= 0 || !req.destAddr)
    {
        error = "invalid efb copy request";
        return false;
    }

    const uint32_t tw = req.half ? (uint32_t)std::max(1, req.sw / 2) : (uint32_t)req.sw;
    const uint32_t th = req.half ? (uint32_t)std::max(1, req.sh / 2) : (uint32_t)req.sh;
    const uint32_t rw = tw * (uint32_t)s_scale, rh = th * (uint32_t)s_scale;
    const uint32_t key = EfbCopyKey(req.destAddr);
    const uint64_t regionPixels =
        (uint64_t)std::max(1, req.sw * s_scale) * (uint64_t)std::max(1, req.sh * s_scale);

    static int s_forceSync = -1;
    static int s_prof = -1;
    if (s_forceSync < 0)
    {
        s_forceSync = EnvFlag("SCR_A4L_EFB_SYNC");
        s_prof = EnvFlag("SCR_A4L_EFB_PROF");
        if (s_forceSync)
            LOG_INFO("A4l: SCR_A4L_EFB_SYNC=1 — per-copy fence wait (A4f/A4k baseline)");
        else
            LOG_INFO("A4l: GPU-ordered inline EFB copies (budget {} copies / {} Mpx); "
                     "SCR_A4L_EFB_SYNC=1 restores sync path",
                     kMaxInlineCopiesPerCb, (unsigned)(kMaxInlineCopyPixelsPerCb / (1024ull * 1024ull)));
        if (s_prof) LOG_INFO("A4l: SCR_A4L_EFB_PROF=1 — per-copy timing in vulkan gx stats");
    }

    FrameSlot& fr = s_frames[s_frameIdx];
    const uint64_t tCopy0 = MonoNs();

    auto finishMeta = [&](EfbCopyCacheEntry& entry, bool synced) {
        entry.genId = s_efbCopyGen++;
        // Fingerprint guest RAM so a later CPU texture at the same EA invalidates this copy
        // (D3D TexEntry::ramHash / ramSize parity — A4i sky/background).
        static const uint32_t bppTable[16] = { 4, 8, 8, 16, 16, 16, 32, 8, 8, 8, 8, 16, 16, 16, 16, 16 };
        entry.ramSize = std::max<uint32_t>(32u, tw * th * bppTable[req.fmt & 15u] / 8u);
        entry.ramHash = Hash64Bytes(PhysPtr(req.destAddr), entry.ramSize);
        entry.hashedFrame = req.gxFrame;
        s_stats.efbCopies++;
        if (synced) s_stats.efbCopySynced++;
        else s_stats.efbCopyInline++;
        s_stats.efbCopyBytes += (uint64_t)rw * rh * 4u;
        s_inlineCopiesSinceSubmit++;
        s_inlineCopyPixelsSinceSubmit += regionPixels;
    };

    // --- Legacy sync path (A4f/A4k): drain, dedicated CB, CPU wait every copy. ---
    if (s_forceSync)
    {
        if (!DrainFrameSlot(fr, error, true)) return false;
        const uint64_t tw0 = MonoNs();
        pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
        s_stats.efbCopyFenceWaits++;
        s_stats.efbCopyFenceWaitNs += MonoNs() - tw0;
        pfn_vkResetFences(Vk_GetDevice(), 1, &fr.fence);
        pfn_vkResetCommandBuffer(fr.cmd, 0);
        VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (pfn_vkBeginCommandBuffer(fr.cmd, &bi) != VK_SUCCESS)
        {
            error = "efb-copy BeginCommandBuffer failed";
            return false;
        }
        fr.recording = true;
        fr.inEfbPass = false;

        EfbCopyCacheEntry& entry = s_efbCopyCache[key];
        if (!EnsureEfbCopyDest(entry, rw, rh, tw, th))
        {
            error = "efb-copy dest alloc failed";
            return false;
        }
        const uint64_t tr0 = MonoNs();
        RecordEfbCopyCommands(fr, req, entry, rw, rh);
        s_stats.efbCopyRecordNs += MonoNs() - tr0;

        pfn_vkEndCommandBuffer(fr.cmd);
        fr.recording = false;
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &fr.cmd;
        const uint64_t ts0 = MonoNs();
        VkResult sr = LoadDev<PFN_vkQueueSubmit>("vkQueueSubmit")(Vk_GetGraphicsQueue(), 1, &si, fr.fence);
        s_stats.efbCopySubmitNs += MonoNs() - ts0;
        if (sr != VK_SUCCESS)
        {
            if (sr == VK_ERROR_DEVICE_LOST) s_deviceLost = true;
            error = "efb-copy submit failed";
            return false;
        }
        const uint64_t tw1 = MonoNs();
        pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
        s_stats.efbCopyFenceWaits++;
        s_stats.efbCopyFenceWaitNs += MonoNs() - tw1;
        finishMeta(entry, true);
        ResetInlineCopyBudget();
        return true;
    }

    // --- A4l default: GPU-ordered inline copy in the live draw CB (no per-copy wait). ---
    // A4h TDR came from unbounded full-EFB blits in one CB; we keep region snaps and a
    // soft-flush budget (submit+wait once when count/pixels trip) instead of waiting every copy.
    if (fr.recording && (s_inlineCopiesSinceSubmit >= kMaxInlineCopiesPerCb ||
                         s_inlineCopyPixelsSinceSubmit + regionPixels > kMaxInlineCopyPixelsPerCb ||
                         s_efbCopyRetire.size() >= 48))
    {
        if (!SoftSubmitContinue(fr, error, true)) return false;
    }
    else if (!fr.recording)
        ResetInlineCopyBudget();

    if (!EnsureFrameRecording(fr, error)) return false;
    EndEfbPass(fr);

    EfbCopyCacheEntry& entry = s_efbCopyCache[key];
    // Size churn defers old image destroy to RetireEfbCopyEntries (after GPU wait) —
    // no mid-copy SoftSubmitContinue solely for realloc.
    if (!EnsureEfbCopyDest(entry, rw, rh, tw, th))
    {
        error = "efb-copy dest alloc failed";
        return false;
    }
    const uint64_t tr1 = MonoNs();
    RecordEfbCopyCommands(fr, req, entry, rw, rh);
    s_stats.efbCopyRecordNs += MonoNs() - tr1;
    finishMeta(entry, false);

    if (s_prof && (s_stats.efbCopies % 2048ull) == 1ull)
    {
        LOG_INFO("A4l-efb: copies={} inline={} sync={} softFlush={} fenceWaits={} "
                 "fenceMs={:.1f} recordMs={:.1f} submitMs={:.1f} budget={}/{}px last={}x{} fmt={}",
                 s_stats.efbCopies, s_stats.efbCopyInline, s_stats.efbCopySynced, s_stats.efbCopySoftFlushes,
                 s_stats.efbCopyFenceWaits, s_stats.efbCopyFenceWaitNs / 1e6, s_stats.efbCopyRecordNs / 1e6,
                 s_stats.efbCopySubmitNs / 1e6, s_inlineCopiesSinceSubmit, s_inlineCopyPixelsSinceSubmit, req.sw,
                 req.sh, req.fmt);
    }
    (void)tCopy0;
    return true;
}

bool VkGx_LookupEfbCopy(uint32_t destAddr, VkImageView& outView, uint32_t& outW, uint32_t& outH,
                        uint32_t gxFrame)
{
    std::lock_guard lk(s_mu);
    return LookupEfbCopyLocked(destAddr, outView, outW, outH, gxFrame);
}

void VkGx_InvalidateEfbCopy(uint32_t destAddr)
{
    std::lock_guard lk(s_mu);
    const uint32_t key = EfbCopyKey(destAddr);
    if (!key) return;
    auto it = s_efbCopyCache.find(key);
    if (it == s_efbCopyCache.end()) return;
    FrameSlot& fr = s_frames[s_frameIdx];
    if (fr.recording)
    {
        std::string err;
        DrainFrameSlot(fr, err);
    }
    else
        pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
    DestroyEfbCopyEntry(it->second);
    s_efbCopyCache.erase(it);
}
