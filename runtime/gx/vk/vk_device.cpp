#include "vk_device.h"

#include "../../log.h"
#include "../../platform/window.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

#if defined(_WIN32)
#  define VK_USE_PLATFORM_WIN32_KHR
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <SDL3/SDL.h>
#  include <SDL3/SDL_vulkan.h>
#endif

#include <vulkan/vulkan.h>

namespace
{

std::mutex s_mu;
void* s_lib = nullptr;
VkInstance s_instance = VK_NULL_HANDLE;
VkPhysicalDevice s_phys = VK_NULL_HANDLE;
VkDevice s_device = VK_NULL_HANDLE;
VkQueue s_queue = VK_NULL_HANDLE;
uint32_t s_queueFamily = 0;
VkSurfaceKHR s_surface = VK_NULL_HANDLE;
VkSwapchainKHR s_swapchain = VK_NULL_HANDLE;
VkFormat s_swapFormat = VK_FORMAT_B8G8R8A8_UNORM;
VkColorSpaceKHR s_swapColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkExtent2D s_extent{ 0, 0 };
std::vector<VkImage> s_images;
std::vector<VkImageView> s_views;
std::vector<VkFramebuffer> s_framebuffers;
VkRenderPass s_renderPass = VK_NULL_HANDLE;
VkCommandPool s_cmdPool = VK_NULL_HANDLE;
VkCommandBuffer s_cmd = VK_NULL_HANDLE;
// A4d: per-frame sync (binary semaphores must not be reused until the wait completes).
constexpr uint32_t kSyncFrames = 3;
VkSemaphore s_imageAvailable[kSyncFrames]{};
// A4d: binary semaphores waited by present must be per swapchain image.
std::vector<VkSemaphore> s_renderFinished;
VkFence s_inFlight[kSyncFrames]{};
uint32_t s_syncFrame = 0;
uint64_t s_presentSerial = 0;
VkDeviceInfo s_selected{};
uint32_t s_imageCount = 0;
uint32_t s_preferredImageCount = 3;
bool s_vsync = false;
bool s_allowTearing = true;
bool s_swapTransferDst = false;
bool s_validationWanted = false;
bool s_validationEnabled = false;
VkDebugUtilsMessengerEXT s_debugMessenger = VK_NULL_HANDLE;
PFN_vkCreateDebugUtilsMessengerEXT pfn_vkCreateDebugUtilsMessengerEXT = nullptr;
PFN_vkDestroyDebugUtilsMessengerEXT pfn_vkDestroyDebugUtilsMessengerEXT = nullptr;
#if defined(_WIN32)
HWND s_hwnd = nullptr;
#else
SDL_Window* s_sdlWindow = nullptr;
#endif

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr_ = nullptr;
PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties_unused = nullptr;

#define VK_LOAD_I(name) PFN_##name pfn_##name = nullptr
#define VK_LOAD_D(name) PFN_##name pfn_##name = nullptr

VK_LOAD_I(vkCreateInstance);
VK_LOAD_I(vkDestroyInstance);
VK_LOAD_I(vkEnumeratePhysicalDevices);
VK_LOAD_I(vkGetPhysicalDeviceProperties);
VK_LOAD_I(vkGetPhysicalDeviceQueueFamilyProperties);
VK_LOAD_I(vkEnumerateDeviceExtensionProperties);
VK_LOAD_I(vkGetPhysicalDeviceSurfaceSupportKHR);
VK_LOAD_I(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
VK_LOAD_I(vkGetPhysicalDeviceSurfaceFormatsKHR);
VK_LOAD_I(vkGetPhysicalDeviceSurfacePresentModesKHR);
VK_LOAD_I(vkCreateDevice);
VK_LOAD_I(vkDestroyDevice);
VK_LOAD_I(vkGetDeviceQueue);
VK_LOAD_I(vkDestroySurfaceKHR);
VK_LOAD_D(vkCreateSwapchainKHR);
VK_LOAD_D(vkDestroySwapchainKHR);
VK_LOAD_D(vkGetSwapchainImagesKHR);
VK_LOAD_D(vkCreateImageView);
VK_LOAD_D(vkDestroyImageView);
VK_LOAD_D(vkCreateRenderPass);
VK_LOAD_D(vkDestroyRenderPass);
VK_LOAD_D(vkCreateFramebuffer);
VK_LOAD_D(vkDestroyFramebuffer);
VK_LOAD_D(vkCreateCommandPool);
VK_LOAD_D(vkDestroyCommandPool);
VK_LOAD_D(vkAllocateCommandBuffers);
VK_LOAD_D(vkBeginCommandBuffer);
VK_LOAD_D(vkEndCommandBuffer);
VK_LOAD_D(vkCmdBeginRenderPass);
VK_LOAD_D(vkCmdEndRenderPass);
VK_LOAD_D(vkCreateSemaphore);
VK_LOAD_D(vkDestroySemaphore);
VK_LOAD_D(vkCreateFence);
VK_LOAD_D(vkDestroyFence);
VK_LOAD_D(vkWaitForFences);
VK_LOAD_D(vkResetFences);
VK_LOAD_D(vkAcquireNextImageKHR);
VK_LOAD_D(vkQueueSubmit);
VK_LOAD_D(vkQueuePresentKHR);
VK_LOAD_D(vkDeviceWaitIdle);
VK_LOAD_D(vkResetCommandBuffer);

#if defined(_WIN32)
VK_LOAD_I(vkCreateWin32SurfaceKHR);
#endif

template <typename T>
static T LoadInst(VkInstance inst, const char* name)
{
    return reinterpret_cast<T>(vkGetInstanceProcAddr_(inst, name));
}

template <typename T>
static T LoadDev(VkDevice dev, const char* name)
{
    auto get = LoadInst<PFN_vkGetDeviceProcAddr>(s_instance, "vkGetDeviceProcAddr");
    return get ? reinterpret_cast<T>(get(dev, name)) : nullptr;
}

static bool LoadLoader(std::string& error)
{
    if (vkGetInstanceProcAddr_) return true;
#if defined(_WIN32)
    s_lib = (void*)LoadLibraryA("vulkan-1.dll");
    if (!s_lib)
    {
        error = "LoadLibrary(vulkan-1.dll) failed — install a Vulkan-capable GPU driver";
        return false;
    }
    vkGetInstanceProcAddr_ = (PFN_vkGetInstanceProcAddr)GetProcAddress((HMODULE)s_lib, "vkGetInstanceProcAddr");
#else
    if (!SDL_Vulkan_LoadLibrary(nullptr))
    {
        error = std::string("SDL_Vulkan_LoadLibrary failed: ") + SDL_GetError();
        return false;
    }
    vkGetInstanceProcAddr_ = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
#endif
    if (!vkGetInstanceProcAddr_)
    {
        error = "vkGetInstanceProcAddr missing";
        return false;
    }
    pfn_vkCreateInstance = LoadInst<PFN_vkCreateInstance>(VK_NULL_HANDLE, "vkCreateInstance");
    (void)vkEnumerateInstanceExtensionProperties_unused;
    if (!pfn_vkCreateInstance)
    {
        error = "vkCreateInstance missing";
        return false;
    }
    return true;
}

static void LoadInstanceFns()
{
    pfn_vkDestroyInstance = LoadInst<PFN_vkDestroyInstance>(s_instance, "vkDestroyInstance");
    pfn_vkEnumeratePhysicalDevices = LoadInst<PFN_vkEnumeratePhysicalDevices>(s_instance, "vkEnumeratePhysicalDevices");
    pfn_vkGetPhysicalDeviceProperties = LoadInst<PFN_vkGetPhysicalDeviceProperties>(s_instance, "vkGetPhysicalDeviceProperties");
    pfn_vkGetPhysicalDeviceQueueFamilyProperties = LoadInst<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(s_instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    pfn_vkEnumerateDeviceExtensionProperties = LoadInst<PFN_vkEnumerateDeviceExtensionProperties>(s_instance, "vkEnumerateDeviceExtensionProperties");
    pfn_vkGetPhysicalDeviceSurfaceSupportKHR = LoadInst<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(s_instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
    pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = LoadInst<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(s_instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    pfn_vkGetPhysicalDeviceSurfaceFormatsKHR = LoadInst<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(s_instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    pfn_vkGetPhysicalDeviceSurfacePresentModesKHR = LoadInst<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(s_instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    pfn_vkCreateDevice = LoadInst<PFN_vkCreateDevice>(s_instance, "vkCreateDevice");
    pfn_vkDestroyDevice = LoadInst<PFN_vkDestroyDevice>(s_instance, "vkDestroyDevice");
    pfn_vkGetDeviceQueue = LoadInst<PFN_vkGetDeviceQueue>(s_instance, "vkGetDeviceQueue");
    pfn_vkDestroySurfaceKHR = LoadInst<PFN_vkDestroySurfaceKHR>(s_instance, "vkDestroySurfaceKHR");
#if defined(_WIN32)
    pfn_vkCreateWin32SurfaceKHR = LoadInst<PFN_vkCreateWin32SurfaceKHR>(s_instance, "vkCreateWin32SurfaceKHR");
#endif
}

static void LoadDeviceFns()
{
    pfn_vkCreateSwapchainKHR = LoadDev<PFN_vkCreateSwapchainKHR>(s_device, "vkCreateSwapchainKHR");
    pfn_vkDestroySwapchainKHR = LoadDev<PFN_vkDestroySwapchainKHR>(s_device, "vkDestroySwapchainKHR");
    pfn_vkGetSwapchainImagesKHR = LoadDev<PFN_vkGetSwapchainImagesKHR>(s_device, "vkGetSwapchainImagesKHR");
    pfn_vkCreateImageView = LoadDev<PFN_vkCreateImageView>(s_device, "vkCreateImageView");
    pfn_vkDestroyImageView = LoadDev<PFN_vkDestroyImageView>(s_device, "vkDestroyImageView");
    pfn_vkCreateRenderPass = LoadDev<PFN_vkCreateRenderPass>(s_device, "vkCreateRenderPass");
    pfn_vkDestroyRenderPass = LoadDev<PFN_vkDestroyRenderPass>(s_device, "vkDestroyRenderPass");
    pfn_vkCreateFramebuffer = LoadDev<PFN_vkCreateFramebuffer>(s_device, "vkCreateFramebuffer");
    pfn_vkDestroyFramebuffer = LoadDev<PFN_vkDestroyFramebuffer>(s_device, "vkDestroyFramebuffer");
    pfn_vkCreateCommandPool = LoadDev<PFN_vkCreateCommandPool>(s_device, "vkCreateCommandPool");
    pfn_vkDestroyCommandPool = LoadDev<PFN_vkDestroyCommandPool>(s_device, "vkDestroyCommandPool");
    pfn_vkAllocateCommandBuffers = LoadDev<PFN_vkAllocateCommandBuffers>(s_device, "vkAllocateCommandBuffers");
    pfn_vkBeginCommandBuffer = LoadDev<PFN_vkBeginCommandBuffer>(s_device, "vkBeginCommandBuffer");
    pfn_vkEndCommandBuffer = LoadDev<PFN_vkEndCommandBuffer>(s_device, "vkEndCommandBuffer");
    pfn_vkCmdBeginRenderPass = LoadDev<PFN_vkCmdBeginRenderPass>(s_device, "vkCmdBeginRenderPass");
    pfn_vkCmdEndRenderPass = LoadDev<PFN_vkCmdEndRenderPass>(s_device, "vkCmdEndRenderPass");
    pfn_vkCreateSemaphore = LoadDev<PFN_vkCreateSemaphore>(s_device, "vkCreateSemaphore");
    pfn_vkDestroySemaphore = LoadDev<PFN_vkDestroySemaphore>(s_device, "vkDestroySemaphore");
    pfn_vkCreateFence = LoadDev<PFN_vkCreateFence>(s_device, "vkCreateFence");
    pfn_vkDestroyFence = LoadDev<PFN_vkDestroyFence>(s_device, "vkDestroyFence");
    pfn_vkWaitForFences = LoadDev<PFN_vkWaitForFences>(s_device, "vkWaitForFences");
    pfn_vkResetFences = LoadDev<PFN_vkResetFences>(s_device, "vkResetFences");
    pfn_vkAcquireNextImageKHR = LoadDev<PFN_vkAcquireNextImageKHR>(s_device, "vkAcquireNextImageKHR");
    pfn_vkQueueSubmit = LoadDev<PFN_vkQueueSubmit>(s_device, "vkQueueSubmit");
    pfn_vkQueuePresentKHR = LoadDev<PFN_vkQueuePresentKHR>(s_device, "vkQueuePresentKHR");
    pfn_vkDeviceWaitIdle = LoadDev<PFN_vkDeviceWaitIdle>(s_device, "vkDeviceWaitIdle");
    pfn_vkResetCommandBuffer = LoadDev<PFN_vkResetCommandBuffer>(s_device, "vkResetCommandBuffer");
}

static VkDeviceInfo FromProps(const VkPhysicalDeviceProperties& p)
{
    VkDeviceInfo d;
    d.name = p.deviceName;
    d.vendorId = p.vendorID;
    d.deviceId = p.deviceID;
    d.driverVersion = p.driverVersion;
    d.apiVersion = p.apiVersion;
    d.discrete = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    d.integrated = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    d.cpu = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
    return d;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                         VkDebugUtilsMessageTypeFlagsEXT /*types*/,
                                                         const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                         void* /*user*/)
{
    const char* sev = "info";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) sev = "error";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) sev = "warn";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) sev = "verbose";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        LOG_ERROR("vulkan validation [{}]: {}", sev, data && data->pMessage ? data->pMessage : "(null)");
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        LOG_WARN("vulkan validation [{}]: {}", sev, data && data->pMessage ? data->pMessage : "(null)");
    else
        LOG_INFO("vulkan validation [{}]: {}", sev, data && data->pMessage ? data->pMessage : "(null)");
    return VK_FALSE;
}

static bool InstanceHasLayer(const char* name)
{
    auto enumExt = LoadInst<PFN_vkEnumerateInstanceLayerProperties>(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties");
    if (!enumExt) return false;
    uint32_t n = 0;
    enumExt(&n, nullptr);
    std::vector<VkLayerProperties> props(n);
    enumExt(&n, props.data());
    for (auto& p : props)
        if (std::strcmp(p.layerName, name) == 0) return true;
    return false;
}

static bool InstanceHasExt(const char* name)
{
    auto enumExt =
        LoadInst<PFN_vkEnumerateInstanceExtensionProperties>(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    if (!enumExt) return false;
    uint32_t n = 0;
    enumExt(nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> props(n);
    enumExt(nullptr, &n, props.data());
    for (auto& p : props)
        if (std::strcmp(p.extensionName, name) == 0) return true;
    return false;
}

static bool CreateInstance(bool needSurface, std::string& error)
{
    if (!LoadLoader(error)) return false;

    std::vector<const char*> exts;
#if defined(_WIN32)
    if (needSurface)
    {
        exts.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        exts.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    }
#else
    if (needSurface)
    {
        Uint32 count = 0;
        const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&count);
        if (!sdlExts || !count)
        {
            error = std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError();
            return false;
        }
        for (Uint32 i = 0; i < count; i++)
            exts.push_back(sdlExts[i]);
    }
#endif

    std::vector<const char*> layers;
    s_validationEnabled = false;
    if (s_validationWanted)
    {
        if (InstanceHasLayer("VK_LAYER_KHRONOS_validation"))
        {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            s_validationEnabled = true;
        }
        else
            LOG_WARN("vulkan: SCR_VK_VALIDATION requested but VK_LAYER_KHRONOS_validation is unavailable");
        if (InstanceHasExt(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
            exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        else if (s_validationEnabled)
            LOG_WARN("vulkan: validation enabled but VK_EXT_debug_utils is unavailable");
    }

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "SonicColorsRecomp";
    app.applicationVersion = VK_MAKE_VERSION(0, 4, 3);
    app.pEngineName = "SCR-A4d";
    app.engineVersion = VK_MAKE_VERSION(0, 4, 3);
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = (uint32_t)exts.size();
    ci.ppEnabledExtensionNames = exts.empty() ? nullptr : exts.data();
    ci.enabledLayerCount = (uint32_t)layers.size();
    ci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

    VkResult r = pfn_vkCreateInstance(&ci, nullptr, &s_instance);
    if (r != VK_SUCCESS)
    {
        error = "vkCreateInstance failed (" + std::to_string((int)r) + ")";
        return false;
    }
    LoadInstanceFns();

    if (s_validationEnabled)
    {
        pfn_vkCreateDebugUtilsMessengerEXT =
            LoadInst<PFN_vkCreateDebugUtilsMessengerEXT>(s_instance, "vkCreateDebugUtilsMessengerEXT");
        pfn_vkDestroyDebugUtilsMessengerEXT =
            LoadInst<PFN_vkDestroyDebugUtilsMessengerEXT>(s_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (pfn_vkCreateDebugUtilsMessengerEXT)
        {
            VkDebugUtilsMessengerCreateInfoEXT dci{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
            dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dci.pfnUserCallback = DebugUtilsCallback;
            if (pfn_vkCreateDebugUtilsMessengerEXT(s_instance, &dci, nullptr, &s_debugMessenger) != VK_SUCCESS)
            {
                LOG_WARN("vulkan: vkCreateDebugUtilsMessengerEXT failed");
                s_debugMessenger = VK_NULL_HANDLE;
            }
            else
                LOG_INFO("vulkan: KHronos validation layer + debug_utils active");
        }
    }
    return true;
}

static bool DeviceHasExt(VkPhysicalDevice pd, const char* name)
{
    uint32_t n = 0;
    pfn_vkEnumerateDeviceExtensionProperties(pd, nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> props(n);
    pfn_vkEnumerateDeviceExtensionProperties(pd, nullptr, &n, props.data());
    for (auto& p : props)
        if (std::strcmp(p.extensionName, name) == 0) return true;
    return false;
}

static bool FindQueue(VkPhysicalDevice pd, VkSurfaceKHR surface, uint32_t& familyOut)
{
    uint32_t n = 0;
    pfn_vkGetPhysicalDeviceQueueFamilyProperties(pd, &n, nullptr);
    std::vector<VkQueueFamilyProperties> props(n);
    pfn_vkGetPhysicalDeviceQueueFamilyProperties(pd, &n, props.data());
    for (uint32_t i = 0; i < n; i++)
    {
        if (!(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
        if (surface != VK_NULL_HANDLE)
        {
            VkBool32 present = VK_FALSE;
            pfn_vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &present);
            if (!present) continue;
        }
        familyOut = i;
        return true;
    }
    return false;
}

static int ScoreDevice(const VkDeviceInfo& d, const VkInitOptions& opt)
{
    if (d.cpu) return -1000;
    int score = 0;
    if (d.discrete) score += 1000;
    if (d.integrated) score += 100;
    if (opt.preferVendorId && d.vendorId == opt.preferVendorId) score += 500;
    if (opt.preferDeviceId && d.deviceId == opt.preferDeviceId) score += 500;
    if (!opt.preferDevice.empty())
    {
        std::string lower = d.name;
        std::string needle = opt.preferDevice;
        for (char& c : lower) c = (char)std::tolower((unsigned char)c);
        for (char& c : needle) c = (char)std::tolower((unsigned char)c);
        if (lower.find(needle) != std::string::npos) score += 2000;
    }
    return score;
}

static void DestroySwapchainObjects()
{
    if (!s_device) return;
    if (s_device && pfn_vkDeviceWaitIdle) pfn_vkDeviceWaitIdle(s_device);
    for (auto fb : s_framebuffers)
        if (fb) pfn_vkDestroyFramebuffer(s_device, fb, nullptr);
    s_framebuffers.clear();
    for (auto v : s_views)
        if (v) pfn_vkDestroyImageView(s_device, v, nullptr);
    s_views.clear();
    s_images.clear();
    for (VkSemaphore s : s_renderFinished)
        if (s) pfn_vkDestroySemaphore(s_device, s, nullptr);
    s_renderFinished.clear();
    if (s_swapchain)
    {
        pfn_vkDestroySwapchainKHR(s_device, s_swapchain, nullptr);
        s_swapchain = VK_NULL_HANDLE;
    }
}

static bool CreateSwapchainObjects(std::string& error)
{
    VkSurfaceCapabilitiesKHR caps{};
    pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_phys, s_surface, &caps);

    uint32_t fmtCount = 0;
    pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &fmtCount, formats.data());
    VkSurfaceFormatKHR chosen = formats[0];
    for (auto& f : formats)
    {
        if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosen = f;
            break;
        }
    }
    s_swapFormat = chosen.format;
    s_swapColorSpace = chosen.colorSpace;

    uint32_t modeCount = 0;
    pfn_vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys, s_surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    pfn_vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys, s_surface, &modeCount, modes.data());
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!s_vsync)
    {
        // Prefer mailbox, then immediate (tearing), else FIFO.
        for (auto m : modes)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = m; break; }
        if (presentMode == VK_PRESENT_MODE_FIFO_KHR && s_allowTearing)
            for (auto m : modes)
                if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) { presentMode = m; break; }
    }

    if (caps.currentExtent.width != UINT32_MAX)
        s_extent = caps.currentExtent;
    else
    {
        int cw = 1280, ch = 720;
        Window_ClientSize(cw, ch);
        s_extent.width = std::clamp((uint32_t)cw, caps.minImageExtent.width, caps.maxImageExtent.width);
        s_extent.height = std::clamp((uint32_t)ch, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = std::max(caps.minImageCount, s_preferredImageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    // A4d root-cause fix: XFB present blits into swapchain images. Without TRANSFER_DST,
    // vkCmdBlitImage is illegal and RADV frequently returns VK_ERROR_DEVICE_LOST (-4).
    // NVIDIA was more permissive, which hid the bug in A4c.
    s_swapTransferDst = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (s_swapTransferDst)
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    else
        LOG_WARN("vulkan: surface lacks TRANSFER_DST — XFB blit present unavailable");

    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = s_surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = s_swapFormat;
    sci.imageColorSpace = s_swapColorSpace;
    sci.imageExtent = s_extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = usage;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = presentMode;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;

    if (pfn_vkCreateSwapchainKHR(s_device, &sci, nullptr, &s_swapchain) != VK_SUCCESS)
    {
        error = "vkCreateSwapchainKHR failed";
        return false;
    }

    uint32_t imgN = 0;
    pfn_vkGetSwapchainImagesKHR(s_device, s_swapchain, &imgN, nullptr);
    s_images.resize(imgN);
    pfn_vkGetSwapchainImagesKHR(s_device, s_swapchain, &imgN, s_images.data());
    s_imageCount = imgN;
    s_views.resize(imgN);
    s_framebuffers.resize(imgN);
    for (uint32_t i = 0; i < imgN; i++)
    {
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = s_images[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = s_swapFormat;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        if (pfn_vkCreateImageView(s_device, &vi, nullptr, &s_views[i]) != VK_SUCCESS)
        {
            error = "vkCreateImageView failed";
            return false;
        }
        VkFramebufferCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass = s_renderPass;
        fi.attachmentCount = 1;
        fi.pAttachments = &s_views[i];
        fi.width = s_extent.width;
        fi.height = s_extent.height;
        fi.layers = 1;
        if (pfn_vkCreateFramebuffer(s_device, &fi, nullptr, &s_framebuffers[i]) != VK_SUCCESS)
        {
            error = "vkCreateFramebuffer failed";
            return false;
        }
    }

    // Recreate per-image present-wait semaphores (safe to signal when that image is acquired again).
    for (VkSemaphore s : s_renderFinished)
        if (s) pfn_vkDestroySemaphore(s_device, s, nullptr);
    s_renderFinished.assign(s_imageCount, VK_NULL_HANDLE);
    VkSemaphoreCreateInfo sei{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (uint32_t i = 0; i < s_imageCount; i++)
    {
        if (pfn_vkCreateSemaphore(s_device, &sei, nullptr, &s_renderFinished[i]) != VK_SUCCESS)
        {
            error = "vkCreateSemaphore (present) failed";
            return false;
        }
    }

    const char* modeName = presentMode == VK_PRESENT_MODE_MAILBOX_KHR     ? "mailbox"
                           : presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "immediate"
                                                                          : "fifo";
    LOG_INFO("vulkan: swapchain {}x{} images={} format={} present={}", s_extent.width, s_extent.height,
             s_imageCount, (int)s_swapFormat, modeName);
    return true;
}

} // namespace

bool Vk_ListDevices(std::vector<VkDeviceInfo>& out, std::string& error)
{
    std::lock_guard lk(s_mu);
    out.clear();
    const bool had = s_instance != VK_NULL_HANDLE;
    if (!had && !CreateInstance(false, error)) return false;
    uint32_t n = 0;
    pfn_vkEnumeratePhysicalDevices(s_instance, &n, nullptr);
    std::vector<VkPhysicalDevice> devs(n);
    pfn_vkEnumeratePhysicalDevices(s_instance, &n, devs.data());
    for (auto pd : devs)
    {
        VkPhysicalDeviceProperties props{};
        pfn_vkGetPhysicalDeviceProperties(pd, &props);
        out.push_back(FromProps(props));
    }
    if (!had)
    {
        pfn_vkDestroyInstance(s_instance, nullptr);
        s_instance = VK_NULL_HANDLE;
    }
    return true;
}

int Vk_PrintDevices()
{
    std::vector<VkDeviceInfo> devs;
    std::string err;
    if (!Vk_ListDevices(devs, err))
    {
        std::fprintf(stderr, "vulkan: %s\n", err.c_str());
        return 1;
    }
    std::printf("Vulkan physical devices (%zu):\n", devs.size());
    for (size_t i = 0; i < devs.size(); i++)
    {
        const auto& d = devs[i];
        const char* kind = d.discrete ? "discrete" : d.integrated ? "integrated" : d.cpu ? "cpu" : "other";
        std::printf("  [%zu] %s\n      vendor=%04x device=%04x driver=%u api=%u.%u.%u (%s)\n", i, d.name.c_str(),
                    d.vendorId, d.deviceId, d.driverVersion, VK_VERSION_MAJOR(d.apiVersion),
                    VK_VERSION_MINOR(d.apiVersion), VK_VERSION_PATCH(d.apiVersion), kind);
    }
    return 0;
}

bool Vk_Init(const VkInitOptions& opt, std::string& error)
{
    std::lock_guard lk(s_mu);
    if (s_device)
    {
        error = "Vulkan already initialised";
        return false;
    }
    if (!opt.windowHandle)
    {
        error = "Vk_Init requires a window handle";
        return false;
    }
    s_vsync = opt.vsync;
    s_allowTearing = opt.allowTearing;
    s_preferredImageCount = std::clamp(opt.preferredImageCount ? opt.preferredImageCount : 3u, 2u, 4u);
    s_validationWanted = opt.enableValidation;
    if (const char* e = std::getenv("SCR_VK_VALIDATION"))
        if (e[0] && e[0] != '0') s_validationWanted = true;
    s_syncFrame = 0;
    s_presentSerial = 0;
#if defined(_WIN32)
    s_hwnd = (HWND)opt.windowHandle;
#else
    s_sdlWindow = (SDL_Window*)opt.windowHandle;
#endif

    if (!CreateInstance(true, error)) return false;

#if defined(_WIN32)
    VkWin32SurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    sci.hinstance = GetModuleHandleW(nullptr);
    sci.hwnd = s_hwnd;
    if (!pfn_vkCreateWin32SurfaceKHR || pfn_vkCreateWin32SurfaceKHR(s_instance, &sci, nullptr, &s_surface) != VK_SUCCESS)
    {
        error = "vkCreateWin32SurfaceKHR failed";
        return false;
    }
#else
    if (!SDL_Vulkan_CreateSurface(s_sdlWindow, s_instance, nullptr, &s_surface))
    {
        error = std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError();
        return false;
    }
#endif

    uint32_t n = 0;
    pfn_vkEnumeratePhysicalDevices(s_instance, &n, nullptr);
    if (!n)
    {
        error = "no Vulkan physical devices";
        return false;
    }
    std::vector<VkPhysicalDevice> devs(n);
    pfn_vkEnumeratePhysicalDevices(s_instance, &n, devs.data());

    int bestScore = -100000;
    VkPhysicalDevice best = VK_NULL_HANDLE;
    uint32_t bestFamily = 0;
    VkDeviceInfo bestInfo{};
    for (auto pd : devs)
    {
        if (!DeviceHasExt(pd, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;
        uint32_t family = 0;
        if (!FindQueue(pd, s_surface, family)) continue;
        VkPhysicalDeviceProperties props{};
        pfn_vkGetPhysicalDeviceProperties(pd, &props);
        VkDeviceInfo info = FromProps(props);
        const int score = ScoreDevice(info, opt);
        LOG_INFO("vulkan: candidate '{}' vendor={:04x} device={:04x} score={}", info.name, info.vendorId,
                 info.deviceId, score);
        if (score > bestScore)
        {
            bestScore = score;
            best = pd;
            bestFamily = family;
            bestInfo = info;
        }
    }
    if (!best)
    {
        error = "no suitable Vulkan GPU with graphics+present+swapchain";
        return false;
    }
    s_phys = best;
    s_queueFamily = bestFamily;
    s_selected = bestInfo;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = s_queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    const char* dexts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = dexts;
    if (pfn_vkCreateDevice(s_phys, &dci, nullptr, &s_device) != VK_SUCCESS)
    {
        error = "vkCreateDevice failed";
        return false;
    }
    LoadDeviceFns();
    pfn_vkGetDeviceQueue(s_device, s_queueFamily, 0, &s_queue);

    VkAttachmentDescription color{};
    color.format = VK_FORMAT_B8G8R8A8_UNORM; // updated after swapchain format known — recreate below
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rpci{};
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;

    // Probe format first via temporary swapchain-less query then create render pass with real format.
    {
        uint32_t fmtCount = 0;
        pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &fmtCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &fmtCount, formats.data());
        s_swapFormat = formats[0].format;
        for (auto& f : formats)
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM)
            {
                s_swapFormat = f.format;
                break;
            }
        color.format = s_swapFormat;
    }
    if (pfn_vkCreateRenderPass(s_device, &rpci, nullptr, &s_renderPass) != VK_SUCCESS)
    {
        error = "vkCreateRenderPass failed";
        return false;
    }

    if (!CreateSwapchainObjects(error)) return false;

    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.queueFamilyIndex = s_queueFamily;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (pfn_vkCreateCommandPool(s_device, &pci, nullptr, &s_cmdPool) != VK_SUCCESS)
    {
        error = "vkCreateCommandPool failed";
        return false;
    }
    VkCommandBufferAllocateInfo cai{};
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = s_cmdPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    if (pfn_vkAllocateCommandBuffers(s_device, &cai, &s_cmd) != VK_SUCCESS)
    {
        error = "vkAllocateCommandBuffers failed";
        return false;
    }
    VkSemaphoreCreateInfo sei{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fei{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fei.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (uint32_t i = 0; i < kSyncFrames; i++)
    {
        if (pfn_vkCreateSemaphore(s_device, &sei, nullptr, &s_imageAvailable[i]) != VK_SUCCESS ||
            pfn_vkCreateFence(s_device, &fei, nullptr, &s_inFlight[i]) != VK_SUCCESS)
        {
            error = "vulkan sync object creation failed";
            return false;
        }
    }

    LOG_INFO("vulkan: selected '{}' vendor={:04x} device={:04x} api={}.{}.{} (native Vulkan, not DXVK)",
             s_selected.name, s_selected.vendorId, s_selected.deviceId, VK_VERSION_MAJOR(s_selected.apiVersion),
             VK_VERSION_MINOR(s_selected.apiVersion), VK_VERSION_PATCH(s_selected.apiVersion));
    LOG_INFO("vulkan: swapchain usage transfer_dst={} validation={} images={} format={} presentSerial={}",
             s_swapTransferDst, s_validationEnabled, s_imageCount, (int)s_swapFormat, s_presentSerial);
    return true;
}

void Vk_Shutdown()
{
    std::lock_guard lk(s_mu);
    if (s_device && pfn_vkDeviceWaitIdle) pfn_vkDeviceWaitIdle(s_device);
    DestroySwapchainObjects();
    if (s_device)
    {
        for (uint32_t i = 0; i < kSyncFrames; i++)
        {
            if (s_imageAvailable[i]) pfn_vkDestroySemaphore(s_device, s_imageAvailable[i], nullptr);
            if (s_inFlight[i]) pfn_vkDestroyFence(s_device, s_inFlight[i], nullptr);
            s_imageAvailable[i] = VK_NULL_HANDLE;
            s_inFlight[i] = VK_NULL_HANDLE;
        }
        if (s_cmdPool) pfn_vkDestroyCommandPool(s_device, s_cmdPool, nullptr);
        if (s_renderPass) pfn_vkDestroyRenderPass(s_device, s_renderPass, nullptr);
        pfn_vkDestroyDevice(s_device, nullptr);
    }
    s_cmdPool = VK_NULL_HANDLE;
    s_cmd = VK_NULL_HANDLE;
    s_renderPass = VK_NULL_HANDLE;
    s_device = VK_NULL_HANDLE;
    s_queue = VK_NULL_HANDLE;
    s_phys = VK_NULL_HANDLE;
    if (s_surface && s_instance) pfn_vkDestroySurfaceKHR(s_instance, s_surface, nullptr);
    s_surface = VK_NULL_HANDLE;
    if (s_debugMessenger && pfn_vkDestroyDebugUtilsMessengerEXT)
        pfn_vkDestroyDebugUtilsMessengerEXT(s_instance, s_debugMessenger, nullptr);
    s_debugMessenger = VK_NULL_HANDLE;
    if (s_instance) pfn_vkDestroyInstance(s_instance, nullptr);
    s_validationEnabled = false;
    s_instance = VK_NULL_HANDLE;
    s_selected = {};
    s_imageCount = 0;
#if !defined(_WIN32)
    SDL_Vulkan_UnloadLibrary();
#endif
#if defined(_WIN32)
    if (s_lib)
    {
        FreeLibrary((HMODULE)s_lib);
        s_lib = nullptr;
    }
#endif
    vkGetInstanceProcAddr_ = nullptr;
}

bool Vk_IsReady() { return s_device != VK_NULL_HANDLE && s_swapchain != VK_NULL_HANDLE; }
std::string Vk_SelectedDeviceName() { return s_selected.name; }
uint32_t Vk_SelectedVendorId() { return s_selected.vendorId; }
uint32_t Vk_SelectedDeviceId() { return s_selected.deviceId; }
uint32_t Vk_SwapchainImageCount() { return s_imageCount; }
bool Vk_SwapchainSupportsTransferDst() { return s_swapTransferDst; }
bool Vk_ValidationEnabled() { return s_validationEnabled; }
uint64_t Vk_PresentSerial() { return s_presentSerial; }


bool Vk_RecreateSwapchain(std::string& error)
{
    std::lock_guard lk(s_mu);
    if (!s_device) return false;
    DestroySwapchainObjects();
    return CreateSwapchainObjects(error);
}

bool Vk_PresentClear(float r, float g, float b, float a, std::string& error)
{
    for (int attempt = 0; attempt < 2; attempt++)
    {
        if (!Vk_IsReady())
        {
            error = "Vulkan not ready";
            return false;
        }
        const uint32_t fi = s_syncFrame % kSyncFrames;
        pfn_vkWaitForFences(s_device, 1, &s_inFlight[fi], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = 0;
        VkResult ar =
            pfn_vkAcquireNextImageKHR(s_device, s_swapchain, UINT64_MAX, s_imageAvailable[fi], VK_NULL_HANDLE, &imageIndex);
        if (ar == VK_ERROR_OUT_OF_DATE_KHR)
        {
            if (!Vk_RecreateSwapchain(error)) return false;
            continue;
        }
        if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR)
        {
            error = "vkAcquireNextImageKHR failed";
            return false;
        }

        pfn_vkResetFences(s_device, 1, &s_inFlight[fi]);
        pfn_vkResetCommandBuffer(s_cmd, 0);
        VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        pfn_vkBeginCommandBuffer(s_cmd, &bi);
        VkClearValue clear{};
        clear.color.float32[0] = r;
        clear.color.float32[1] = g;
        clear.color.float32[2] = b;
        clear.color.float32[3] = a;
        VkRenderPassBeginInfo rpbi{};
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = s_renderPass;
        rpbi.framebuffer = s_framebuffers[imageIndex];
        rpbi.renderArea.extent = s_extent;
        rpbi.clearValueCount = 1;
        rpbi.pClearValues = &clear;
        pfn_vkCmdBeginRenderPass(s_cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        pfn_vkCmdEndRenderPass(s_cmd);
        pfn_vkEndCommandBuffer(s_cmd);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &s_imageAvailable[fi];
        si.pWaitDstStageMask = &waitStage;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &s_cmd;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &s_renderFinished[imageIndex];
        if (pfn_vkQueueSubmit(s_queue, 1, &si, s_inFlight[fi]) != VK_SUCCESS)
        {
            error = "vkQueueSubmit failed";
            return false;
        }

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &s_renderFinished[imageIndex];
        pi.swapchainCount = 1;
        pi.pSwapchains = &s_swapchain;
        pi.pImageIndices = &imageIndex;
        VkResult pr = pfn_vkQueuePresentKHR(s_queue, &pi);
        if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
        {
            if (!Vk_RecreateSwapchain(error)) return false;
            continue;
        }
        if (pr != VK_SUCCESS)
        {
            error = "vkQueuePresentKHR failed (" + std::to_string((int)pr) + ")";
            return false;
        }
        s_syncFrame = (s_syncFrame + 1) % kSyncFrames;
        s_presentSerial++;
        return true;
    }
    error = "swapchain recreate loop exhausted";
    return false;
}

VkInstance Vk_GetInstance() { return s_instance; }
VkPhysicalDevice Vk_GetPhysicalDevice() { return s_phys; }
VkDevice Vk_GetDevice() { return s_device; }
VkQueue Vk_GetGraphicsQueue() { return s_queue; }
uint32_t Vk_GetGraphicsQueueFamily() { return s_queueFamily; }
VkExtent2D Vk_GetSwapchainExtent() { return s_extent; }
VkFormat Vk_GetSwapchainFormat() { return s_swapFormat; }
VkSwapchainKHR Vk_GetSwapchain() { return s_swapchain; }
uint32_t Vk_GetSwapchainImageCount() { return (uint32_t)s_images.size(); }
VkImage Vk_GetSwapchainImage(uint32_t index)
{
    return index < s_images.size() ? s_images[index] : VK_NULL_HANDLE;
}
PFN_vkVoidFunction Vk_GetInstanceProc(const char* name)
{
    return vkGetInstanceProcAddr_ ? vkGetInstanceProcAddr_(s_instance, name) : nullptr;
}
PFN_vkVoidFunction Vk_GetDeviceProc(const char* name)
{
    auto get = LoadInst<PFN_vkGetDeviceProcAddr>(s_instance, "vkGetDeviceProcAddr");
    return (get && s_device) ? get(s_device, name) : nullptr;
}

bool Vk_AcquireImage(uint32_t& imageIndex, std::string& error)
{
    if (!Vk_IsReady())
    {
        error = "Vulkan not ready";
        return false;
    }
    const uint32_t fi = s_syncFrame % kSyncFrames;
    pfn_vkWaitForFences(s_device, 1, &s_inFlight[fi], VK_TRUE, UINT64_MAX);
    VkResult ar =
        pfn_vkAcquireNextImageKHR(s_device, s_swapchain, UINT64_MAX, s_imageAvailable[fi], VK_NULL_HANDLE, &imageIndex);
    if (ar == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if (!Vk_RecreateSwapchain(error)) return false;
        ar = pfn_vkAcquireNextImageKHR(s_device, s_swapchain, UINT64_MAX, s_imageAvailable[fi], VK_NULL_HANDLE, &imageIndex);
    }
    if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR)
    {
        error = "vkAcquireNextImageKHR failed (" + std::to_string((int)ar) + ") serial=" + std::to_string(s_presentSerial);
        return false;
    }
    pfn_vkResetFences(s_device, 1, &s_inFlight[fi]);
    return true;
}

static uint64_t s_lastPresentWaitNs = 0;
static bool s_lastExtraWaitConsumed = false;

bool Vk_LastSubmitWaitedExtraSem() { return s_lastExtraWaitConsumed; }

bool Vk_SubmitPresent(VkCommandBuffer cmd, uint32_t imageIndex, std::string& error, VkPipelineStageFlags waitStage,
                      VkSemaphore extraWaitSem)
{
    s_lastExtraWaitConsumed = false;
    const uint32_t fi = s_syncFrame % kSyncFrames;
    if (imageIndex >= s_renderFinished.size() || s_renderFinished[imageIndex] == VK_NULL_HANDLE)
    {
        error = "present semaphore missing for imageIndex=" + std::to_string(imageIndex);
        return false;
    }
    VkSemaphore waits[2]{};
    VkPipelineStageFlags stages[2]{};
    uint32_t waitCount = 0;
    waits[waitCount] = s_imageAvailable[fi];
    stages[waitCount] = waitStage;
    waitCount++;
    if (extraWaitSem != VK_NULL_HANDLE)
    {
        waits[waitCount] = extraWaitSem;
        stages[waitCount] = waitStage;
        waitCount++;
    }
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = waitCount;
    si.pWaitSemaphores = waits;
    si.pWaitDstStageMask = stages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &s_renderFinished[imageIndex];
    {
        VkResult sr = pfn_vkQueueSubmit(s_queue, 1, &si, s_inFlight[fi]);
        if (sr != VK_SUCCESS)
        {
            error = "vkQueueSubmit failed (" + std::to_string((int)sr) + ") serial=" + std::to_string(s_presentSerial) +
                    " syncFrame=" + std::to_string(fi);
            return false;
        }
        if (extraWaitSem != VK_NULL_HANDLE) s_lastExtraWaitConsumed = true;
    }
    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &s_renderFinished[imageIndex];
    pi.swapchainCount = 1;
    pi.pSwapchains = &s_swapchain;
    pi.pImageIndices = &imageIndex;
    VkResult pr = pfn_vkQueuePresentKHR(s_queue, &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
    {
        // A4d: do not treat failed/suboptimal present as success. Recreate for the next frame.
        std::string e2;
        Vk_RecreateSwapchain(e2);
        s_syncFrame = (s_syncFrame + 1) % kSyncFrames;
        s_presentSerial++;
        if (pr == VK_ERROR_OUT_OF_DATE_KHR)
        {
            error = "vkQueuePresentKHR OUT_OF_DATE (swapchain recreated) serial=" +
                    std::to_string(s_presentSerial);
            return false;
        }
        // SUBOPTIMAL: image was presented; allow success after recreate for next acquire.
        return true;
    }
    if (pr != VK_SUCCESS)
    {
        error = "vkQueuePresentKHR failed (" + std::to_string((int)pr) + ") serial=" + std::to_string(s_presentSerial);
        return false;
    }
    s_syncFrame = (s_syncFrame + 1) % kSyncFrames;
    s_presentSerial++;
    return true;
}

void Vk_DeviceWaitIdle()
{
    if (s_device && pfn_vkDeviceWaitIdle) pfn_vkDeviceWaitIdle(s_device);
}

void Vk_WaitLastSubmit()
{
    s_lastPresentWaitNs = 0;
    if (!s_device || !pfn_vkWaitForFences) return;
    // SubmitPresent advances s_syncFrame after queue submit; wait the fence that was just signaled.
    const uint32_t fi = (s_syncFrame + kSyncFrames - 1) % kSyncFrames;
    if (s_inFlight[fi])
    {
        using clock = std::chrono::steady_clock;
        const auto t0 = clock::now();
        pfn_vkWaitForFences(s_device, 1, &s_inFlight[fi], VK_TRUE, UINT64_MAX);
        s_lastPresentWaitNs =
            (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - t0).count();
    }
}

uint64_t Vk_LastPresentWaitNs() { return s_lastPresentWaitNs; }

int Vk_SmokePresentFrames(int frames, const VkInitOptions& opt, std::string& error)
{
    if (!Vk_Init(opt, error)) return 1;
    for (int i = 0; i < frames; i++)
    {
        const float t = (float)i / (float)std::max(1, frames);
        if (!Vk_PresentClear(0.05f, 0.15f + 0.2f * t, 0.35f, 1.0f, error))
        {
            Vk_Shutdown();
            return 2;
        }
    }
    LOG_INFO("vulkan smoke: presented {} clear frames on '{}'", frames, Vk_SelectedDeviceName());
    Vk_Shutdown();
    return 0;
}
