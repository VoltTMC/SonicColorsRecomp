#include "gx_defer.h"
#include "../log.h"

#include <cstdlib>
#include <cstring>

namespace
{
bool s_enabledResolved = false;
bool s_enabled = false;
uint32_t s_psDirty = (uint32_t)GxDeferFamily::All;
uint32_t s_vsDirty = (uint32_t)GxDeferFamily::All;
bool s_psKeyValid = false;
bool s_vsKeyValid = false;
uint32_t s_vsComponents = 0xFFFFFFFFu;
GxDeferStats s_stats{};

constexpr uint32_t kPsMask =
    (uint32_t)GxDeferFamily::GenMode | (uint32_t)GxDeferFamily::Tev | (uint32_t)GxDeferFamily::Ksel |
    (uint32_t)GxDeferFamily::Fog | (uint32_t)GxDeferFamily::Alpha | (uint32_t)GxDeferFamily::TexUnit |
    (uint32_t)GxDeferFamily::Blend | (uint32_t)GxDeferFamily::Depth | (uint32_t)GxDeferFamily::Ras;

constexpr uint32_t kVsMask = (uint32_t)GxDeferFamily::GenMode | (uint32_t)GxDeferFamily::Ras |
                             (uint32_t)GxDeferFamily::Light;

bool EnvTruthy(const char* v)
{
    return v && v[0] && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0 &&
           std::strcmp(v, "False") != 0 && std::strcmp(v, "FALSE") != 0 && std::strcmp(v, "off") != 0 &&
           std::strcmp(v, "OFF") != 0;
}

void ResolveEnabled()
{
    if (s_enabledResolved)
        return;
    s_enabledResolved = true;
    s_enabled = EnvTruthy(std::getenv("SCR_A5F_DEFERRED_BP"));
    if (s_enabled)
        LOG_INFO("A5f: SCR_A5F_DEFERRED_BP=1 — skip empty Flush/OnBP; coalesce PSKey/VSKey");
}
} // namespace

bool GxDefer_Enabled()
{
    ResolveEnabled();
    return s_enabled;
}

GxDeferFamily GxDefer_FamiliesForBp(uint32_t reg)
{
    // Triggers with broad invalidation (rare relative to konst).
    if (reg == 0x52 || reg == 0x65 || reg == 0x66)
        return GxDeferFamily::All;
    // Finish/token: no shader-key fields.
    if (reg == 0x45 || reg == 0x47 || reg == 0x48)
        return GxDeferFamily::None;
    // Konst/color latch (hot): OnBP updates g_tevKonst immediately; PSKey does not
    // encode konst values (constants are filled at Flush from live state).
    if (reg >= 0xE0 && reg <= 0xE7)
        return GxDeferFamily::None;

    if (reg == 0x00)
        return GxDeferFamily::GenMode | GxDeferFamily::Tev | GxDeferFamily::Ras;

    if ((reg >= 0xC0 && reg <= 0xDF) || (reg >= 0x10 && reg <= 0x2F))
        return GxDeferFamily::Tev;

    if (reg >= 0xF6 && reg <= 0xFD)
        return GxDeferFamily::Ksel | GxDeferFamily::Tev;

    // Fog: F1 control, E8 range adj, F0/F2 params (BuildPSKey uses F1/E8)
    if (reg == 0xE8 || reg == 0xF0 || reg == 0xF1 || reg == 0xF2)
        return GxDeferFamily::Fog;

    if (reg == 0xF3)
        return GxDeferFamily::Alpha;
    if (reg == 0xF4 || reg == 0xF5)
        return GxDeferFamily::Alpha | GxDeferFamily::Fog;

    if ((reg >= 0x80 && reg <= 0xBB) || (reg >= 0x30 && reg <= 0x3F))
        return GxDeferFamily::TexUnit;

    if (reg == 0x41 || reg == 0x42 || reg == 0x43)
        return GxDeferFamily::Blend;

    if (reg == 0x40 || reg == 0x44)
        return GxDeferFamily::Depth;

    if (reg == 0x20 || reg == 0x21 || reg == 0x59 || reg == 0x5A)
        return GxDeferFamily::Ras;

    // Conservative: treat unknown ordinary as TEV-facing derived state.
    return GxDeferFamily::Tev | GxDeferFamily::Fog | GxDeferFamily::Alpha;
}

void GxDefer_Mark(GxDeferFamily fam)
{
    if (!GxDefer_Enabled())
        return;
    const uint32_t bits = (uint32_t)fam;
    if (bits == 0)
        return;
    ++s_stats.marks;
    if (bits == (uint32_t)GxDeferFamily::All)
    {
        s_psDirty = kPsMask;
        s_vsDirty = kVsMask;
        s_psKeyValid = false;
        s_vsKeyValid = false;
        return;
    }
    if (bits & kPsMask)
    {
        s_psDirty |= (bits & kPsMask);
        s_psKeyValid = false;
    }
    if (bits & kVsMask)
    {
        s_vsDirty |= (bits & kVsMask);
        s_vsKeyValid = false;
    }
}

void GxDefer_MarkBp(uint32_t reg)
{
    GxDefer_Mark(GxDefer_FamiliesForBp(reg));
}

void GxDefer_InvalidateAll(const char* /*why*/)
{
    s_psDirty = kPsMask;
    s_vsDirty = kVsMask;
    s_psKeyValid = false;
    s_vsKeyValid = false;
    s_vsComponents = 0xFFFFFFFFu;
    ++s_stats.invalidations;
}

bool GxDefer_NeedsImmediateOnBp(uint32_t reg)
{
    return reg == 0x52 || reg == 0x65 || (reg >= 0xE0 && reg <= 0xE7);
}

bool GxDefer_PsKeyNeedsRebuild()
{
    if (!GxDefer_Enabled())
        return true;
    if (!s_psKeyValid || s_psDirty != 0)
        return true;
    ++s_stats.psKeyReuse;
    return false;
}

bool GxDefer_VsKeyNeedsRebuild(uint32_t components)
{
    if (!GxDefer_Enabled())
        return true;
    if (!s_vsKeyValid || s_vsDirty != 0 || components != s_vsComponents)
        return true;
    ++s_stats.vsKeyReuse;
    return false;
}

void GxDefer_NotePsKeyBuilt()
{
    if (!GxDefer_Enabled())
        return;
    s_psDirty = 0;
    s_psKeyValid = true;
    ++s_stats.psKeyBuild;
}

void GxDefer_NoteVsKeyBuilt(uint32_t components)
{
    if (!GxDefer_Enabled())
        return;
    s_vsDirty = 0;
    s_vsKeyValid = true;
    s_vsComponents = components;
    ++s_stats.vsKeyBuild;
}

const GxDeferStats& GxDefer_Stats()
{
    return s_stats;
}

void GxDefer_StatsReset()
{
    s_stats = {};
}

void GxDefer_LogStats(const char* tag)
{
    LOG_INFO("A5f-{}: flushSkip={} onBpSkip={} psReuse={} psBuild={} vsReuse={} vsBuild={} marks={} inv={} "
             "psDirty={:x} vsDirty={:x}",
             tag ? tag : "stats", s_stats.flushSkipped, s_stats.onBpSkipped, s_stats.psKeyReuse,
             s_stats.psKeyBuild, s_stats.vsKeyReuse, s_stats.vsKeyBuild, s_stats.marks,
             s_stats.invalidations, s_psDirty, s_vsDirty);
}

void GxDefer_NoteFlushSkipped()
{
    ++s_stats.flushSkipped;
}

void GxDefer_NoteOnBpSkipped()
{
    ++s_stats.onBpSkipped;
}
