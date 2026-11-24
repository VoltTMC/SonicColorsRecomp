// A4c Stage B: GX Flush → Vulkan DrawIndexed (real HostVertex geometry).
#include "vk_gx.h"
#include "vk_device.h"
#include "vk_spv_embed.h"

#include "../../log.h"
#include "../vertex_loader.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
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
L(vkCmdPipelineBarrier); L(vkCmdCopyBuffer); L(vkCmdCopyBufferToImage); L(vkCmdBlitImage);
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
    LD(vkCmdPipelineBarrier); LD(vkCmdCopyBuffer); LD(vkCmdCopyBufferToImage); LD(vkCmdBlitImage);
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
    uint8_t topo = 0, cull = 0, blend = 0, depthWrite = 1, depthTest = 1;
    bool operator==(const PipeKey& o) const
    {
        return topo == o.topo && cull == o.cull && blend == o.blend && depthWrite == o.depthWrite &&
               depthTest == o.depthTest;
    }
};
struct PipeKeyHash
{
    size_t operator()(const PipeKey& k) const
    {
        return (size_t)k.topo | ((size_t)k.cull << 8) | ((size_t)k.blend << 16) |
               ((size_t)k.depthWrite << 24) | ((size_t)k.depthTest << 25);
    }
};

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

VkShaderModule s_vs = VK_NULL_HANDLE, s_fs = VK_NULL_HANDLE;
VkShaderModule s_blitVs = VK_NULL_HANDLE, s_blitFs = VK_NULL_HANDLE;
VkDescriptorSetLayout s_dsl = VK_NULL_HANDLE;
VkPipelineLayout s_pipeLayout = VK_NULL_HANDLE;
VkDescriptorPool s_descPool = VK_NULL_HANDLE;
std::vector<VkDescriptorSet> s_frameDescSets;
uint32_t s_descCursor = 0;

Buf s_xfBuf{}, s_vsUbo{}, s_psUbo{}, s_dynUbo{};
VkDeviceSize s_dynUboPos = 0;
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

std::unordered_map<std::string, uint32_t> s_unsupCounts;

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
                        VkImageAspectFlags aspect)
{
    VkDevice dev = Vk_GetDevice();
    VkImageCreateInfo ci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = fmt;
    ci.extent = { w, h, 1 };
    ci.mipLevels = 1;
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
    vi.subresourceRange.levelCount = 1;
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

static void Barrier(VkCommandBuffer cmd, VkImage img, VkImageLayout oldL, VkImageLayout newL,
                    VkImageAspectFlags aspect, VkPipelineStageFlags srcSt, VkPipelineStageFlags dstSt,
                    VkAccessFlags srcA, VkAccessFlags dstA)
{
    VkImageMemoryBarrier b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.oldLayout = oldL;
    b.newLayout = newL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = aspect;
    b.subresourceRange.levelCount = 1;
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
    // GX: cull 1 = back, 2 = front; screen-space front faces are clockwise (match D3D11).
    rs.cullMode = key.cull == 1   ? VK_CULL_MODE_FRONT_BIT
                  : key.cull == 2 ? VK_CULL_MODE_BACK_BIT
                                  : VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds.depthTestEnable = key.depthTest ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = key.depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = 0xF;
    if (key.blend)
    {
        ba.blendEnable = VK_TRUE;
        ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ba.colorBlendOp = VK_BLEND_OP_ADD;
        ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        ba.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;

    VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dy{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = s_vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = s_fs;
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
    LOG_INFO("vulkan gx: created pipeline topo={} cull={} blend={} depth={}/{}", (int)key.topo, (int)key.cull,
             (int)key.blend, (int)key.depthTest, (int)key.depthWrite);
    return pipe;
}

static bool UploadTex(const GfxTexBind& t, VkImageView& outView, std::string& error)
{
    if (!t.use || !t.rgba || t.w == 0 || t.h == 0)
    {
        outView = s_whiteTex.view;
        return true;
    }
    auto it = s_texCache.find(t.key);
    if (it != s_texCache.end() && it->second.img.view)
    {
        outView = it->second.img.view;
        return true;
    }
    FrameSlot& fr = s_frames[s_frameIdx];
    if (!EnsureFrameRecording(fr, error)) return false;
    EndEfbPass(fr);

    const VkDeviceSize bytes = (VkDeviceSize)t.w * t.h * 4;
    if (bytes > s_staging.size)
    {
        error = "texture larger than staging buffer";
        return false;
    }
    // A4d: per-upload staging slices — overwriting offset 0 while prior vkCmdCopyBufferToImage
    // commands in the same CB still reference it is illegal and can DEVICE_LOST on RADV.
    auto align4 = [](VkDeviceSize v) { return (v + 3) & ~VkDeviceSize(3); };
    VkDeviceSize off = align4(s_stagingPos);
    if (off + bytes > s_staging.size)
    {
        // Drain recorded copies before wrapping the staging ring.
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
        s_stagingPos = 0;
        off = 0;
        if (!EnsureFrameRecording(fr, error)) return false;
    }
    memcpy((uint8_t*)s_staging.mapped + off, t.rgba, (size_t)bytes);
    s_stagingPos = off + bytes;

    TexCacheEntry e{};
    e.key = t.key;
    e.w = t.w;
    e.h = t.h;
    if (!CreateImg2D(e.img, t.w, t.h, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT))
    {
        error = "CreateImg2D tex failed";
        return false;
    }
    Barrier(fr.cmd, e.img.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy region{};
    region.bufferOffset = off;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { t.w, t.h, 1 };
    pfn_vkCmdCopyBufferToImage(fr.cmd, s_staging.buf, e.img.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    Barrier(fr.cmd, e.img.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    s_texCache[t.key] = std::move(e);
    outView = s_texCache[t.key].img.view;
    s_stats.texUploads++;
    s_stats.uploadBytes += bytes;
    return true;
}

static void ResetFrameDescriptors();

// A4d: never update a descriptor set after it has been bound in the current CB
// (Vulkan forbids that without UPDATE_AFTER_BIND). Allocate a fresh set per draw.
static bool s_allocNeedFlush = false;

static bool AllocDrawDescriptors(VkImageView texView, uint32_t useTex, const VSConstants& vsConst,
                                   VkDescriptorSet& outSet, std::string& error)
{
    // Never reset the pool while the current CB still references allocated sets.
    // Exhaustion → signal caller to flushFrame (submit + wait + reset), then retry.
    s_allocNeedFlush = false;
    // Both VS and PS UBO offsets must respect minUniformBufferOffsetAlignment (NVIDIA: 64).
    constexpr VkDeviceSize kUboAlign = 256;
    auto alignUbo = [](VkDeviceSize v) { return (v + kUboAlign - 1) / kUboAlign * kUboAlign; };
    const VkDeviceSize vsOff = alignUbo(s_dynUboPos);
    const VkDeviceSize psOff = alignUbo(vsOff + sizeof(VSConstants));
    const VkDeviceSize slotEnd = alignUbo(psOff + 16);
    if (s_descCursor >= s_frameDescSets.size() || slotEnd > s_dynUbo.size)
    {
        s_allocNeedFlush = true;
        error = "per-frame descriptor/dynUbo exhausted (flush required)";
        return false;
    }
    memcpy((uint8_t*)s_dynUbo.mapped + vsOff, &vsConst, sizeof(VSConstants));
    uint32_t flags[4] = { useTex, 0, 0, 0 };
    memcpy((uint8_t*)s_dynUbo.mapped + psOff, flags, sizeof(flags));
    s_dynUboPos = slotEnd;

    VkDescriptorSet set = s_frameDescSets[s_descCursor++];

    VkDescriptorBufferInfo xf{};
    xf.buffer = s_xfBuf.buf;
    xf.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo vs{};
    vs.buffer = s_dynUbo.buf;
    vs.offset = vsOff;
    vs.range = sizeof(VSConstants);
    VkDescriptorImageInfo img{};
    img.sampler = s_sampLinear;
    img.imageView = texView;
    img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorBufferInfo ps{};
    ps.buffer = s_dynUbo.buf;
    ps.offset = psOff;
    ps.range = 16;

    VkWriteDescriptorSet writes[4]{};
    for (int i = 0; i < 4; i++)
    {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = (uint32_t)i;
        writes[i].descriptorCount = 1;
    }
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &xf;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &vs;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &img;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[3].pBufferInfo = &ps;
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
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
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

    if (!CreateBuf(s_xfBuf, 0x1000 * sizeof(float),
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

    s_vs = MakeModule(k_gx_vert_spv, k_gx_vert_spv_words);
    s_fs = MakeModule(k_gx_frag_spv, k_gx_frag_spv_words);
    s_blitVs = MakeModule(k_blit_vert_spv, k_blit_vert_spv_words);
    s_blitFs = MakeModule(k_blit_frag_spv, k_blit_frag_spv_words);
    if (!s_vs || !s_fs || !s_blitVs || !s_blitFs)
    {
        error = "shader module creation failed";
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
        binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binds[2].descriptorCount = 1;
        binds[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        binds[3].binding = 3;
        binds[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binds[3].descriptorCount = 1;
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
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kDescSets },
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
    LOG_INFO("vulkan gx: Stage B init EFB {}x{} (scale {}) on '{}' — {} SPIR-V modules", s_efbW, s_efbH, s_scale,
             Vk_SelectedDeviceName(), s_stats.shaderModules);
    return true;
}

void VkGx_Shutdown()
{
    std::lock_guard lk(s_mu);
    if (!s_active) return;
    Vk_DeviceWaitIdle();
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
    if (s_vs) pfn_vkDestroyShaderModule(dev, s_vs, nullptr);
    if (s_fs) pfn_vkDestroyShaderModule(dev, s_fs, nullptr);
    if (s_blitVs) pfn_vkDestroyShaderModule(dev, s_blitVs, nullptr);
    if (s_blitFs) pfn_vkDestroyShaderModule(dev, s_blitFs, nullptr);
    if (s_sampNearest) pfn_vkDestroySampler(dev, s_sampNearest, nullptr);
    if (s_sampLinear) pfn_vkDestroySampler(dev, s_sampLinear, nullptr);
    if (s_pool) pfn_vkDestroyCommandPool(dev, s_pool, nullptr);
    s_efbFb = VK_NULL_HANDLE;
    s_efbPass = VK_NULL_HANDLE;
    s_descPool = VK_NULL_HANDLE;
    s_pipeLayout = VK_NULL_HANDLE;
    s_dsl = VK_NULL_HANDLE;
    s_vs = s_fs = s_blitVs = s_blitFs = VK_NULL_HANDLE;
    s_sampNearest = s_sampLinear = VK_NULL_HANDLE;
    s_pool = VK_NULL_HANDLE;
    s_presentCmd = VK_NULL_HANDLE;
    s_frameDescSets.clear();
    s_descCursor = 0;
    s_dynUboPos = 0;
    s_deviceLost = false;
    s_active = false;
}

bool VkGx_IsActive() { return s_active; }

bool VkGx_Draw(const GfxDrawBatch& batch, std::string& error)
{
    std::lock_guard lk(s_mu);
    if (!s_active || !batch.indices || batch.indexCount == 0) return true;
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

    auto align = [](VkDeviceSize v, VkDeviceSize a) { return (v + a - 1) / a * a; };
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
        fr.vbPos = 0;
        fr.ibPos = 0;
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

    VkImageView texView = s_whiteTex.view;
    if (!UploadTex(batch.tex0, texView, error)) return false;
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
        const size_t n = (size_t)std::min(batch.xfFloatCount, 0x1000u) * sizeof(float);
        if (batch.xfmem) memcpy(s_xfBuf.mapped, batch.xfmem, n);
        s_xfUploaded = true;
        s_stats.uploadBytes += n;
    }
    VkDescriptorSet drawSet = VK_NULL_HANDLE;
    if (!AllocDrawDescriptors(texView, batch.tex0.use ? 1u : 0u, batch.vs, drawSet, error))
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
        if (!AllocDrawDescriptors(texView, batch.tex0.use ? 1u : 0u, batch.vs, drawSet, error))
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
    pk.blend = batch.blendEnable ? 1 : 0;
    pk.depthTest = batch.depthTest ? 1 : 0;
    pk.depthWrite = batch.depthWrite ? 1 : 0;
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
    ResetFrameDescriptors();
    s_dynUboPos = 0;
    s_stagingPos = 0;
    // Present did not use fr.fence; keep it signaled for EnsureFrameRecording.
    if (!fr.recording)
    {
        pfn_vkWaitForFences(Vk_GetDevice(), 1, &fr.fence, VK_TRUE, UINT64_MAX);
        // If unsignaled somehow, wait may hang — fence stays signaled from draw flush or init.
    }
    s_frameIdx = (s_frameIdx + 1) % kFrames;
    return true;
}

VkGxStats VkGx_Stats() { return s_stats; }
void VkGx_ResetStats() { s_stats = {}; }

void VkGx_NoteUnsupported(const char* op)
{
    if (!op) return;
    auto& c = s_unsupCounts[op];
    if (c < 8)
        LOG_WARN("vulkan gx: unsupported op '{}' (count {})", op, c + 1);
    c++;
    s_stats.unsupportedOps++;
}
