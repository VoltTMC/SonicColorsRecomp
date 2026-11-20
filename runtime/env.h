#pragma once
// Prefer SCR_* env vars; fall back to legacy BKR_* names used by older scripts.
#include <cstdlib>

inline const char* EnvGet(const char* scr, const char* bkr)
{
    if (const char* v = std::getenv(scr)) return v;
    return std::getenv(bkr);
}

inline bool EnvHas(const char* scr, const char* bkr)
{
    return EnvGet(scr, bkr) != nullptr;
}
