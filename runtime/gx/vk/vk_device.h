#pragma once
// A4b Stage A: native Vulkan instance / device / surface / swapchain.
// Does not route through D3D11 or DXVK. GX draw translation is Stage B+.
#include <cstdint>
#include <string>
#include <vector>

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

// Recreate swapchain after resize (or out-of-date). Safe no-op if not ready.
bool Vk_RecreateSwapchain(std::string& error);

// Clear to rgba and present one frame. Handles acquire / submit / present.
// Returns false on fatal error; out-of-date triggers recreate + retry once.
bool Vk_PresentClear(float r, float g, float b, float a, std::string& error);

// Stage A smoke: create window externally, then call Init + N clear presents.
int Vk_SmokePresentFrames(int frames, const VkInitOptions& opt, std::string& error);
