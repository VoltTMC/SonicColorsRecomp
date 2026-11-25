// A4k: HLSL vs GLSL fog/lighting generators must stay formula-aligned for TR1 configs.
#include "gx/shadergen.h"
#include "gx/shadergen_glsl.h"
#include "gx/vertex_loader.h"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(c))                                                                                                      \
        {                                                                                                              \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                    \
            g_fail++;                                                                                                  \
        }                                                                                                              \
    } while (0)

static bool Contains(const std::string& s, const char* needle) { return s.find(needle) != std::string::npos; }

static void TestFogLinearPersp()
{
    PSKey k{};
    k.numTevStages = 1;
    k.fogType = 2; // linear, orthographic/persp bit 0, no range
    std::string hl = GeneratePixelShader(k);
    std::string gl = GeneratePixelShaderGLSL(k);
    CHECK(Contains(hl, "fogF.x * 16777216.0"));
    CHECK(Contains(gl, "ps.fogF.x * 16777216.0"));
    CHECK(Contains(hl, "clamp(ze - fogF.y"));
    CHECK(Contains(gl, "clamp(ze - ps.fogF.y"));
    CHECK(!Contains(hl, "SV_Position") || Contains(hl, "i.pos.x") || !Contains(hl, "offset ="));
}

static void TestFogWithRange()
{
    PSKey k{};
    k.numTevStages = 1;
    k.fogType = 2 | (1u << 4); // linear + range adj
    std::string hl = GeneratePixelShader(k);
    std::string gl = GeneratePixelShaderGLSL(k);
    CHECK(Contains(hl, "i.pos.x / fogF.w"));
    CHECK(Contains(gl, "gl_FragCoord.x / ps.fogF.w")); // A4i parity with SV_Position.x
    CHECK(Contains(hl, "sqrt(offset * offset + kk * kk)"));
    CHECK(Contains(gl, "sqrt(offset * offset + kk * kk)"));
    CHECK(!Contains(gl, "inClipFinal.x")); // regression: must not use clip-space X
}

static void TestFogExpModes()
{
    for (uint32_t fsel : { 4u, 5u, 6u, 7u })
    {
        PSKey k{};
        k.numTevStages = 1;
        k.fogType = fsel;
        std::string hl = GeneratePixelShader(k);
        std::string gl = GeneratePixelShaderGLSL(k);
        CHECK(Contains(hl, "exp2(-8.0"));
        CHECK(Contains(gl, "exp2(-8.0"));
    }
}

static void TestLightingSpotDiffuse()
{
    VSKey k{};
    k.components = VC_NRM | VC_COL0;
    k.numColorChans = 1;
    k.chan[0] = 0x2 | (0x01u << 2) | (2u << 7) | (3u << 9); // light0, diffuse clamp, spot
    std::string hl = GenerateVertexShader(k);
    std::string gl = GenerateVertexShaderGLSL(k);
    CHECK(Contains(hl, "dot(ldir, xf3(") || Contains(hl, "attn = max(0.0, xf["));
    CHECK(Contains(gl, "dot(ldir, xf3(") || Contains(gl, "attn = max(0.0, xf["));
    CHECK(Contains(hl, "max(0.0, ") || Contains(hl, "lacc."));
    CHECK(Contains(gl, "ivec3(") || Contains(gl, "int("));
    CHECK(!Contains(gl, "float3("));
    CHECK(!Contains(gl, "int3("));
}

// TR1-class: matsrc/ambsrc + signed diffuse (common outdoor materials).
static void TestLightingMatsrcAmb()
{
    VSKey k{};
    k.components = VC_NRM | VC_COL0;
    k.numColorChans = 1;
    // lighting enable, light0+1, diffuse=2 (signed), attn=1 (spec/dir)
    k.chan[0] = 0x2 | (0x03u << 2) | (2u << 7) | (1u << 9) | (1u << 11) | (0u << 13);
    std::string hl = GenerateVertexShader(k);
    std::string gl = GenerateVertexShaderGLSL(k);
    CHECK(Contains(hl, "lacc.") || Contains(hl, "ambColor") || Contains(hl, "matColor"));
    CHECK(Contains(gl, "lacc.") || Contains(gl, "ambColor") || Contains(gl, "matColor"));
    CHECK(Contains(gl, "ivec3(") || Contains(gl, "int("));
    CHECK(!Contains(gl, "float3("));
}

static void TestFogLinearPerspProjBit()
{
    // fsel=2 + proj bit (orthographic ze path) — rare/absent in TR1 but must stay aligned.
    PSKey k{};
    k.numTevStages = 1;
    k.fogType = 2 | (1u << 3);
    std::string hl = GeneratePixelShader(k);
    std::string gl = GeneratePixelShaderGLSL(k);
    CHECK(Contains(hl, "fogF.x * float(zCoord)"));
    CHECK(Contains(gl, "ps.fogF.x * float(zCoord)"));
}

int main()
{
    TestFogLinearPersp();
    TestFogWithRange();
    TestFogExpModes();
    TestFogLinearPerspProjBit();
    TestLightingSpotDiffuse();
    TestLightingMatsrcAmb();
    if (g_fail)
    {
        std::printf("%d checks failed\n", g_fail);
        return 1;
    }
    std::puts("scr_fog_light_parity_test OK");
    return 0;
}
