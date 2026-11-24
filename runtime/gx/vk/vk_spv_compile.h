#pragma once
// A4e: compile generated GLSL 450 → SPIR-V via glslangValidator; disk-cache by source hash.
#include <cstdint>
#include <string>
#include <vector>

struct VkSpvCompileStats
{
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    uint64_t compileOk = 0;
    uint64_t compileFail = 0;
};

// stage: "vert" or "frag". On success fills outSpv (uint32 words). On failure sets error.
bool VkSpv_CompileGlsl(const std::string& glsl, const char* stage, std::vector<uint32_t>& outSpv,
                       std::string& error);

VkSpvCompileStats VkSpv_Stats();
void VkSpv_ResetStats();
