#pragma once
// A4b: host graphics API selection. GX translation stays shared; only the RHI changes.
#include <string>

enum class GfxBackendKind
{
    D3D11 = 0,   // current default (Linux: DXVK; Windows: native D3D11)
    Vulkan = 1,  // native Vulkan (Stage A+: device/swapchain; GX path in later stages)
};

// Parse "d3d11" / "vulkan" (case-insensitive). Empty → default.
GfxBackendKind Gfx_ParseBackend(const char* s);
GfxBackendKind Gfx_SelectedBackend();
void Gfx_SetBackend(GfxBackendKind k);
const char* Gfx_BackendName(GfxBackendKind k);

// Default remains D3D11 until Vulkan GX reaches parity (Stage F).
GfxBackendKind Gfx_DefaultBackend();
