// Vulkan GLSL 450 generation for the GX transform/lighting unit and TEV pixel pipeline.
#include "shadergen_glsl.h"
#include "vertex_loader.h"

#include <format>

static void W(std::string& s, const std::string& t) { s += t; }

// ---------------------------------------------------------------------- vertex shader
static const char* kVSCommonGLSL = R"(
#version 450
layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNrm;
layout(location=2) in vec3 inBin;
layout(location=3) in vec3 inTan;
layout(location=4) in vec4 inCol0;
layout(location=5) in vec4 inCol1;
layout(location=6) in vec2 inTex0;
layout(location=7) in vec2 inTex1;
layout(location=8) in vec2 inTex2;
layout(location=9) in vec2 inTex3;
layout(location=10) in vec2 inTex4;
layout(location=11) in vec2 inTex5;
layout(location=12) in vec2 inTex6;
layout(location=13) in vec2 inTex7;
layout(location=14) in uvec3 inMtx;

layout(set=0, binding=0) readonly buffer XF { float xf[]; };
layout(set=0, binding=1) uniform VSConst
{
    layout(row_major) mat4 proj;
    vec4 depthp;
    ivec4 materials[4];
    vec4 pixelOffset;
} ubo;

vec4 xrow(uint r) { uint b = r * 4u; return vec4(xf[b], xf[b + 1u], xf[b + 2u], xf[b + 3u]); }
vec3 nrow(uint b) { return vec3(xf[b], xf[b + 1u], xf[b + 2u]); }
vec3 xf3(uint b) { return vec3(xf[b], xf[b + 1u], xf[b + 2u]); }
uint xfu(uint b) { return floatBitsToUint(xf[b]); }
bool isNaN(float x) { return (floatBitsToUint(x) & 0x7FFFFFFFu) > 0x7F800000u; }
ivec4 lightColor(uint i) { uint c = xfu(0x600u + i * 16u + 3u); return ivec4(int(c >> 24), int((c >> 16) & 0xFFu), int((c >> 8) & 0xFFu), int(c & 0xFFu)); }
)";

static void GenLightGLSL(std::string& s, uint32_t chanReg, int light, bool alpha)
{
    uint32_t diffuse = (chanReg >> 7) & 3;
    uint32_t attn = (chanReg >> 9) & 3;
    std::string swz = alpha ? "a" : "rgb";
    std::string cmp = alpha ? "" : "3";
    std::string b = std::format("0x{:X}", 0x600 + light * 16);
    if (attn == 1)
    {
        W(s, std::format("\t\tldir = normalize(xf3({0} + 10) - pos.xyz);\n", b));
        W(s, std::format("\t\tattn = (dot(n, ldir) >= 0.0) ? max(0.0, dot(n, xf3({0} + 13))) : 0.0;\n", b));
        W(s, std::format("\t\tcosAttn = xf3({0} + 4);\n", b));
        W(s, std::format("\t\tdistAttn = {1}(xf3({0} + 7));\n", b, diffuse == 0 ? "" : "normalize"));
        W(s, "\t\tattn = max(0.0, dot(cosAttn, vec3(1.0, attn, attn * attn))) / dot(distAttn, vec3(1.0, attn, attn * attn));\n");
    }
    else if (attn == 3)
    {
        W(s, std::format("\t\tldir = xf3({0} + 10) - pos.xyz;\n", b));
        W(s, "\t\tdist2 = dot(ldir, ldir); dist = sqrt(dist2); ldir = ldir / dist;\n");
        W(s, std::format("\t\tattn = max(0.0, dot(ldir, xf3({0} + 13)));\n", b));
        W(s, std::format("\t\tattn = max(0.0, xf[{0} + 4] + xf[{0} + 5] * attn + xf[{0} + 6] * attn * attn) / dot(xf3({0} + 7), vec3(1.0, dist, dist2));\n", b));
    }
    else
    {
        W(s, std::format("\t\tldir = normalize(xf3({0} + 10) - pos.xyz);\n", b));
        W(s, "\t\tattn = 1.0;\n");
        W(s, "\t\tif (length(ldir) == 0.0) ldir = n;\n");
    }
    if (diffuse == 0)
        W(s, std::format("\t\tlacc.{0} += ivec{1}(round(attn * vec{1}(lightColor({2}).{0})));\n", swz, cmp, light));
    else
        W(s, std::format("\t\tlacc.{0} += ivec{1}(round(attn * {3}dot(ldir, n)) * vec{1}(lightColor({2}).{0})));\n", swz, cmp, light,
                         diffuse == 2 ? "max(0.0, " : "("));
}

static void GenLightingChannelGLSL(std::string& s, const VSKey& k, int j)
{
    uint32_t c = k.chan[j];
    uint32_t a = k.chan[j + 2];
    W(s, std::format("vec4 lighting{}(vec4 base, vec3 pos, vec3 n)\n{{\n", j));
    W(s, "\tivec4 lacc; vec3 ldir, cosAttn, distAttn; float dist, dist2, attn;\n");
    bool cMatVtx = c & 1, aMatVtx = a & 1;
    W(s, cMatVtx ? "\tivec4 mat = ivec4(round(base * 255.0));\n" : std::format("\tivec4 mat = ubo.materials[{}];\n", j + 2));
    if (c & 2)
        W(s, (c & 0x40) ? "\tlacc = ivec4(round(base * 255.0));\n" : std::format("\tlacc = ubo.materials[{}];\n", j));
    else
        W(s, "\tlacc = ivec4(255, 255, 255, 255);\n");
    if (aMatVtx != cMatVtx)
        W(s, aMatVtx ? "\tmat.w = int(round(base.w * 255.0));\n" : std::format("\tmat.w = ubo.materials[{}].w;\n", j + 2));
    if (a & 2)
        W(s, (a & 0x40) ? "\tlacc.w = int(round(base.w * 255.0));\n" : std::format("\tlacc.w = ubo.materials[{}].w;\n", j));
    else
        W(s, "\tlacc.w = 255;\n");
    auto mask = [](uint32_t r) { return ((r >> 2) & 0xF) | (((r >> 11) & 0xF) << 4); };
    if (c & 2)
        for (int i = 0; i < 8; i++)
            if (mask(c) & (1 << i)) { W(s, "\t{\n"); GenLightGLSL(s, c, i, false); W(s, "\t}\n"); }
    if (a & 2)
        for (int i = 0; i < 8; i++)
            if (mask(a) & (1 << i)) { W(s, "\t{\n"); GenLightGLSL(s, a, i, true); W(s, "\t}\n"); }
    W(s, "\tlacc = clamp(lacc, 0, 255);\n");
    W(s, "\treturn vec4((mat * (lacc + (lacc >> 7))) >> 8) / 255.0;\n}\n");
}

std::string GenerateVertexShaderGLSL(const VSKey& k)
{
    std::string s = kVSCommonGLSL;
    W(s, "layout(location=0) out vec4 outCol0;\n");
    W(s, "layout(location=1) out vec4 outCol1;\n");
    W(s, "layout(location=2) out vec4 outClipPos;\n");
    for (uint32_t i = 0; i < k.numTexGens; i++)
        W(s, std::format("layout(location={}) out vec3 outTex{};\n", 3 + i, i));
    const uint32_t clipFinalLoc = 3 + k.numTexGens;
    W(s, std::format("layout(location={}) out vec4 outClipFinal;\n", clipFinalLoc));

    GenLightingChannelGLSL(s, k, 0);
    GenLightingChannelGLSL(s, k, 1);

    W(s, "void main()\n{\n");
    W(s, "\tuint mtx[9] = uint[9](inMtx.x & 0xFFu, (inMtx.x >> 8) & 0xFFu, (inMtx.x >> 16) & 0xFFu, inMtx.x >> 24, inMtx.y & 0xFFu, (inMtx.y >> 8) & 0xFFu, (inMtx.y >> 16) & 0xFFu, inMtx.y >> 24, inMtx.z & 0xFFu);\n");
    W(s, "\tvec4 rawpos = vec4(inPos, 1.0);\n");
    W(s, "\tuint pidx = mtx[0];\n");
    W(s, "\tvec4 pos = vec4(dot(xrow(pidx), rawpos), dot(xrow(pidx + 1u), rawpos), dot(xrow(pidx + 2u), rawpos), 1.0);\n");
    W(s, "\tuint nb = 0x400u + (pidx & 31u) * 3u;\n");
    bool hasN = k.components & VC_NRM;
    if (hasN)
    {
        W(s, "\tvec3 n = normalize(vec3(dot(nrow(nb), inNrm), dot(nrow(nb + 3u), inNrm), dot(nrow(nb + 6u), inNrm)));\n");
        W(s, "\tvec3 _t = vec3(dot(nrow(nb), inTan), dot(nrow(nb + 3u), inTan), dot(nrow(nb + 6u), inTan));\n");
        W(s, "\tvec3 _b = vec3(dot(nrow(nb), inBin), dot(nrow(nb + 3u), inBin), dot(nrow(nb + 6u), inBin));\n");
    }
    else
        W(s, "\tvec3 n = vec3(0, 0, 1);\n\tvec3 _t = vec3(0, 0, 0);\n\tvec3 _b = vec3(0, 0, 0);\n");

    bool c0 = k.components & VC_COL0, c1 = k.components & VC_COL1;
    W(s, std::format("\tvec4 vc0 = {};\n", c0 ? "inCol0" : (c1 ? "inCol1" : "vec4(1,1,1,1)")));
    W(s, std::format("\tvec4 vc1 = {};\n", (c0 && c1) ? "inCol1" : "vec4(1,1,1,1)"));
    W(s, "\toutCol0 = lighting0(vc0, pos.xyz, n);\n");
    W(s, "\toutCol1 = lighting1(vc1, pos.xyz, n);\n");
    if (k.numColorChans == 0) W(s, "\toutCol0 = vec4(0,0,0,0);\n");
    if (k.numColorChans < 2) W(s, "\toutCol1 = vec4(0,0,0,0);\n");

    for (uint32_t i = 0; i < k.numTexGens; i++)
    {
        uint32_t info = k.texMtxInfo[i];
        uint32_t projection = (info >> 1) & 1;
        uint32_t inputForm = (info >> 2) & 1;
        uint32_t type = (info >> 4) & 7;
        uint32_t srcRow = (info >> 7) & 31;
        uint32_t embossSrc = (info >> 12) & 7;
        uint32_t embossLight = (info >> 15) & 7;
        W(s, "\t{\n\t\tvec4 coord = vec4(0.0, 0.0, 1.0, 1.0);\n");
        switch (srcRow)
        {
        case 0: W(s, "\t\tcoord.xyz = rawpos.xyz;\n"); break;
        case 1: if (hasN) W(s, "\t\tcoord.xyz = inNrm;\n"); break;
        case 2: break;
        case 3: if (k.components & VC_NBT) W(s, "\t\tcoord.xyz = inTan;\n"); break;
        case 4: if (k.components & VC_NBT) W(s, "\t\tcoord.xyz = inBin;\n"); break;
        default:
        {
            uint32_t t = srcRow - 5;
            if (t < 8 && (k.components & (VC_TEX0 << t)))
            {
                static const char* inTex[] = { "inTex0", "inTex1", "inTex2", "inTex3", "inTex4", "inTex5", "inTex6", "inTex7" };
                W(s, std::format("\t\tcoord = vec4({0}.x, {0}.y, 1.0, 1.0);\n", inTex[t]));
            }
            break;
        }
        }
        if (inputForm == 0) W(s, "\t\tcoord.z = 1.0;\n");
        W(s, "\t\tif (isNaN(coord.x)) coord.x = 1.0; if (isNaN(coord.y)) coord.y = 1.0; if (isNaN(coord.z)) coord.z = 1.0;\n");
        W(s, "\t\tvec3 r;\n");
        if (type == 0)
        {
            W(s, std::format("\t\tuint t = mtx[{}];\n", 1 + i));
            if (projection)
                W(s, "\t\tr = vec3(dot(xrow(t), coord), dot(xrow(t + 1u), coord), dot(xrow(t + 2u), coord));\n");
            else
                W(s, "\t\tr = vec3(dot(xrow(t), coord), dot(xrow(t + 1u), coord), 1.0);\n");
        }
        else if (type == 1)
        {
            W(s, std::format("\t\tvec3 ldir = normalize(xf3(0x{:X} + 10) - pos.xyz);\n", 0x600 + embossLight * 16));
            W(s, std::format("\t\tr = outTex{} + vec3(dot(ldir, _t), dot(ldir, _b), 0.0);\n", embossSrc < i ? embossSrc : 0));
        }
        else if (type == 2)
            W(s, "\t\tr = vec3(outCol0.x, outCol0.y, 1.0);\n");
        else
            W(s, "\t\tr = vec3(outCol1.x, outCol1.y, 1.0);\n");

        if (k.dualTex && type == 0)
        {
            uint32_t post = k.postMtxInfo[i];
            uint32_t base = (post & 0x3F) * 4;
            if ((post >> 8) & 1) W(s, "\t\tr = normalize(r);\n");
            W(s, std::format("\t\tvec4 rp = vec4(r, 1.0);\n"
                             "\t\tr = vec3(dot(vec4(xf[0x500+{0}],xf[0x501+{0}],xf[0x502+{0}],xf[0x503+{0}]), rp), "
                             "dot(vec4(xf[0x504+{0}],xf[0x505+{0}],xf[0x506+{0}],xf[0x507+{0}]), rp), "
                             "dot(vec4(xf[0x508+{0}],xf[0x509+{0}],xf[0x50A+{0}],xf[0x50B+{0}]), rp));\n", base));
        }
        if (type == 0) W(s, "\t\tif (r.z == 0.0) r.xy = clamp(r.xy / 2.0, vec2(-1.0, -1.0), vec2(1.0, 1.0));\n");
        W(s, std::format("\t\toutTex{} = r;\n\t}}\n", i));
    }

    W(s, "\tvec4 clip = ubo.proj * pos;\n");
    W(s, "\toutClipPos = clip;\n");
    W(s, "\tclip.z = clip.z * (1.0 - 1e-7);\n");
    W(s, "\tclip.z = clip.w * ubo.depthp.x + clip.z * ubo.depthp.y;\n");
    W(s, "\tclip.xy *= ubo.depthp.zw;\n");
    W(s, "\tclip.xy += ubo.pixelOffset.xy * clip.w;\n");
    // Vulkan NDC Y is opposite D3D11; negate so GX/HLSL projection matches the EFB.
    W(s, "\tclip.y = -clip.y;\n");
    W(s, "\tgl_Position = clip;\n");
    W(s, "\toutClipFinal = clip;\n");
    W(s, "}\n");
    return s;
}

// ---------------------------------------------------------------------- pixel shader
static const char* kColorIn[16] = {
    "prev.rgb", "prev.aaa", "c0.rgb", "c0.aaa", "c1.rgb", "c1.aaa", "c2.rgb", "c2.aaa",
    "textemp.rgb", "textemp.aaa", "rastemp.rgb", "rastemp.aaa", "ivec3(255,255,255)", "ivec3(128,128,128)", "konsttemp.rgb", "ivec3(0,0,0)",
};
static const char* kAlphaIn[8] = { "prev.a", "c0.a", "c1.a", "c2.a", "textemp.a", "rastemp.a", "konsttemp.a", "0" };
static const char* kOut[4] = { "prev", "c0", "c1", "c2" };
static const char* kKC[32] = {
    "ivec3(255,255,255)", "ivec3(223,223,223)", "ivec3(191,191,191)", "ivec3(159,159,159)", "ivec3(128,128,128)", "ivec3(96,96,96)", "ivec3(64,64,64)", "ivec3(32,32,32)",
    "ivec3(0,0,0)", "ivec3(0,0,0)", "ivec3(0,0,0)", "ivec3(0,0,0)",
    "ps.konst[0].rgb", "ps.konst[1].rgb", "ps.konst[2].rgb", "ps.konst[3].rgb",
    "ps.konst[0].rrr", "ps.konst[1].rrr", "ps.konst[2].rrr", "ps.konst[3].rrr",
    "ps.konst[0].ggg", "ps.konst[1].ggg", "ps.konst[2].ggg", "ps.konst[3].ggg",
    "ps.konst[0].bbb", "ps.konst[1].bbb", "ps.konst[2].bbb", "ps.konst[3].bbb",
    "ps.konst[0].aaa", "ps.konst[1].aaa", "ps.konst[2].aaa", "ps.konst[3].aaa",
};
static const char* kKA[32] = {
    "255", "223", "191", "159", "128", "96", "64", "32",
    "0", "0", "0", "0", "0", "0", "0", "0",
    "ps.konst[0].r", "ps.konst[1].r", "ps.konst[2].r", "ps.konst[3].r",
    "ps.konst[0].g", "ps.konst[1].g", "ps.konst[2].g", "ps.konst[3].g",
    "ps.konst[0].b", "ps.konst[1].b", "ps.konst[2].b", "ps.konst[3].b",
    "ps.konst[0].a", "ps.konst[1].a", "ps.konst[2].a", "ps.konst[3].a",
};
static const char* kRas[8] = {
    "ivec4(round(inCol0 * 255.0))", "ivec4(round(inCol1 * 255.0))", "ivec4(0,0,0,0)", "ivec4(0,0,0,0)", "ivec4(0,0,0,0)",
    "(ivec4(1,1,1,1) * alphabump)", "(ivec4(1,1,1,1) * (alphabump | (alphabump >> 5)))", "ivec4(0,0,0,0)",
};
static const char* kSwz = "rgba";

static std::string TevRegularGLSL(const char* comp, uint32_t bias, uint32_t op, uint32_t scale)
{
    static const char* scaleL[4] = { "", " << 1", " << 2", "" };
    static const char* scaleR[4] = { "", "", "", " >> 1" };
    static const char* lerpBias[2] = { " + 128", " + 127" };
    static const char* biasT[4] = { "", " + 128", " - 128", "" };
    return std::format("(((tevin_d.{0}{1}){2}) {3} (((((tevin_a.{0} << 8) + (tevin_b.{0} - tevin_a.{0}) * (tevin_c.{0} + (tevin_c.{0} >> 7))){2}){4}) >> 8)){5}",
                       comp, biasT[bias], scaleL[scale], op ? '-' : '+', scale != 3 ? lerpBias[op] : "", scaleR[scale]);
}

std::string GeneratePixelShaderGLSL(const PSKey& k)
{
    std::string s;
    W(s, R"(#version 450
layout(set=0, binding=2) uniform PSConst
{
    ivec4 colors[4];
    ivec4 konst[4];
    ivec4 alphaRef;
    ivec4 texDim[8];
    ivec4 indScale[4];
    ivec4 indMtx[6];
    ivec4 zBias[2];
    vec4 fogF;
    ivec4 fogI;
    ivec4 fogColor;
    vec4 fogRange[3];
    vec4 depthParams;
} ps;
layout(set=0, binding=3) uniform sampler2D tex[8];
int idot(ivec3 a, ivec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
int idot(ivec4 a, ivec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
ivec4 sampleTex(uint m, ivec2 uv)
{
    vec2 c = vec2(uv) / (128.0 * vec2(ps.texDim[m].xy));
    vec4 r;
    switch (m)
    {
    case 0u: r = texture(tex[0], c); break;
    case 1u: r = texture(tex[1], c); break;
    case 2u: r = texture(tex[2], c); break;
    case 3u: r = texture(tex[3], c); break;
    case 4u: r = texture(tex[4], c); break;
    case 5u: r = texture(tex[5], c); break;
    case 6u: r = texture(tex[6], c); break;
    default: r = texture(tex[7], c); break;
    }
    return ivec4(round(r * 255.0));
}
)");
    W(s, "layout(location=0) in vec4 inCol0;\n");
    W(s, "layout(location=1) in vec4 inCol1;\n");
    W(s, "layout(location=2) in vec4 inClipPos;\n");
    for (uint32_t i = 0; i < k.numTexGens; i++)
        W(s, std::format("layout(location={}) in vec3 inTex{};\n", 3 + i, i));
    const uint32_t clipFinalLoc = 3 + k.numTexGens;
    W(s, std::format("layout(location={}) in vec4 inClipFinal;\n", clipFinalLoc));
    W(s, "layout(location=0) out vec4 outColor;\n");

    W(s, "void main()\n{\n");
    W(s, "\tivec4 c0 = ps.colors[1], c1 = ps.colors[2], c2 = ps.colors[3], prev = ps.colors[0];\n");
    W(s, "\tivec4 rastemp = ivec4(0,0,0,0), textemp = ivec4(0,0,0,0), konsttemp = ivec4(0,0,0,0);\n");
    W(s, "\tint alphabump = 0;\n\tivec3 tevcoord = ivec3(0,0,0);\n\tivec2 wrappedcoord = ivec2(0,0);\n");
    W(s, "\tivec4 tevin_a, tevin_b, tevin_c, tevin_d;\n");

    for (uint32_t t = 0; t < k.numTexGens; t++)
    {
        int map = -1;
        for (uint32_t st = 0; st < k.numTevStages && map < 0; st++)
        {
            uint32_t ord = (k.tevOrder[st / 2] >> ((st & 1) * 12)) & 0xFFF;
            if (((ord >> 3) & 7) == t) map = ord & 7;
        }
        for (uint32_t is = 0; is < k.numIndStages && map < 0; is++)
            if (((k.indOrder >> (6 * is + 3)) & 7) == t) map = (k.indOrder >> (6 * is)) & 7;
        if (map < 0) map = 0;
        W(s, std::format("\tvec2 uvf{0} = (inTex{0}.z == 0.0) ? inTex{0}.xy : inTex{0}.xy / inTex{0}.z;\n", t));
        W(s, std::format("\tivec2 fixpoint_uv{0} = ivec2(uvf{0} * vec2(ps.texDim[{1}].xy) * 128.0);\n", t, map));
    }

    for (uint32_t is = 0; is < k.numIndStages; is++)
    {
        uint32_t map = (k.indOrder >> (6 * is)) & 7;
        uint32_t coord = (k.indOrder >> (6 * is + 3)) & 7;
        if (coord < k.numTexGens)
        {
            W(s, std::format("\tivec2 indcoord{0} = fixpoint_uv{1} >> ps.indScale[{0}].xy;\n", is, coord));
            W(s, std::format("\tivec3 iindtex{0} = sampleTex({1}, indcoord{0}).abg;\n", is, map));
        }
        else
            W(s, std::format("\tivec3 iindtex{0} = ivec3(0,0,0);\n", is));
    }

    for (uint32_t n = 0; n < k.numTevStages; n++)
    {
        uint32_t ord = (k.tevOrder[n / 2] >> ((n & 1) * 12)) & 0xFFF;
        uint32_t texmap = ord & 7, texcoord = (ord >> 3) & 7, texEnable = (ord >> 6) & 1, chan = (ord >> 7) & 7;
        bool hasCoord = texcoord < k.numTexGens;
        if (!hasCoord) texcoord = 0;
        uint32_t ind = k.tevInd[n];
        uint32_t bt = ind & 3, fmt = (ind >> 2) & 3, bias = (ind >> 4) & 7, bs = (ind >> 7) & 3, mid = (ind >> 9) & 15;
        uint32_t sw = (ind >> 13) & 7, tw = (ind >> 16) & 7, fb = (ind >> 20) & 1;
        W(s, std::format("\t// stage {}\n", n));
        bool hasInd = bt < k.numIndStages;
        if (hasInd && bs != 0)
        {
            static const char* sel[4] = { "", "x", "y", "z" };
            static const char shift[4] = { '0', '5', '4', '3' };
            W(s, std::format("\talphabump = (iindtex{}.{} << {}) & 248;\n", bt, sel[bs], shift[fmt]));
        }
        if (hasInd && (mid & 3) != 0 && k.numTexGens > 0)
        {
            static const char fshift[4] = { '0', '3', '4', '5' };
            W(s, std::format("\tivec3 iindtevcrd{0} = iindtex{1} >> {2};\n", n, bt, fshift[fmt]));
            static const char* bfield[8] = { "", "x", "y", "xy", "z", "xz", "yz", "xyz" };
            const char* badd = fmt == 0 ? "-128" : "1";
            if (bias)
                W(s, std::format("\tiindtevcrd{}.{} += {};\n", n, bfield[bias], badd));
            uint32_t mtxIdx = 2 * ((mid & 3) - 1);
            uint32_t mtxId = mid >> 2;
            if (mtxId == 0)
                W(s, std::format("\tivec2 indtevtrans{0} = ivec2(idot(ps.indMtx[{1}].xyz, iindtevcrd{0}), idot(ps.indMtx[{2}].xyz, iindtevcrd{0})) >> 3;\n", n, mtxIdx, mtxIdx + 1));
            else if (mtxId == 1)
                W(s, std::format("\tivec2 indtevtrans{0} = ivec2(fixpoint_uv{1} * iindtevcrd{0}.xx) >> 8;\n", n, texcoord));
            else
                W(s, std::format("\tivec2 indtevtrans{0} = ivec2(fixpoint_uv{1} * iindtevcrd{0}.yy) >> 8;\n", n, texcoord));
            W(s, std::format("\tif (ps.indMtx[{1}].w >= 0) indtevtrans{0} >>= ps.indMtx[{1}].w; else indtevtrans{0} <<= (-ps.indMtx[{1}].w);\n", n, mtxIdx));
        }
        else
            W(s, std::format("\tivec2 indtevtrans{} = ivec2(0, 0);\n", n));

        if (k.numTexGens > 0)
        {
            static const char* wrapStart[5] = { "(256<<7)", "(128<<7)", "(64<<7)", "(32<<7)", "(16<<7)" };
            auto wrap = [&](uint32_t w, const char* c) {
                if (w == 0) W(s, std::format("\twrappedcoord.{0} = fixpoint_uv{1}.{0};\n", c, texcoord));
                else if (w >= 6) W(s, std::format("\twrappedcoord.{} = 0;\n", c));
                else W(s, std::format("\twrappedcoord.{0} = fixpoint_uv{1}.{0} & ({2} - 1);\n", c, texcoord, wrapStart[w - 1]));
            };
            wrap(sw, "x");
            wrap(tw, "y");
            W(s, std::format("\ttevcoord.xy {}= wrappedcoord + indtevtrans{};\n", fb ? "+" : "", n));
            W(s, "\ttevcoord.xy = (tevcoord.xy << 8) >> 8;\n");
        }

        uint32_t cc = k.colorComb[n], ac = k.alphaComb[n];
        uint32_t ca = (cc >> 12) & 15, cb = (cc >> 8) & 15, ccc = (cc >> 4) & 15, cd = cc & 15;
        uint32_t aa = (ac >> 13) & 7, ab = (ac >> 10) & 7, acc = (ac >> 7) & 7, ad = (ac >> 4) & 7;
        uint32_t rswap = ac & 3, tswap = (ac >> 2) & 3;
        auto swapStr = [&](uint32_t table) {
            uint32_t rg = k.ksel[table * 2], ba = k.ksel[table * 2 + 1];
            char str[5] = { kSwz[rg & 3], kSwz[(rg >> 2) & 3], kSwz[ba & 3], kSwz[(ba >> 2) & 3], 0 };
            return std::string(str);
        };
        bool usesRas = ca == 10 || ca == 11 || cb == 10 || cb == 11 || ccc == 10 || ccc == 11 || cd == 10 || cd == 11 ||
                       aa == 5 || ab == 5 || acc == 5 || ad == 5;
        if (usesRas)
            W(s, std::format("\trastemp = {}.{};\n", kRas[chan], swapStr(rswap)));
        if (texEnable && k.numTexGens > 0)
            W(s, std::format("\ttextemp = sampleTex({}, tevcoord.xy).{};\n", texmap, swapStr(tswap)));
        else if (k.numTexGens == 0)
            W(s, "\ttextemp = ivec4(0,0,0,0);\n");
        else
            W(s, "\ttextemp = ivec4(255,255,255,255);\n");
        bool usesK = ca == 14 || cb == 14 || ccc == 14 || cd == 14 || aa == 6 || ab == 6 || acc == 6 || ad == 6;
        if (usesK)
        {
            uint32_t ks = k.ksel[n / 2];
            uint32_t kc = (n & 1) ? (ks >> 14) & 31 : (ks >> 4) & 31;
            uint32_t ka = (n & 1) ? (ks >> 19) & 31 : (ks >> 9) & 31;
            W(s, std::format("\tkonsttemp = ivec4({}, {});\n", kKC[kc], kKA[ka]));
        }
        W(s, std::format("\ttevin_a = ivec4({}, {}) & ivec4(255,255,255,255);\n", kColorIn[ca], kAlphaIn[aa]));
        W(s, std::format("\ttevin_b = ivec4({}, {}) & ivec4(255,255,255,255);\n", kColorIn[cb], kAlphaIn[ab]));
        W(s, std::format("\ttevin_c = ivec4({}, {}) & ivec4(255,255,255,255);\n", kColorIn[ccc], kAlphaIn[acc]));
        W(s, std::format("\ttevin_d = ivec4({}, {});\n", kColorIn[cd], kAlphaIn[ad]));

        uint32_t cbias = (cc >> 16) & 3, cop = (cc >> 18) & 1, cclamp = (cc >> 19) & 1, cscale = (cc >> 20) & 3, cdest = (cc >> 22) & 3;
        uint32_t abias = (ac >> 16) & 3, aop = (ac >> 18) & 1, aclamp = (ac >> 19) & 1, ascale = (ac >> 20) & 3, adest = (ac >> 22) & 3;
        W(s, std::format("\t{}.rgb = clamp(", kOut[cdest]));
        if (cbias != 3)
            W(s, TevRegularGLSL("rgb", cbias, cop, cscale));
        else
        {
            static const char* gt[4] = {
                "((tevin_a.r > tevin_b.r) ? tevin_c.rgb : ivec3(0,0,0))",
                "((idot(tevin_a.rgb, ivec3(1,256,0)) > idot(tevin_b.rgb, ivec3(1,256,0))) ? tevin_c.rgb : ivec3(0,0,0))",
                "((idot(tevin_a.rgb, ivec3(1,256,65536)) > idot(tevin_b.rgb, ivec3(1,256,65536))) ? tevin_c.rgb : ivec3(0,0,0))",
                "(max(sign(tevin_a.rgb - tevin_b.rgb), ivec3(0,0,0)) * tevin_c.rgb)" };
            static const char* eq[4] = {
                "((tevin_a.r == tevin_b.r) ? tevin_c.rgb : ivec3(0,0,0))",
                "((idot(tevin_a.rgb, ivec3(1,256,0)) == idot(tevin_b.rgb, ivec3(1,256,0))) ? tevin_c.rgb : ivec3(0,0,0))",
                "((idot(tevin_a.rgb, ivec3(1,256,65536)) == idot(tevin_b.rgb, ivec3(1,256,65536))) ? tevin_c.rgb : ivec3(0,0,0))",
                "((ivec3(1,1,1) - sign(abs(tevin_a.rgb - tevin_b.rgb))) * tevin_c.rgb)" };
            W(s, std::format("tevin_d.rgb + {}", cop ? eq[cscale] : gt[cscale]));
        }
        W(s, cclamp ? ", ivec3(0,0,0), ivec3(255,255,255));\n" : ", ivec3(-1024,-1024,-1024), ivec3(1023,1023,1023));\n");
        W(s, std::format("\t{}.a = clamp(", kOut[adest]));
        if (abias != 3)
            W(s, TevRegularGLSL("a", abias, aop, ascale));
        else
        {
            static const char* gt[4] = {
                "((tevin_a.r > tevin_b.r) ? tevin_c.a : 0)",
                "((idot(tevin_a.rgb, ivec3(1,256,0)) > idot(tevin_b.rgb, ivec3(1,256,0))) ? tevin_c.a : 0)",
                "((idot(tevin_a.rgb, ivec3(1,256,65536)) > idot(tevin_b.rgb, ivec3(1,256,65536))) ? tevin_c.a : 0)",
                "((tevin_a.a > tevin_b.a) ? tevin_c.a : 0)" };
            static const char* eq[4] = {
                "((tevin_a.r == tevin_b.r) ? tevin_c.a : 0)",
                "((idot(tevin_a.rgb, ivec3(1,256,0)) == idot(tevin_b.rgb, ivec3(1,256,0))) ? tevin_c.a : 0)",
                "((idot(tevin_a.rgb, ivec3(1,256,65536)) == idot(tevin_b.rgb, ivec3(1,256,65536))) ? tevin_c.a : 0)",
                "((tevin_a.a == tevin_b.a) ? tevin_c.a : 0)" };
            W(s, std::format("tevin_d.a + {}", aop ? eq[ascale] : gt[ascale]));
        }
        W(s, aclamp ? ", 0, 255);\n" : ", -1024, 1023);\n");
    }
    W(s, "\tprev = prev & 255;\n");

  {
        uint32_t comp0 = k.alphaFunc & 7, comp1 = (k.alphaFunc >> 3) & 7, logic = (k.alphaFunc >> 6) & 3;
        static const char* f[8] = { "(false)", "(prev.a < {})", "(prev.a == {})", "(prev.a <= {})", "(prev.a > {})", "(prev.a != {})", "(prev.a >= {})", "(true)" };
        static const char* lg[4] = { " && ", " || ", " != ", " == " };
        bool always = (logic == 0 && comp0 == 7 && comp1 == 7) || (logic == 1 && (comp0 == 7 || comp1 == 7));
        if (!always)
        {
            std::string a0 = std::vformat(f[comp0], std::make_format_args("ps.alphaRef.x"));
            std::string a1 = std::vformat(f[comp1], std::make_format_args("ps.alphaRef.y"));
            W(s, std::format("\tif (!({}{}{})) discard;\n", a0, lg[logic], a1));
        }
    }
    W(s, "\tif (prev.a == 1) prev.a = 0;\n");

    W(s, "\tint zCoord = int(ps.depthParams.x) + int((inClipPos.z / inClipPos.w) * ps.depthParams.y);\n");
    W(s, "\tzCoord = clamp(zCoord, 0, 16777215);\n");
    uint32_t zop = (k.zTexType >> 2) & 3;
    if (zop != 0 && k.numTevStages > 0)
    {
        W(s, std::format("\tzCoord = idot(ps.zBias[0], textemp) + ps.zBias[1].w {};\n", zop == 1 ? "+ zCoord" : ""));
        W(s, "\tzCoord = zCoord & 0xFFFFFF;\n");
    }
    if (k.perPixelDepth) W(s, "\tgl_FragDepth = float(zCoord) / 16777216.0;\n");

    uint32_t fsel = k.fogType & 7, fproj = (k.fogType >> 3) & 1, frange = (k.fogType >> 4) & 1;
    if (fsel >= 2)
    {
        if (fproj == 0)
            W(s, "\tfloat ze = (ps.fogF.x * 16777216.0) / float(ps.fogI.y - (zCoord >> ps.fogI.w));\n");
        else
            W(s, "\tfloat ze = ps.fogF.x * float(zCoord) / 16777216.0;\n");
        if (frange)
        {
            W(s, "\tfloat offset = (2.0 * (inClipFinal.x / ps.fogF.w)) - 1.0 - ps.fogF.z;\n"
                 "\tfloat floatindex = clamp(9.0 - abs(offset) * 9.0, 0.0, 9.0);\n"
                 "\tuint indexlower = uint(floatindex); uint indexupper = indexlower + 1u;\n"
                 "\tfloat klower = ps.fogRange[indexlower >> 2u][indexlower & 3u];\n"
                 "\tfloat kupper = ps.fogRange[indexupper >> 2u][indexupper & 3u];\n"
                 "\tfloat kk = mix(klower, kupper, fract(floatindex));\n"
                 "\tze *= sqrt(offset * offset + kk * kk) / kk;\n");
        }
        W(s, "\tfloat fog = clamp(ze - ps.fogF.y, 0.0, 1.0);\n");
        switch (fsel)
        {
        case 4: W(s, "\tfog = 1.0 - exp2(-8.0 * fog);\n"); break;
        case 5: W(s, "\tfog = 1.0 - exp2(-8.0 * fog * fog);\n"); break;
        case 6: W(s, "\tfog = exp2(-8.0 * (1.0 - fog));\n"); break;
        case 7: W(s, "\tfog = 1.0 - fog; fog = exp2(-8.0 * fog * fog);\n"); break;
        }
        W(s, "\tint ifog = int(round(fog * 256.0));\n");
        W(s, "\tprev.rgb = (prev.rgb * (256 - ifog) + ps.fogColor.rgb * ifog) >> 8;\n");
    }

    bool rgba6 = k.pixelFormat == 1;
    if (rgba6) W(s, "\toutColor.rgb = vec3(prev.rgb >> 2) / 63.0;\n");
    else W(s, "\toutColor.rgb = vec3(prev.rgb) / 255.0;\n");
    if (k.dstAlphaEnable)
        W(s, rgba6 ? "\toutColor.a = float(ps.alphaRef.z >> 2) / 63.0;\n" : "\toutColor.a = float(ps.alphaRef.z) / 255.0;\n");
    else
        W(s, rgba6 ? "\toutColor.a = float(prev.a >> 2) / 63.0;\n" : "\toutColor.a = float(prev.a) / 255.0;\n");
    W(s, "}\n");
    return s;
}