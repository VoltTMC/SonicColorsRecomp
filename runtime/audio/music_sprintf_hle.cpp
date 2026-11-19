// Intercept MSL sprintf so Music Mods learns the live stage from paths the game
// builds (scripts/stgNNN_mission.lua, set/stgNNN.orc, …). Match on the written
// result as well as known format strings — Colors may use variants.
#include "../game_addrs.h"
#include "music_mod.h"
#include "../kernel/memory.h"

#include <cctype>
#include <cstring>

namespace
{

bool GuestEA(uint32_t ea)
{
    return ea && g_base && ((ea >> 28) == 0x8 || (ea >> 28) == 0x9);
}

// Pull stgNNN out of a path/token if present.
bool StageFromText(const char* s, char out[7])
{
    if (!s) return false;
    for (const char* p = s; *p; ++p)
    {
        if ((p[0] == 's' || p[0] == 'S') && (p[1] == 't' || p[1] == 'T') && (p[2] == 'g' || p[2] == 'G') &&
            std::isdigit((unsigned char)p[3]) && std::isdigit((unsigned char)p[4]) &&
            std::isdigit((unsigned char)p[5]))
        {
            // Prefer tokens tied to mission/orc stage loads
            const char* rest = p + 6;
            const bool mission = std::strncmp(rest, "_mission", 8) == 0;
            const bool orc = std::strncmp(rest, ".orc", 4) == 0;
            const bool bare = rest[0] == '\0' || rest[0] == '/' || rest[0] == '\\';
            if (!mission && !orc && !bare) continue;
            out[0] = 's';
            out[1] = 't';
            out[2] = 'g';
            out[3] = p[3];
            out[4] = p[4];
            out[5] = p[5];
            out[6] = '\0';
            return true;
        }
        if (p - s > 256) break;
    }
    return false;
}

} // namespace

PPC_OVERRIDE(sprintf)
{
    const uint32_t dstEA = (uint32_t)ctx.r[3];
    const uint32_t fmtEA = (uint32_t)ctx.r[4];
    const uint32_t argEA = (uint32_t)ctx.r[5];

    const char* via = nullptr;
    if (GuestEA(fmtEA))
    {
        const char* fmt = (const char*)GuestPtr(fmtEA);
        if (std::strncmp(fmt, "scripts/%s_mission.lua", 22) == 0)
            via = "scripts/%s_mission.lua";
        else if (std::strncmp(fmt, "%s_mission.lua", 14) == 0)
            via = "%s_mission.lua";
        else if (std::strncmp(fmt, "set/%s.orc", 10) == 0)
            via = "set/%s.orc";
        else if (std::strstr(fmt, "_mission.lua") != nullptr)
            via = "fmt*_mission.lua";
        else if (std::strstr(fmt, ".orc") != nullptr && std::strstr(fmt, "set/") != nullptr)
            via = "fmt*set/*.orc";
    }

    if (via && GuestEA(argEA))
    {
        char stage[7];
        if (StageFromText((const char*)GuestPtr(argEA), stage))
            MusicMod_OnStageName(stage, via);
    }

    PPC_ORIGINAL(sprintf);

    // After write: catch any sprintf that produced a stage mission/orc path
    if (GuestEA(dstEA))
    {
        const char* dst = (const char*)GuestPtr(dstEA);
        char stage[7];
        if (StageFromText(dst, stage))
        {
            const char* how = via ? via : "sprintf-result";
            if (std::strstr(dst, "_mission") != nullptr || std::strstr(dst, ".orc") != nullptr)
                MusicMod_OnStageName(stage, how);
        }
    }
}
