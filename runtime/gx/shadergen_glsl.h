#pragma once
#include "shadergen.h"
#include <string>

std::string GenerateVertexShaderGLSL(const VSKey& key);
std::string GeneratePixelShaderGLSL(const PSKey& key);