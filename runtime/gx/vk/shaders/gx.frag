#version 450
layout(location=0) in vec4 vCol0;
layout(location=1) in vec3 vTex0;
layout(location=0) out vec4 outColor;

layout(set=0, binding=2) uniform sampler2D tex0;
layout(set=0, binding=3) uniform PSFlags {
    uint useTex;
    uint _pad0;
    uint _pad1;
    uint _pad2;
} flags;

void main() {
    vec4 c = vCol0;
    if (flags.useTex != 0u) {
        vec2 uv = vTex0.xy;
        if (abs(vTex0.z) > 1e-6) uv /= vTex0.z;
        c *= texture(tex0, uv);
    }
    outColor = c;
}
