#include "gfx_backend.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

static GfxBackendKind s_backend = GfxBackendKind::D3D11;
static bool s_set = false;

GfxBackendKind Gfx_DefaultBackend()
{
    return GfxBackendKind::D3D11;
}

GfxBackendKind Gfx_ParseBackend(const char* s)
{
    if (!s || !s[0]) return Gfx_DefaultBackend();
    char buf[32]{};
    size_t n = 0;
    for (; s[n] && n + 1 < sizeof(buf); n++)
        buf[n] = (char)std::tolower((unsigned char)s[n]);
    buf[n] = 0;
    if (std::strcmp(buf, "vulkan") == 0 || std::strcmp(buf, "vk") == 0)
        return GfxBackendKind::Vulkan;
    if (std::strcmp(buf, "d3d11") == 0 || std::strcmp(buf, "d3d") == 0 || std::strcmp(buf, "dx11") == 0)
        return GfxBackendKind::D3D11;
    return Gfx_DefaultBackend();
}

void Gfx_SetBackend(GfxBackendKind k)
{
    s_backend = k;
    s_set = true;
}

GfxBackendKind Gfx_SelectedBackend()
{
    if (!s_set)
    {
        if (const char* e = std::getenv("SCR_RENDERER"))
            s_backend = Gfx_ParseBackend(e);
        else
            s_backend = Gfx_DefaultBackend();
        s_set = true;
    }
    return s_backend;
}

const char* Gfx_BackendName(GfxBackendKind k)
{
    switch (k)
    {
    case GfxBackendKind::Vulkan: return "vulkan";
    case GfxBackendKind::D3D11:
    default: return "d3d11";
    }
}
