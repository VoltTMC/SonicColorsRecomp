// A4f: controlled checks for EFB-copy format conversion, mip chains, and GLSL VS lighting types.
#include "../../runtime/gx/shadergen.h"
#include "../../runtime/gx/shadergen_glsl.h"
#include "../../runtime/gx/vertex_loader.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            g_fail++;                                                                                                  \
        }                                                                                                              \
    } while (0)

struct Vec4
{
    float r, g, b, a;
};

static Vec4 V4(float r, float g, float b, float a) { return { r, g, b, a }; }
static bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
static bool Near4(Vec4 a, Vec4 b)
{
    return Near(a.r, b.r) && Near(a.g, b.g) && Near(a.b, b.b) && Near(a.a, b.a);
}

// CPU reference matching D3D11 pscopy / Vulkan kCopyFragGlsl colour path (no depth).
static Vec4 CopyConvertColor(Vec4 c, int fmt, bool intensity, bool halfIgnored)
{
    (void)halfIgnored;
    if (intensity)
    {
        float y = c.r * 0.257f + c.g * 0.504f + c.b * 0.098f + 16.0f / 255.0f;
        c.r = c.g = c.b = y;
    }
    switch (fmt)
    {
    case 0:
    case 1:
        return V4(c.r, c.r, c.r, c.r);
    case 2:
    case 3:
        return V4(c.r, c.r, c.r, c.a);
    case 4:
        c.a = 1.0f;
        return c;
    case 7:
        return V4(c.a, c.a, c.a, c.a);
    case 8:
        return V4(c.r, c.r, c.r, c.r);
    case 9:
        return V4(c.g, c.g, c.g, c.g);
    case 10:
        return V4(c.b, c.b, c.b, c.b);
    case 11:
        return V4(c.r, c.r, c.r, c.g);
    case 12:
        return V4(c.g, c.g, c.g, c.b);
    default:
        return c;
    }
}

static Vec4 CopyConvertDepth(float d, int fmt)
{
    uint32_t z = (uint32_t)(std::fmin(1.0f, std::fmax(0.0f, d)) * 16777215.0f);
    float h = float((z >> 16) & 0xFF) / 255.0f;
    float m = float((z >> 8) & 0xFF) / 255.0f;
    float l = float(z & 0xFF) / 255.0f;
    switch (fmt)
    {
    case 0:
    case 1:
    case 8:
        return V4(h, h, h, h);
    case 9:
        return V4(m, m, m, m);
    case 10:
        return V4(l, l, l, l);
    case 3:
        return V4(m, m, m, h);
    case 11:
    case 12:
        return V4(m, l, l, m);
    default:
        return V4(h, m, l, 1.0f);
    }
}

static void MipDims(uint32_t w, uint32_t h, uint32_t maxLod, uint32_t minF, uint32_t& outLevels,
                    std::vector<uint32_t>& ws, std::vector<uint32_t>& hs)
{
    uint32_t levels = 1;
    if (minF >= 2 && maxLod > 0)
    {
        uint32_t lw = w, lh = h;
        while (levels <= maxLod && (lw > 1 || lh > 1))
        {
            lw = std::max(1u, lw / 2);
            lh = std::max(1u, lh / 2);
            levels++;
        }
    }
    levels = std::min(levels, 16u);
    outLevels = levels;
    ws.clear();
    hs.clear();
    uint32_t lw = w, lh = h;
    for (uint32_t i = 0; i < levels; i++)
    {
        ws.push_back(lw);
        hs.push_back(lh);
        lw = std::max(1u, lw / 2);
        lh = std::max(1u, lh / 2);
    }
}

static bool CompileGlslVert(const std::string& src, std::string& err)
{
    const char* glslang = std::getenv("SCR_GLSLANG");
    if (!glslang || !*glslang) glslang = "glslangValidator";
    const char* tmpIn = "/tmp/a4f_efb_test_vs.glsl";
    const char* tmpOut = "/tmp/a4f_efb_test_vs.spv";
    {
        std::ofstream f(tmpIn, std::ios::binary);
        if (!f) { err = "open tmp failed"; return false; }
        f << src;
    }
    std::string cmd = std::string(glslang) + " -V -S vert -e main " + tmpIn + " -o " + tmpOut + " 2>&1";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) { err = "popen failed"; return false; }
    char buf[512];
    std::string out;
    while (fgets(buf, sizeof(buf), p)) out += buf;
    int rc = pclose(p);
    if (rc != 0)
    {
        err = out.empty() ? "glslangValidator failed" : out;
        return false;
    }
    return true;
}

int main()
{
    // --- colour format conversion (known RGBA sample) ---
    Vec4 src = V4(0.2f, 0.4f, 0.6f, 0.8f);
    CHECK(Near4(CopyConvertColor(src, 6, false, false), src)); // RGBA8 default
    CHECK(Near4(CopyConvertColor(src, 4, false, false), V4(0.2f, 0.4f, 0.6f, 1.0f)));
    CHECK(Near4(CopyConvertColor(src, 7, false, false), V4(0.8f, 0.8f, 0.8f, 0.8f)));
    CHECK(Near4(CopyConvertColor(src, 9, false, false), V4(0.4f, 0.4f, 0.4f, 0.4f)));
    CHECK(Near4(CopyConvertColor(src, 11, false, false), V4(0.2f, 0.2f, 0.2f, 0.4f)));
    CHECK(Near4(CopyConvertColor(src, 2, false, false), V4(0.2f, 0.2f, 0.2f, 0.8f)));
    {
        float y = 0.2f * 0.257f + 0.4f * 0.504f + 0.6f * 0.098f + 16.0f / 255.0f;
        CHECK(Near4(CopyConvertColor(src, 0, true, false), V4(y, y, y, y)));
    }

    // --- depth packing ---
    {
        Vec4 d = CopyConvertDepth(0.5f, 6);
        CHECK(d.r >= 0.0f && d.r <= 1.0f);
        CHECK(Near4(CopyConvertDepth(0.0f, 8), V4(0, 0, 0, 0)));
    }

    // --- mip dimension chains ---
    {
        uint32_t levels = 0;
        std::vector<uint32_t> ws, hs;
        MipDims(64, 32, 8, 5, levels, ws, hs);
        CHECK(levels == 7); // 64x32 → 1x1
        CHECK(ws[0] == 64 && hs[0] == 32);
        CHECK(ws.back() == 1 && hs.back() == 1);
        MipDims(64, 32, 8, 1, levels, ws, hs); // linear (no mip)
        CHECK(levels == 1);
        MipDims(16, 16, 2, 2, levels, ws, hs);
        CHECK(levels == 3);
        CHECK(ws[1] == 8 && ws[2] == 4);
    }

    // --- half-scale destination size ---
    {
        int sw = 640, sh = 528;
        bool half = true;
        uint32_t tw = half ? (uint32_t)std::max(1, sw / 2) : (uint32_t)sw;
        uint32_t th = half ? (uint32_t)std::max(1, sh / 2) : (uint32_t)sh;
        CHECK(tw == 320 && th == 264);
    }

    // --- GX Z-mode compare encoding (matches D3D11 funcs[] / Vulkan GxDepthCompareToVk) ---
    {
        // zm bits: enable | compare<<1 | update<<4
        auto cmp = [](uint32_t zm) { return (zm >> 1) & 7; };
        CHECK(cmp(0x17) == 3); // enable + <= + update (common)
        CHECK(cmp(0x1B) == 5); // !=
        CHECK(cmp(0x1D) == 6); // >=
        CHECK(cmp(0x1F) == 7); // always
    }

    // --- VS GLSL with colour+alpha lighting must compile (catches int3/float3 bug) ---
    {
        VSKey k{};
        k.components = VC_NRM | VC_COL0 | VC_TEX0 | (VC_TEX0 << 1);
        k.numTexGens = 2;
        k.numColorChans = 1;
        k.dualTex = 1;
        // Enable colour+alpha lighting with spot attenuation (attn=3) + diffuse clamp (2).
        k.chan[0] = 0x2 | (0x01u << 2) | (2u << 7) | (3u << 9); // colour light0, diffuse2, spot
        k.chan[2] = 0x2 | (0x04u << 2) | (2u << 7) | (0u << 9); // alpha light2
        k.texMtxInfo[0] = 0;
        k.texMtxInfo[1] = 0;
        std::string glsl = GenerateVertexShaderGLSL(k);
        CHECK(glsl.find("void main()") != std::string::npos);
        CHECK(glsl.find("ivec3(") != std::string::npos || glsl.find("int(") != std::string::npos);
        CHECK(glsl.find("float3(") == std::string::npos);
        CHECK(glsl.find("int3(") == std::string::npos);
        std::string err;
        bool ok = CompileGlslVert(glsl, err);
        if (!ok) fprintf(stderr, "SPIR-V compile failed:\n%s\n", err.c_str());
        CHECK(ok);
    }

    // --- PS GLSL entry point present ---
    {
        PSKey pk{};
        pk.numTevStages = 1;
        pk.numTexGens = 1;
        std::string pglsl = GeneratePixelShaderGLSL(pk);
        CHECK(pglsl.find("void main()") != std::string::npos);
    }

    if (g_fail)
    {
        fprintf(stderr, "%d checks failed\n", g_fail);
        return 1;
    }
    printf("scr_efb_copy_tex_test OK\n");
    return 0;
}
