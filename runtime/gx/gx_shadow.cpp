// A5c: BP shadow validity + shared apply path (FIFO and typed direct).
#include "gx_shadow.h"
#include "gfx_native.h"
#include "gx_state.h"
#include "../log.h"

#include <cstdlib>
#include <cstring>
#include <mutex>

void GX_PE_Finish();
void GX_PE_Token(uint16_t token, bool interrupt);

namespace
{
std::mutex s_mu;
uint64_t s_gen = 1;
uint8_t s_valid[0x100];
GxBpRegProf s_prof[0x100];
uint64_t s_invalidations = 0;
uint64_t s_reestablish = 0;
int s_bpProf = -1;

void EnsureProf()
{
    if (s_bpProf >= 0) return;
    const char* e = std::getenv("SCR_A5C_BP_PROFILE");
    s_bpProf = (e && e[0] == '1') ? 1 : 0;
}
} // namespace

bool GxShadow_BpProfileEnabled()
{
    EnsureProf();
    return s_bpProf != 0;
}

void GxShadow_Reset()
{
    std::lock_guard lk(s_mu);
    std::memset(s_valid, 0, sizeof(s_valid));
    std::memset(s_prof, 0, sizeof(s_prof));
    s_gen++;
    s_invalidations = 0;
    s_reestablish = 0;
}

void GxShadow_InvalidateAll(const char* why)
{
    std::lock_guard lk(s_mu);
    std::memset(s_valid, 0, sizeof(s_valid));
    s_gen++;
    s_invalidations++;
    if (why && GfxNative_ProfEnabled())
        LOG_INFO("A5c-shadow: invalidate-all gen={} ({})", s_gen, why);
}

void GxShadow_InvalidateReg(uint32_t reg)
{
    if (reg >= 0x100) return;
    std::lock_guard lk(s_mu);
    s_valid[reg] = 0;
    s_gen++;
    s_invalidations++;
}

bool GxShadow_IsValid(uint32_t reg)
{
    if (reg >= 0x100) return false;
    return s_valid[reg] != 0;
}

uint64_t GxShadow_Generation()
{
    return s_gen;
}

GxBpClass GxShadow_Classify(uint32_t reg)
{
    // Classification from SNCP8P LoadBP / Render_OnBP side effects (A5b/A5c).
    // Do not skip identical writes for Special/Trigger/Unknown.
    switch (reg)
    {
    case 0x45: // PE_DONE / finish
    case 0x47: // PE token
    case 0x48: // PE token + interrupt
        return GxBpClass::Trigger;
    case 0x52: // EFB copy trigger
    case 0x65: // TLUT load
        return GxBpClass::Trigger;
    default:
        if (reg >= 0xE0 && reg <= 0xE7)
            return GxBpClass::Special; // TEV color/konst latch semantics
        if (reg > 0xFF)
            return GxBpClass::Unknown;
        return GxBpClass::Ordinary;
    }
}

bool GxShadow_IsSpecial(uint32_t reg)
{
    const GxBpClass c = GxShadow_Classify(reg);
    return c == GxBpClass::Special || c == GxBpClass::Trigger || c == GxBpClass::Unknown;
}

void GxShadow_NoteDirectBp(uint32_t reg)
{
    if (reg >= 0x100) return;
    EnsureProf();
    if (!s_bpProf && !GfxNative_ProfEnabled()) return;
    std::lock_guard lk(s_mu);
    s_prof[reg].direct++;
}

void GxShadow_NoteInvalidation()
{
    std::lock_guard lk(s_mu);
    s_invalidations++;
}

void GxShadow_NoteReestablish()
{
    std::lock_guard lk(s_mu);
    s_reestablish++;
}

const GxBpRegProf* GxShadow_RegProf() { return s_prof; }

void GxShadow_LogProfile(const char* tag)
{
    EnsureProf();
    if (!s_bpProf) return;
    struct Row
    {
        uint32_t reg;
        uint64_t writes;
    };
    Row top[16]{};
    int ntop = 0;
    uint64_t total = 0, nops = 0, special = 0, direct = 0;
    for (uint32_t r = 0; r < 0x100; r++)
    {
        const GxBpRegProf& p = s_prof[r];
        total += p.writes;
        nops += p.nops;
        special += p.special;
        direct += p.direct;
        if (!p.writes) continue;
        int slot = ntop < 16 ? ntop++ : -1;
        if (slot < 0)
        {
            int worst = 0;
            for (int i = 1; i < 16; i++)
                if (top[i].writes < top[worst].writes) worst = i;
            if (p.writes <= top[worst].writes) continue;
            slot = worst;
        }
        top[slot] = { r, p.writes };
    }
    for (int i = 0; i < ntop; i++)
        for (int j = i + 1; j < ntop; j++)
            if (top[j].writes > top[i].writes)
            {
                Row t = top[i];
                top[i] = top[j];
                top[j] = t;
            }
    LOG_INFO("A5c-bpprof-{}: total={} nops={} special={} direct={} inv={} reest={} gen={}",
             tag ? tag : "stats", total, nops, special, direct, s_invalidations, s_reestablish, s_gen);
    for (int i = 0; i < ntop && i < 12; i++)
    {
        const uint32_t r = top[i].reg;
        const GxBpRegProf& p = s_prof[r];
        LOG_INFO("A5c-bpprof-{}: reg={:02x} class={} writes={} changes={} nops={} special={} direct={}",
                 tag ? tag : "stats", r, (int)GxShadow_Classify(r), p.writes, p.changes, p.nops, p.special,
                 p.direct);
    }
}

void GX_ApplyBpWord(uint32_t word)
{
    const uint32_t reg = word >> 24;
    const uint32_t val = word & 0xFFFFFF;
    uint32_t mask = g_gx.bpMaskValue;
    g_gx.bpMaskValue = 0xFFFFFF;
    if (reg == 0xFE)
    {
        g_gx.bpMaskValue = val;
        if (GxShadow_BpProfileEnabled())
        {
            std::lock_guard lk(s_mu);
            s_prof[0xFE].writes++;
            s_prof[0xFE].changes++;
            s_valid[0xFE] = 1;
        }
        return;
    }
    if (reg >= 0x100) return;

    const uint32_t old = g_gx.bp[reg];
    const uint32_t nv = (old & ~mask) | (val & mask);
    const bool special = GxShadow_IsSpecial(reg);
    const bool wasValid = s_valid[reg] != 0;

    EnsureProf();
    if (s_bpProf || GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_mu);
        s_prof[reg].writes++;
        if (special) s_prof[reg].special++;
        if (nv != old) s_prof[reg].changes++;
    }

    // Identical ordinary write with a valid shadow: pure no-op (A5b bpNop).
    if (nv == old && !special && wasValid)
    {
        GfxNative_NoteBpNop();
        if (s_bpProf)
        {
            std::lock_guard lk(s_mu);
            s_prof[reg].nops++;
        }
        return;
    }

    // Never skip special/trigger; also never skip when shadow was invalid
    // (re-establish by applying).
    if (nv != old || special)
        Render_Flush();
    g_gx.bp[reg] = nv;
    {
        std::lock_guard lk(s_mu);
        if (!wasValid) s_reestablish++;
        s_valid[reg] = 1;
    }

    switch (reg)
    {
    case 0x45:
        if (nv & 2) GX_PE_Finish();
        break;
    case 0x47:
        GX_PE_Token((uint16_t)nv, false);
        break;
    case 0x48:
        GX_PE_Token((uint16_t)nv, true);
        break;
    default:
        break;
    }
    Render_OnBP(reg, old, nv);
}
