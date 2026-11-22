#pragma once
#include <cstdint>
#include <d3d11.h>
#include <string>
#include <vector>

struct VideoConfig
{
    int windowWidth = 1280;
    int windowHeight = 720;
    int internalScale = 3;     // EFB resolution multiplier
    bool vsync = false;        // the game paces itself from VI interrupts
    bool widescreen = true;    // present 16:9
    bool integerScale = false; // present at integer multiples of 640×(360|480); letterbox remainder
    bool fullscreen = false;
};

struct VideoGpuAdapter
{
    std::string name;   // display name
    std::string id;     // stable config value ("luid:HIGH:LOW")
};

extern VideoConfig g_videoConfig;
extern ID3D11Device* g_device;
extern ID3D11DeviceContext* g_ctx;

bool Video_Init();
std::vector<VideoGpuAdapter> Video_ListGpuAdapters();
// Presents the given texture region (an XFB copy) to the window.
void Video_Present(ID3D11ShaderResourceView* srv, float u0, float v0, float u1, float v1);
// Applied on the window thread (settings changed while the game runs)
void Video_SetFullscreen(bool on);
void Video_SetWindowSize(int width, int height);
// Host monitor refresh (Hz) for the game window; 0 if unknown. Independent of guest VI.
float Video_DisplayRefreshHz();
bool Video_VsyncEnabled();
bool Video_AllowTearing();   // DXGI ALLOW_TEARING when VSync is off (may be false)
