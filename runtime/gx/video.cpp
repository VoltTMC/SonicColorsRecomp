// D3D11 device and presentation (the window itself is the platform's, platform/window.h).
#include "video.h"
#include "gx_state.h"
#include "../config.h"
#include "../log.h"
#include "../platform/platform.h"
#include "../platform/window.h"
#include "../ios/nand_autosave.h"
#include "../perf.h"
#include "../ui/overlay.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <vector>

VideoConfig g_videoConfig;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;

static IDXGISwapChain1* s_swapchain = nullptr;
static bool s_allowTearing = false;
static ID3D11RenderTargetView* s_backbufferRTV = nullptr;
static ID3D11VertexShader* s_blitVS = nullptr;
static ID3D11PixelShader* s_blitPS = nullptr;
static ID3D11SamplerState* s_linearSampler = nullptr;
static ID3D11Buffer* s_blitCB = nullptr;
static ID3D11RasterizerState* s_blitRS = nullptr;
static ID3D11BlendState* s_blitBS = nullptr;
static ID3D11DepthStencilState* s_blitDS = nullptr;
// UI screenshots of a minimized test window (BKR_UI_SHOT_OVERLAY): drawn off screen
static ID3D11Texture2D* s_shotTex = nullptr;
static ID3D11RenderTargetView* s_shotRTV = nullptr;
// Double-buffered GPU timestamps around the present blit (one frame late when resolved).
static ID3D11Query* s_gpuDisjoint[2]{};
static ID3D11Query* s_gpuTsBegin[2]{};
static ID3D11Query* s_gpuTsEnd[2]{};
static int s_gpuQWrite = 0;
static bool s_gpuQPending[2]{};

static void GpuTimingEnsure()
{
    if (s_gpuDisjoint[0]) return;
    D3D11_QUERY_DESC qd{};
    for (int i = 0; i < 2; i++)
    {
        qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        if (FAILED(g_device->CreateQuery(&qd, &s_gpuDisjoint[i]))) return;
        qd.Query = D3D11_QUERY_TIMESTAMP;
        if (FAILED(g_device->CreateQuery(&qd, &s_gpuTsBegin[i]))) return;
        if (FAILED(g_device->CreateQuery(&qd, &s_gpuTsEnd[i]))) return;
    }
}

static void GpuTimingBegin()
{
    if (!g_config.showFpsOverlay) return;
    GpuTimingEnsure();
    if (!s_gpuDisjoint[0]) return;
    g_ctx->Begin(s_gpuDisjoint[s_gpuQWrite]);
    g_ctx->End(s_gpuTsBegin[s_gpuQWrite]);
}

static void GpuTimingEndAndResolve()
{
    if (!g_config.showFpsOverlay || !s_gpuDisjoint[0]) return;
    g_ctx->End(s_gpuTsEnd[s_gpuQWrite]);
    g_ctx->End(s_gpuDisjoint[s_gpuQWrite]);
    s_gpuQPending[s_gpuQWrite] = true;
    s_gpuQWrite ^= 1;
    const int r = s_gpuQWrite;
    if (!s_gpuQPending[r]) return;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};
    UINT64 t0 = 0, t1 = 0;
    // DONOTFLUSH: return S_FALSE until ready (do not stall the pipe).
    if (g_ctx->GetData(s_gpuDisjoint[r], &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) return;
    if (g_ctx->GetData(s_gpuTsBegin[r], &t0, sizeof(t0), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) return;
    if (g_ctx->GetData(s_gpuTsEnd[r], &t1, sizeof(t1), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) return;
    s_gpuQPending[r] = false;
    if (dj.Disjoint || !dj.Frequency || t1 < t0) return;
    const uint64_t gpuNs = (uint64_t)((double)(t1 - t0) * 1.0e9 / (double)dj.Frequency);
    Perf_NoteGpuNs(gpuNs);
}
static ID3DBlob* Compile(const char* src, const char* entry, const char* target)
{
    ID3DBlob* blob = nullptr;
    ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
    if (FAILED(hr))
    {
        LOG_ERROR("shader compile failed: {}", err ? (const char*)err->GetBufferPointer() : "?");
        if (err) err->Release();
        return nullptr;
    }
    if (err) err->Release();
    return blob;
}

static const char* kBlitHLSL = R"(
cbuffer C : register(b0) { float4 uvRect; float4 dstRect; };
Texture2D tex : register(t0);
SamplerState samp : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V vs(uint id : SV_VertexID) {
    V o;
    float2 t = float2((id << 1) & 2, id & 2) * 0.5;   // 0..1 quad corners (triangle strip of 4)
    t = float2(id & 1, id >> 1);
    o.pos = float4(lerp(dstRect.x, dstRect.z, t.x), lerp(dstRect.y, dstRect.w, t.y), 0, 1);
    o.uv = lerp(uvRect.xy, uvRect.zw, t);
    return o;
}
float4 ps(V i) : SV_Target { return float4(tex.Sample(samp, i.uv).rgb, 1.0); }
)";

static void CreateBackbufferRTV()
{
    ID3D11Texture2D* bb = nullptr;
    s_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
    g_device->CreateRenderTargetView(bb, nullptr, &s_backbufferRTV);
    bb->Release();
}

static std::string GpuAdapterId(const LUID& luid)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "luid:%ld:%lu", (long)luid.HighPart, (unsigned long)luid.LowPart);
    return buf;
}

static bool ParseGpuAdapterId(const std::string& id, LUID& out)
{
    long high = 0;
    unsigned long low = 0;
    if (std::sscanf(id.c_str(), "luid:%ld:%lu", &high, &low) != 2) return false;
    out.HighPart = (LONG)high;
    out.LowPart = (DWORD)low;
    return true;
}

std::vector<VideoGpuAdapter> Video_ListGpuAdapters()
{
    std::vector<VideoGpuAdapter> out;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory)) || !factory) return out;
    for (UINT i = 0;; i++)
    {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc)))
        {
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
                out.push_back({ Platform_Utf8(desc.Description), GpuAdapterId(desc.AdapterLuid) });
        }
        adapter->Release();
    }
    factory->Release();
    return out;
}

static IDXGIAdapter1* SelectAdapter()
{
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory)) || !factory) return nullptr;
    LUID want{};
    const bool wantSpecific = ParseGpuAdapterId(g_config.gpuAdapter, want);
    IDXGIAdapter1* chosen = nullptr;
    for (UINT i = 0;; i++)
    {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc)))
        {
            adapter->Release();
            continue;
        }
        if (wantSpecific && desc.AdapterLuid.HighPart == want.HighPart && desc.AdapterLuid.LowPart == want.LowPart)
        {
            chosen = adapter;
            break;
        }
        if (!chosen && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
            chosen = adapter;
        else
            adapter->Release();
    }
    factory->Release();
    if (wantSpecific && !chosen)
        LOG_WARN("Configured GPU adapter {} not found; using automatic adapter", g_config.gpuAdapter);
    return chosen;
}

bool Video_Init()
{
    Platform_LogHost();
    if (!Window_Create(g_videoConfig.windowWidth, g_videoConfig.windowHeight, g_videoConfig.fullscreen)) return false;
    if (!g_videoConfig.fullscreen && g_config.windowMaximized)
        Window_SetMaximized(true);
    HWND hwnd = (HWND)Window_Handle();

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    IDXGIAdapter1* selected = SelectAdapter();
    D3D_DRIVER_TYPE driverType = selected ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(selected, driverType, nullptr, flags, &fl, 1, D3D11_SDK_VERSION, &g_device, nullptr, &g_ctx);
    if (FAILED(hr))
    {
        LOG_ERROR("D3D11CreateDevice failed {:08x}", (uint32_t)hr);
        if (selected) selected->Release();
        return false;
    }
    IDXGIDevice* dxgiDev = nullptr;
    g_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev);
    IDXGIAdapter* adapter = nullptr;
    dxgiDev->GetAdapter(&adapter);
    IDXGIFactory2* factory = nullptr;
    adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory);
    DXGI_ADAPTER_DESC ad;
    adapter->GetDesc(&ad);
    LOG_INFO("D3D11 adapter: {} (feature level {:x})", Platform_Utf8(ad.Description), (uint32_t)g_device->GetFeatureLevel());
    if (selected) selected->Release();

    IDXGIFactory5* factory5 = nullptr;
    if (SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory5), (void**)&factory5)))
    {
        BOOL allow = FALSE;
        if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))))
            s_allowTearing = allow != FALSE;
        factory5->Release();
    }

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = 0;
    sd.Height = 0;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (s_allowTearing)
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    hr = factory->CreateSwapChainForHwnd(g_device, hwnd, &sd, nullptr, nullptr, &s_swapchain);
    if (FAILED(hr) && s_allowTearing)
    {
        // Fallback without tearing if the compositor rejects the flag.
        sd.Flags = 0;
        s_allowTearing = false;
        hr = factory->CreateSwapChainForHwnd(g_device, hwnd, &sd, nullptr, nullptr, &s_swapchain);
    }
    if (FAILED(hr))
    {
        LOG_ERROR("CreateSwapChainForHwnd failed {:08x}", (uint32_t)hr);
        return false;
    }
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    factory->Release();
    adapter->Release();
    dxgiDev->Release();
    CreateBackbufferRTV();
    {
        const float hz = Window_DisplayRefreshHz();
        LOG_INFO("present: vsync={} tearing={} (cap={}) display_refresh={:.0f} Hz (guest VI is independent)",
                 g_videoConfig.vsync, s_allowTearing && !g_videoConfig.vsync, s_allowTearing, hz);
    }

    ID3DBlob* vs = Compile(kBlitHLSL, "vs", "vs_5_0");
    ID3DBlob* ps = Compile(kBlitHLSL, "ps", "ps_5_0");
    if (!vs || !ps)
    {
        LOG_ERROR("present shaders failed to compile");
        return false;
    }
    g_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &s_blitVS);
    g_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &s_blitPS);
    vs->Release();
    ps->Release();
    D3D11_SAMPLER_DESC smp{};
    smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.MaxLOD = D3D11_FLOAT32_MAX;
    g_device->CreateSamplerState(&smp, &s_linearSampler);
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = 32;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g_device->CreateBuffer(&bd, nullptr, &s_blitCB);
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_NONE;
    g_device->CreateRasterizerState(&rs, &s_blitRS);
    D3D11_BLEND_DESC bs{};
    bs.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_device->CreateBlendState(&bs, &s_blitBS);
    D3D11_DEPTH_STENCIL_DESC ds{};
    g_device->CreateDepthStencilState(&ds, &s_blitDS);
    Overlay_Init(hwnd, g_device, g_ctx);
    return true;
}

void Video_Present(ID3D11ShaderResourceView* srv, float u0, float v0, float u1, float v1)
{
    extern void Perf_OnPresentBegin();
    extern void Perf_OnPresentEnd();
    Perf_OnPresentBegin();
    if (Window_TakeResized())
    {
        g_ctx->OMSetRenderTargets(0, nullptr, nullptr);
        if (s_backbufferRTV) s_backbufferRTV->Release();
        s_swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN,
                                   s_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
        CreateBackbufferRTV();
    }
    int cw, ch;
    Window_ClientSize(cw, ch);
    float w = (float)std::max(1, cw);
    float h = (float)std::max(1, ch);
    ID3D11RenderTargetView* target = s_backbufferRTV;
    if (Overlay_ShotPending() && w < 64)
    {
        if (!s_shotTex)
        {
            D3D11_TEXTURE2D_DESC td{};
            td.Width = 1280; td.Height = 720; td.MipLevels = 1; td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.BindFlags = D3D11_BIND_RENDER_TARGET;
            g_device->CreateTexture2D(&td, nullptr, &s_shotTex);
            g_device->CreateRenderTargetView(s_shotTex, nullptr, &s_shotRTV);
        }
        target = s_shotRTV;
        w = 1280;
        h = 720;
    }
    D3D11_VIEWPORT vp{ 0, 0, w, h, 0, 1 };
    g_ctx->RSSetViewports(1, &vp);
    float black[4] = { 0, 0, 0, 1 };
    g_ctx->OMSetRenderTargets(1, &target, nullptr);
    g_ctx->ClearRenderTargetView(target, black);
    GpuTimingBegin();
    if (srv)
    {
        // letterbox to target aspect (optional integer scale keeps sharp pixels)
        float aspect = g_videoConfig.widescreen ? 16.0f / 9.0f : 4.0f / 3.0f;
        float dw = w, dh = h;
        if (w / h > aspect) dw = h * aspect;
        else dh = w / aspect;
        if (g_videoConfig.integerScale)
        {
            const float baseW = 640.0f;
            const float baseH = baseW / aspect;
            const int scale = std::max(1, (int)std::floor(std::min(dw / baseW, dh / baseH)));
            dw = baseW * (float)scale;
            dh = baseH * (float)scale;
        }
        float x0 = (w - dw) / 2, y0 = (h - dh) / 2;
        float cb[8] = { u0, v0, u1, v1,
                        x0 / w * 2 - 1, 1 - y0 / h * 2, (x0 + dw) / w * 2 - 1, 1 - (y0 + dh) / h * 2 };
        D3D11_MAPPED_SUBRESOURCE m;
        g_ctx->Map(s_blitCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
        memcpy(m.pData, cb, sizeof(cb));
        g_ctx->Unmap(s_blitCB, 0);
        g_ctx->IASetInputLayout(nullptr);
        g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        g_ctx->VSSetShader(s_blitVS, nullptr, 0);
        g_ctx->VSSetConstantBuffers(0, 1, &s_blitCB);
        g_ctx->PSSetShader(s_blitPS, nullptr, 0);
        g_ctx->PSSetShaderResources(0, 1, &srv);
        g_ctx->PSSetSamplers(0, 1, &s_linearSampler);
        g_ctx->RSSetState(s_blitRS);
        g_ctx->OMSetBlendState(s_blitBS, nullptr, 0xFFFFFFFF);
        g_ctx->OMSetDepthStencilState(s_blitDS, 0);
        g_ctx->GSSetShader(nullptr, nullptr, 0);
        g_ctx->Draw(4, 0);
        ID3D11ShaderResourceView* nullSrv = nullptr;
        g_ctx->PSSetShaderResources(0, 1, &nullSrv);
    }
    // A3x: blit + overlay replace IA/VS/PS/OM; Flush must not reuse A3t bind cache.
    Render_InvalidateHostBindCache();
    Overlay_Render(w, h);   // in-game settings, drawn over the game while it keeps running
    Render_InvalidateHostBindCache();
    NandAutosave_Tick();
    GpuTimingEndAndResolve();
    // sync=1 follows the *host* display refresh (60/120/165/…), not emulated VI.
    // VSync off: prefer ALLOW_TEARING when supported so presentation is not FIFO-capped.
    const UINT sync = g_videoConfig.vsync ? 1u : 0u;
    const UINT flags = (!g_videoConfig.vsync && s_allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
    HRESULT hr = s_swapchain->Present(sync, flags);
    if (FAILED(hr))
    {
        static int failures = 0;
        if (failures++ < 5)
            LOG_ERROR("Present failed {:08x} (device removed reason {:08x})", (uint32_t)hr, (uint32_t)g_device->GetDeviceRemovedReason());
    }
    Perf_OnPresentEnd();
}

void Video_SetFullscreen(bool on) { Window_SetFullscreen(on); }
void Video_SetWindowSize(int w, int h) { Window_SetSize(w, h); }
float Video_DisplayRefreshHz() { return Window_DisplayRefreshHz(); }
bool Video_VsyncEnabled() { return g_videoConfig.vsync; }
bool Video_AllowTearing() { return s_allowTearing && !g_videoConfig.vsync; }
