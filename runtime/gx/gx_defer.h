#pragma once
// A5f: dirty derived-state tracking for coalescing PSKey/VSKey rebuilds.
// Canonical BP/`__gx` updates remain immediate in GX_ApplyBpWord.
#include <cstdint>

// Opt-in: SCR_A5F_DEFERRED_BP=1
bool GxDefer_Enabled();

enum class GxDeferFamily : uint32_t
{
    None = 0,
    GenMode = 1u << 0,   // BP 0x00 — tev/texgen counts, cull
    Tev = 1u << 1,       // color/alpha env, tev order, ind
    Ksel = 1u << 2,      // 0xF6–0xFD
    Fog = 1u << 3,       // fog + related
    Alpha = 1u << 4,     // alpha test / ztex
    TexUnit = 1u << 5,   // TX 0x80–0xBB, SU scale
    Blend = 1u << 6,     // CMODE / PE blend
    Depth = 1u << 7,     // ZMODE / PE control
    Ras = 1u << 8,       // scissor/viewport-ish BP
    Light = 1u << 9,     // (XF-driven; marked from OnXF if needed)
    All = 0xFFFFFFFFu,
};

inline GxDeferFamily operator|(GxDeferFamily a, GxDeferFamily b)
{
    return (GxDeferFamily)((uint32_t)a | (uint32_t)b);
}
inline GxDeferFamily& operator|=(GxDeferFamily& a, GxDeferFamily b)
{
    a = a | b;
    return a;
}
inline bool GxDefer_Any(GxDeferFamily a, GxDeferFamily b)
{
    return ((uint32_t)a & (uint32_t)b) != 0;
}

// Map a BP register to dirty families (pure classification; no I/O).
GxDeferFamily GxDefer_FamiliesForBp(uint32_t reg);

// Mark derived state dirty after a canonical BP update.
void GxDefer_MarkBp(uint32_t reg);
void GxDefer_Mark(GxDeferFamily fam);
void GxDefer_InvalidateAll(const char* why);

// True if a consuming Build*Key must rebuild (dirty or never cached).
bool GxDefer_PsKeyNeedsRebuild();
bool GxDefer_VsKeyNeedsRebuild(uint32_t components);

// After a successful Build*Key, clear the matching dirty families.
void GxDefer_NotePsKeyBuilt();
void GxDefer_NoteVsKeyBuilt(uint32_t components);

// Whether Render_OnBP has immediate work for this register (EFB/TLUT/konst).
bool GxDefer_NeedsImmediateOnBp(uint32_t reg);

struct GxDeferStats
{
    uint64_t flushSkipped = 0;   // Render_Flush skipped (no pending geometry)
    uint64_t onBpSkipped = 0;    // empty ordinary OnBP skipped
    uint64_t psKeyReuse = 0;
    uint64_t psKeyBuild = 0;
    uint64_t vsKeyReuse = 0;
    uint64_t vsKeyBuild = 0;
    uint64_t marks = 0;
    uint64_t invalidations = 0;
};

const GxDeferStats& GxDefer_Stats();
void GxDefer_StatsReset();
void GxDefer_LogStats(const char* tag);

// Internal counters (gx_shadow).
void GxDefer_NoteFlushSkipped();
void GxDefer_NoteOnBpSkipped();
