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

layout(location=0) out vec4 vCol0;
layout(location=1) out vec3 vTex0;

layout(set=0, binding=0) readonly buffer XF {
    float xf[];
};
layout(set=0, binding=1) uniform VSConst {
    layout(row_major) mat4 proj;       // row-major from host — we mul as pos * proj with transpose, or use column
    vec4 depthp;     // far/2^24, zRange/2^24, flipX, flipY
    ivec4 materials[4];
    vec4 pixelOffset;
} ubo;

vec4 xrow(uint r) {
    uint b = r * 4u;
    return vec4(xf[b], xf[b+1u], xf[b+2u], xf[b+3u]);
}

void main() {
    uint pidx = inMtx.x & 0xFFu;
    vec4 rawpos = vec4(inPos, 1.0);
    vec4 pos = vec4(dot(xrow(pidx), rawpos),
                    dot(xrow(pidx + 1u), rawpos),
                    dot(xrow(pidx + 2u), rawpos),
                    1.0);

    // Vertex colour (lighting deferred — Stage B uses vtx/material blend)
    vCol0 = inCol0;

    // Texgen type-0 for tex0 using matrix index from mtx byte1 when present
    uint tidx = (inMtx.x >> 8) & 0xFFu;
    vec4 tcoord = vec4(inTex0.x, inTex0.y, 1.0, 1.0);
    vTex0 = vec3(dot(xrow(tidx), tcoord), dot(xrow(tidx + 1u), tcoord), 1.0);

    // Host stores row-major proj like D3D HLSL mul(proj, pos)
    vec4 clip = ubo.proj * pos;
    clip.z = clip.z * (1.0 - 1e-7);
    clip.z = clip.w * ubo.depthp.x + clip.z * ubo.depthp.y;
    clip.xy *= ubo.depthp.zw;
    clip.xy += ubo.pixelOffset.xy * clip.w;
    gl_Position = clip;
}
