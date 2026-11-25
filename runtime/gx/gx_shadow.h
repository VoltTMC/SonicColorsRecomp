#pragma once
// A5c: coherent host BP shadow validity / classification around g_gx.bp[].
// Guest-visible __gx (SNCP8P lbl_807FA960 via __GXData @ 808FC248) remains authoritative
// for game-readable SDK fields; g_gx.bp[] mirrors hardware BP after FIFO or direct apply.
#include <cstdint>

// SNCP8P GXData object (size 0x600). Pointer held at .sdata2:808FC248.
constexpr uint32_t kSncp8pGxDataEa = 0x807FA960u;
constexpr uint32_t kSncp8pGxDataPtrEa = 0x808FC248u;
constexpr uint32_t kGxDirtyFlagsOff = 0x2u;
constexpr uint32_t kGxTevColorEnvOff = 0x180u;
constexpr uint32_t kGxTevAlphaEnvOff = 0x1C0u;
constexpr uint32_t kGxCmode0Off = 0x220u; // BP 0x41
constexpr uint32_t kGxZmodeOff = 0x228u;  // BP 0x40

enum class GxBpClass : uint8_t
{
    Ordinary = 0,   // pure state; identical write may skip OnBP
    Special,        // side effects even when value unchanged (or always flush)
    Trigger,        // EFB copy / TLUT / finish / token
    Unknown,
};

struct GxBpRegProf
{
    uint64_t writes = 0;
    uint64_t nops = 0;     // identical ordinary skipped (feeds bpNop)
    uint64_t changes = 0;  // value changed
    uint64_t special = 0;  // classified special/trigger
    uint64_t direct = 0;   // applied via typed direct path
};

bool GxShadow_BpProfileEnabled();
void GxShadow_Reset();
void GxShadow_InvalidateAll(const char* why);
void GxShadow_InvalidateReg(uint32_t reg);
bool GxShadow_IsValid(uint32_t reg);
uint64_t GxShadow_Generation();
GxBpClass GxShadow_Classify(uint32_t reg);
bool GxShadow_IsSpecial(uint32_t reg); // Special or Trigger

// Shared BP apply (FIFO decode + typed direct). Updates g_gx.bp, side effects, Render_OnBP.
void GX_ApplyBpWord(uint32_t word);

void GxShadow_NoteDirectBp(uint32_t reg);
void GxShadow_NoteInvalidation();
void GxShadow_NoteReestablish();
const GxBpRegProf* GxShadow_RegProf(); // [256]
void GxShadow_LogProfile(const char* tag);
