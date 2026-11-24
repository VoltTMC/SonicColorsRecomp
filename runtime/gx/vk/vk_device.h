#pragma once
// A4b Stage A: native Vulkan instance / device / surface / swapchain.
// Does not route through D3D11 or DXVK. GX draw translation is Stage B+.
#include <cstdint>
#include <string>
#include <vector>

#if defined(_WIN32)
#  ifndef VK_USE_PLATFORM_WIN32_KHR
#    define VK_USE_PLATFORM_WIN32_KHR
#  endif
#endif
#include <vulkan/vulkan.h>

struct VkDeviceInfo
{
    std::string name;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint32_t driverVersion = 0;
    uint32_t apiVersion = 0;
    bool discrete = false;
    bool integrated = false;
    bool cpu = false;
};

// Create a temporary VkInstance (no surface) and list physical devices.
bool Vk_ListDevices(std::vector<VkDeviceInfo>& out, std::string& error);
// Print devices to stdout; returns process exit code (0 ok, 1 error).
int Vk_PrintDevices();

struct VkInitOptions
{
    void* windowHandle = nullptr;   // Linux: SDL_Window*; Windows: HWND
    bool vsync = false;
    bool allowTearing = true;
    // Prefer matching this substring in deviceName (e.g. "3080", "RAPHAEL"); empty = auto.
    std::string preferDevice;
    uint32_t preferVendorId = 0;
    uint32_t preferDeviceId = 0;
    // A4a reference: prefer 3 images when supported.
    uint32_t preferredImageCount = 3;
    // A4d: enable KHronos validation + debug utils when available (also SCR_VK_VALIDATION=1).
    bool enableValidation = false;
};

// Full device + swapchain for presentation. Returns false on failure (error filled).
bool Vk_Init(const VkInitOptions& opt, std::string& error);
void Vk_Shutdown();
bool Vk_IsReady();

// Selected device summary (empty if not ready).
std::string Vk_SelectedDeviceName();
uint32_t Vk_SelectedVendorId();
uint32_t Vk_SelectedDeviceId();
uint32_t Vk_SwapchainImageCount();
// True when swapchain images were created with TRANSFER_DST (required for XFB blit present).
bool Vk_SwapchainSupportsTransferDst();
bool Vk_ValidationEnabled();
uint64_t Vk_PresentSerial();

// Recreate swapchain after resize (or out-of-date). Safe no-op if not ready.
bool Vk_RecreateSwapchain(std::string& error);

// Clear to rgba and present one frame. Handles acquire / submit / present.
// Returns false on fatal error; out-of-date triggers recreate + retry once.
bool Vk_PresentClear(float r, float g, float b, float a, std::string& error);

// Stage A smoke: create window externally, then call Init + N clear presents.
int Vk_SmokePresentFrames(int frames, const VkInitOptions& opt, std::string& error);

// A4c Stage B accessors (null/0 if not ready).
VkInstance Vk_GetInstance();
VkPhysicalDevice Vk_GetPhysicalDevice();
VkDevice Vk_GetDevice();
VkQueue Vk_GetGraphicsQueue();
uint32_t Vk_GetGraphicsQueueFamily();
VkExtent2D Vk_GetSwapchainExtent();
VkFormat Vk_GetSwapchainFormat();
VkSwapchainKHR Vk_GetSwapchain();
uint32_t Vk_GetSwapchainImageCount();
VkImage Vk_GetSwapchainImage(uint32_t index);
PFN_vkVoidFunction Vk_GetInstanceProc(const char* name);
PFN_vkVoidFunction Vk_GetDeviceProc(const char* name);
// Shared present sync: acquire → caller records cmd → submit+present.
bool Vk_AcquireImage(uint32_t& imageIndex, std::string& error);
// waitStage: COLOR_ATTACHMENT for clear/renderpass presents; TRANSFER for blit-to-swapchain.
bool Vk_SubmitPresent(VkCommandBuffer cmd, uint32_t imageIndex, std::string& error,
                      VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
void Vk_DeviceWaitIdle();
// Wait for the most recently submitted present/graphics fence (not a full device idle).
void Vk_WaitLastSubmit();
