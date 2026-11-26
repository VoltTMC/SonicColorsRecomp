// A5a–A5c: opt-in direct GX SDK → typed GX-thread commands (ordered vs FIFO).
#include "gfx_native.h"
#include "gx_shadow.h"
#include "gx_state.h"
#include "../cpu/ppc_context.h"
#include "../game_addrs.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../platform/platform.h"

#include <atomic>
#include <cstring>
#include <mutex>

size_t GX_FifoBytesWritten();
void GX_FifoPublishPending();
void GX_FifoSkipPipeBytes(uint32_t bytes);
bool GX_FifoRecordingToRAM();
void GX_FifoWakeGpu();

namespace
{
constexpr size_t kTypedCap = 1u << 14;
constexpr uint32_t kXfWordsMax = 64;

enum class TypedKind : uint8_t { Xf = 0, Bp };

struct TypedCmd
{
    TypedKind kind = TypedKind::Xf;
    size_t afterFifoW = 0;
    uint32_t addr = 0;   // XF addr, or BP word for Bp
    uint32_t count = 0;  // XF count; unused for Bp
    uint32_t words[kXfWordsMax]{};
};

std::atomic<uint32_t> s_typedW{ 0 };
std::atomic<uint32_t> s_typedR{ 0 };
TypedCmd s_typed[kTypedCap];

GfxNativeStats s_stats{};
std::mutex s_statsMu;

int s_direct = -1;
int s_directBp = -1;
int s_directHot = -1;
int s_directKsel = -1;
int s_directTexObj = -1;
int s_prof = -1;
int s_decodeProf = -1;

uint64_t MonoNs()
{
    static uint64_t freq = 0;
    if (!freq) freq = Platform_PerfFrequency();
    if (!freq) return 0;
    return Platform_PerfCounter() * 1000000000ull / freq;
}

void EnsureFlags()
{
    if (s_direct < 0)
    {
        const char* e = std::getenv("SCR_A5A_DIRECT");
        s_direct = (e && e[0] == '1') ? 1 : 0;
        if (s_direct)
            LOG_INFO("A5a/A5b: DIRECT=1 — Pos/Nrm/TexMtxImm → typed XF (SNCP8P 8036EE10/EE90/EF50)");
    }
    if (s_directBp < 0)
    {
        const char* e = std::getenv("SCR_A5C_DIRECT_BP");
        s_directBp = (e && e[0] == '1') ? 1 : 0;
        if (s_directBp)
            LOG_INFO("A5c: DIRECT_BP=1 — Blend/Z/TEV/PE → typed BP (SNCP8P D500–E080)");
    }
    if (s_directHot < 0)
    {
        const char* e = std::getenv("SCR_A5D_DIRECT_HOT");
        s_directHot = (e && e[0] == '1') ? 1 : 0;
        if (s_directHot)
            LOG_INFO("A5d: DIRECT_HOT=1 — IND_IMASK/SU_SCALE/TX_INVAL (SNCP8P D400/D430/C930/C9C0/CA40/A8B0)");
    }
    if (s_directKsel < 0)
    {
        const char* e = std::getenv("SCR_A5D_DIRECT_KSEL");
        s_directKsel = (e && e[0] == '1') ? 1 : 0;
        if (s_directKsel)
            LOG_INFO("A5d: DIRECT_KSEL=1 — GXSetTevSwapMode → KSEL 0xF6 (SNCP8P D2B0)");
    }
    if (s_directTexObj < 0)
    {
        const char* e = std::getenv("SCR_A5E_DIRECT_TEXOBJ");
        s_directTexObj = (e && e[0] == '1') ? 1 : 0;
        if (s_directTexObj)
            LOG_INFO("A5e: DIRECT_TEXOBJ=1 — GXLoadTexObj/PreLoaded/LoadTlut (SNCP8P C5C0/C730/C7C0)");
    }
    if (s_prof < 0)
    {
        const char* e = std::getenv("SCR_A5A_PROF");
        const char* e2 = std::getenv("SCR_A5B_PROF");
        const char* e3 = std::getenv("SCR_A5C_BP_PROFILE");
        s_prof = (e && e[0] && e[0] != '0') || (e2 && e2[0] && e2[0] != '0') ||
                         (e3 && e3[0] == '1') || s_direct || s_directBp || s_directHot || s_directKsel ||
                         s_directTexObj
                     ? 1
                     : 0;
        s_decodeProf = (e2 && e2[0] == '1') || s_prof ? 1 : 0;
    }
}

bool ReserveSlot(uint32_t& outW)
{
    GX_FifoPublishPending();
    uint32_t w = s_typedW.load(std::memory_order_relaxed);
    uint32_t r = s_typedR.load(std::memory_order_acquire);
    if (w - r >= kTypedCap)
    {
        auto lk = GX_SyncGPU();
        (void)lk;
        r = s_typedR.load(std::memory_order_acquire);
        if (w - r >= kTypedCap) return false;
    }
    outW = w;
    return true;
}

bool EnqueueXf(uint32_t addr, uint32_t count, const uint32_t* hostWords)
{
    if (count == 0 || count > kXfWordsMax || !hostWords) return false;
    EnsureFlags();
    uint32_t w = 0;
    if (!ReserveSlot(w)) return false;
    const size_t after = GX_FifoBytesWritten();
    TypedCmd& slot = s_typed[w % kTypedCap];
    slot.kind = TypedKind::Xf;
    slot.afterFifoW = after;
    slot.addr = addr;
    slot.count = count;
    std::memcpy(slot.words, hostWords, count * sizeof(uint32_t));
    s_typedW.store(w + 1, std::memory_order_release);
    GX_FifoWakeGpu();
    const uint32_t avoided = 5u + 4u * count;
    GX_FifoSkipPipeBytes(avoided);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directXf++;
        s_stats.fifoBytesAvoided += avoided;
    }
    return true;
}

bool EnqueueBpWord(uint32_t word)
{
    EnsureFlags();
    uint32_t w = 0;
    if (!ReserveSlot(w)) return false;
    const size_t after = GX_FifoBytesWritten();
    TypedCmd& slot = s_typed[w % kTypedCap];
    slot.kind = TypedKind::Bp;
    slot.afterFifoW = after;
    slot.addr = word;
    slot.count = 0;
    s_typedW.store(w + 1, std::memory_order_release);
    GX_FifoWakeGpu();
    constexpr uint32_t kAvoided = 5u; // 0x61 + u32
    GX_FifoSkipPipeBytes(kAvoided);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directBp++;
        s_stats.fifoBytesAvoided += kAvoided;
    }
    GxShadow_NoteDirectBp(word >> 24);
    return true;
}

void ApplyXf(const TypedCmd& c)
{
    const uint64_t t0 = GfxNative_ProfEnabled() ? MonoNs() : 0;
    Render_Flush();
    for (uint32_t i = 0; i < c.count; i++)
    {
        const uint32_t a = c.addr + i;
        const uint32_t v = c.words[i];
        if (a < 0x1000) g_gx.xfmem[a] = v;
        else if (a < 0x1100) g_gx.xfregs[a - 0x1000] = v;
    }
    Render_OnXF(c.addr, c.count, false);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.typedApplyNs += MonoNs() - t0;
    }
}

void ApplyBp(const TypedCmd& c)
{
    const uint64_t t0 = GfxNative_ProfEnabled() ? MonoNs() : 0;
    GX_ApplyBpWord(c.addr);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.typedApplyNs += MonoNs() - t0;
    }
}

inline uint32_t GuestU32(uint32_t ea)
{
    const uint8_t* p = PhysPtr(ea);
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

inline void StoreGuestU32(uint32_t ea, uint32_t v)
{
    uint8_t* p = PhysPtr(ea);
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

inline void StoreGuestU16(uint32_t ea, uint16_t v)
{
    uint8_t* p = PhysPtr(ea);
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

inline uint32_t GxDataEa()
{
    // Prefer live __GXData pointer (sdata2); fall back to SNCP8P BSS object.
    const uint32_t p = GuestU32(kSncp8pGxDataPtrEa);
    return p ? p : kSncp8pGxDataEa;
}

inline uint32_t Rotl(uint32_t v, uint32_t n) { return (v << n) | (v >> (32 - n)); }

inline uint32_t Cntlzw(uint32_t v)
{
    if (!v) return 32;
    uint32_t n = 0;
    if ((v >> 16) == 0) { n += 16; v <<= 16; }
    if ((v >> 24) == 0) { n += 8; v <<= 8; }
    if ((v >> 28) == 0) { n += 4; v <<= 4; }
    if ((v >> 30) == 0) { n += 2; v <<= 2; }
    if ((v >> 31) == 0) n += 1;
    return n;
}

enum class BpKind : uint8_t { Blend, Z, Tev, PeBit };

bool DirectOrOriginalBp(PPCContext& ctx, uint8_t* base, uint32_t word, GA orig, BpKind kind)
{
    EnsureFlags();
    if (!s_directBp || GX_FifoRecordingToRAM())
    {
        if (s_directBp && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        g_ppcOriginals[(uint32_t)orig](ctx, base);
        return false;
    }
    if (!EnqueueBpWord(word))
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        g_ppcOriginals[(uint32_t)orig](ctx, base);
        return false;
    }
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        if (kind == BpKind::Blend) s_stats.directBpBlend++;
        else if (kind == BpKind::Z) s_stats.directBpZ++;
        else if (kind == BpKind::Tev) s_stats.directBpTev++;
        else s_stats.directBpPeBit++;
    }
    return true;
}

void ClearDirty(uint32_t gx)
{
    StoreGuestU16(gx + kGxDirtyFlagsOff, 0);
}
} // namespace

bool GfxNative_DirectEnabled()
{
    EnsureFlags();
    return s_direct != 0;
}

bool GfxNative_DirectBpEnabled()
{
    EnsureFlags();
    return s_directBp != 0;
}

bool GfxNative_DirectHotEnabled()
{
    EnsureFlags();
    return s_directHot != 0;
}

bool GfxNative_DirectKselEnabled()
{
    EnsureFlags();
    return s_directKsel != 0;
}

bool GfxNative_DirectTexObjEnabled()
{
    EnsureFlags();
    return s_directTexObj != 0;
}

bool GfxNative_ProfEnabled()
{
    EnsureFlags();
    return s_prof != 0;
}

bool GfxNative_DecodeProfEnabled()
{
    EnsureFlags();
    return s_decodeProf != 0;
}

const GfxNativeStats& GfxNative_Stats() { return s_stats; }

void GfxNative_StatsReset()
{
    std::lock_guard lk(s_statsMu);
    s_stats = {};
    GxShadow_Reset();
}

void GfxNative_LogStats(const char* tag)
{
    const GfxNativeStats& s = s_stats;
    LOG_INFO("A5e-{}: fifoIn={} topDecoded={} dlDecoded={} cmds={} draws={} dl={} xf={} bp={} bpNop={} cp={} "
             "directXf={} pos={} nrm={} tex={} directBp={} blend={} z={} tev={} peBit={} "
             "flush={} flushSkip={} scale={} inval={} ksel={} texObj={} texObjBp={} tlut={} "
             "fallback={} dlRec={} avoided={} "
             "gatherMs={:.1f} topDecodeMs={:.1f} nestDecodeMs={:.1f} typedMs={:.1f} "
             "catMs cp={:.1f} xfImm={:.1f} xfIdx={:.1f} bp={:.1f} dl={:.1f} draw={:.1f}",
             tag ? tag : "stats", s.fifoBytesIn, s.fifoBytesDecoded, s.dlBytesDecoded, s.cmdsDecoded, s.drawOps,
             s.dlCalls, s.xfLoads, s.bpLoads, s.bpNop, s.cpLoads, s.directXf, s.directPos, s.directNrm, s.directTex,
             s.directBp, s.directBpBlend, s.directBpZ, s.directBpTev, s.directBpPeBit, s.directBpFlush,
             s.directBpFlushSkip, s.directBpScale, s.directBpInval, s.directBpKsel, s.directTexObj,
             s.directTexObjBp, s.directTlut, s.directFallback, s.directDlRecord, s.fifoBytesAvoided,
             s.gatherNs / 1e6, s.decodeNs / 1e6, s.decodeNestedNs / 1e6, s.typedApplyNs / 1e6,
             s.catNs[(size_t)GfxDecodeCat::Cp] / 1e6, s.catNs[(size_t)GfxDecodeCat::XfImm] / 1e6,
             s.catNs[(size_t)GfxDecodeCat::XfIdx] / 1e6, s.catNs[(size_t)GfxDecodeCat::Bp] / 1e6,
             s.catNs[(size_t)GfxDecodeCat::DlCall] / 1e6, s.catNs[(size_t)GfxDecodeCat::Draw] / 1e6);
    GxShadow_LogProfile(tag);
}

bool GfxNative_EnqueueXfImm(uint32_t addr, uint32_t count, const uint32_t* words, bool wordsAreBE)
{
    if (!words || count == 0 || count > kXfWordsMax) return false;
    uint32_t host[kXfWordsMax];
    if (wordsAreBE)
        for (uint32_t i = 0; i < count; i++) host[i] = words[i];
    else
        std::memcpy(host, words, count * sizeof(uint32_t));
    return EnqueueXf(addr, count, host);
}

bool GfxNative_EnqueueBp(uint32_t word) { return EnqueueBpWord(word); }

void GfxNative_DrainReady(size_t fifoBytesConsumedTotal)
{
    for (;;)
    {
        uint32_t r = s_typedR.load(std::memory_order_relaxed);
        uint32_t w = s_typedW.load(std::memory_order_acquire);
        if (r == w) return;
        const TypedCmd& c = s_typed[r % kTypedCap];
        if (c.afterFifoW > fifoBytesConsumedTotal) return;
        if (c.kind == TypedKind::Bp) ApplyBp(c);
        else ApplyXf(c);
        s_typedR.store(r + 1, std::memory_order_release);
    }
}

void GfxNative_NoteGather(uint64_t bytes, uint64_t ns)
{
    if (!GfxNative_ProfEnabled()) return;
    std::lock_guard lk(s_statsMu);
    s_stats.fifoBytesIn += bytes;
    s_stats.gatherNs += ns;
}

void GfxNative_NoteDecode(uint64_t bytes, uint64_t cmds, uint64_t ns, bool topLevel)
{
    if (!GfxNative_ProfEnabled()) return;
    std::lock_guard lk(s_statsMu);
    s_stats.cmdsDecoded += cmds;
    if (topLevel)
    {
        s_stats.fifoBytesDecoded += bytes;
        s_stats.decodeNs += ns;
    }
    else
    {
        s_stats.dlBytesDecoded += bytes;
        s_stats.decodeNestedNs += ns;
    }
}

void GfxNative_NoteOpcode(uint8_t op)
{
    if (!GfxNative_ProfEnabled()) return;
    std::lock_guard lk(s_statsMu);
    if (op == 0x08) s_stats.cpLoads++;
    else if (op == 0x10) s_stats.xfLoads++;
    else if (op == 0x61) s_stats.bpLoads++;
    else if (op == 0x40) s_stats.dlCalls++;
    else if (op & 0x80) s_stats.drawOps++;
    else if (op == 0x20 || op == 0x28 || op == 0x30 || op == 0x38) s_stats.xfLoads++;
}

void GfxNative_NoteDecodeCat(GfxDecodeCat cat, uint64_t ns)
{
    if (!GfxNative_DecodeProfEnabled()) return;
    const size_t i = (size_t)cat;
    if (i >= (size_t)GfxDecodeCat::Count) return;
    std::lock_guard lk(s_statsMu);
    s_stats.catNs[i] += ns;
    s_stats.catCount[i]++;
}

void GfxNative_NoteBpNop()
{
    if (!GfxNative_ProfEnabled()) return;
    std::lock_guard lk(s_statsMu);
    s_stats.bpNop++;
}

// ---- PPC overrides (XF A5a/A5b) ----

enum class XfKind : uint8_t { Pos, Nrm, Tex };

static void DirectOrOriginalXf(PPCContext& ctx, uint8_t* base, uint32_t xfAddr, uint32_t count, GA orig, XfKind kind)
{
    EnsureFlags();
    if (!s_direct || GX_FifoRecordingToRAM())
    {
        if (s_direct && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        g_ppcOriginals[(uint32_t)orig](ctx, base);
        return;
    }
    const uint32_t mtxEa = ctx.r[3];
    uint32_t words[kXfWordsMax];
    for (uint32_t i = 0; i < count; i++)
        words[i] = GuestU32(mtxEa + 4 * i);
    if (!EnqueueXf(xfAddr, count, words))
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        g_ppcOriginals[(uint32_t)orig](ctx, base);
        return;
    }
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        if (kind == XfKind::Pos) s_stats.directPos++;
        else if (kind == XfKind::Nrm) s_stats.directNrm++;
        else s_stats.directTex++;
    }
}

PPC_OVERRIDE(GXLoadPosMtxImm)
{
    const uint32_t id = ctx.r[4] & 63u;
    DirectOrOriginalXf(ctx, base, id * 4u, 12u, GA::GXLoadPosMtxImm, XfKind::Pos);
}

PPC_OVERRIDE(GXLoadNrmMtxImm)
{
    const uint32_t id = ctx.r[4] & 63u;
    DirectOrOriginalXf(ctx, base, 0x400u + id * 3u, 9u, GA::GXLoadNrmMtxImm, XfKind::Nrm);
}

PPC_OVERRIDE(GXLoadTexMtxImm)
{
    // void GXLoadTexMtxImm(f32 mtx[][4], u32 id, GXTexMtxType type); SNCP8P @ 8036EF50.
    const uint32_t id = ctx.r[4];
    const uint32_t type = ctx.r[5];
    const uint32_t addr = (id >= 0x40u) ? ((id - 0x40u) * 4u + 0x500u) : (id * 4u);
    const uint32_t count = (type == 1u) ? 8u : 12u;
    DirectOrOriginalXf(ctx, base, addr, count, GA::GXLoadTexMtxImm, XfKind::Tex);
}

// ---- PPC overrides (BP A5c) — SNCP8P addresses verified against DOL / recomp ----

PPC_OVERRIDE(GXSetTevColorIn)
{
    // void GXSetTevColorIn(GXTevStageID stage, a,b,c,d); @ 8036D500
    EnsureFlags();
    if (!s_directBp || GX_FifoRecordingToRAM())
    {
        if (s_directBp && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        PPC_ORIGINAL(GXSetTevColorIn);
        return;
    }
    const uint32_t gx = GxDataEa();
    const uint32_t stageOff = (ctx.r[3] << 2) & 0xFFFFFFFCu;
    const uint32_t ea = gx + stageOff + kGxTevColorEnvOff;
    uint32_t word = GuestU32(ea);
    word = (Rotl(ctx.r[4], 12) & 0xF000) | (word & 0xFFFF0FFF);
    word = (Rotl(ctx.r[5], 8) & 0xF00) | (word & 0xFFFFF0FF);
    word = (Rotl(ctx.r[6], 4) & 0xF0) | (word & 0xFFFFFF0F);
    word = (ctx.r[7] & 0xF) | (word & 0xFFFFFFF0);
    if (!DirectOrOriginalBp(ctx, base, word, GA::GXSetTevColorIn, BpKind::Tev)) return;
    StoreGuestU32(ea, word);
    ClearDirty(gx);
}

PPC_OVERRIDE(GXSetTevAlphaIn)
{
    // @ 8036D540
    EnsureFlags();
    if (!s_directBp || GX_FifoRecordingToRAM())
    {
        if (s_directBp && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        PPC_ORIGINAL(GXSetTevAlphaIn);
        return;
    }
    const uint32_t gx = GxDataEa();
    const uint32_t stageOff = (ctx.r[3] << 2) & 0xFFFFFFFCu;
    const uint32_t ea = gx + stageOff + kGxTevAlphaEnvOff;
    uint32_t word = GuestU32(ea);
    word = (Rotl(ctx.r[4], 13) & 0xE000) | (word & 0xFFFF1FFF);
    word = (Rotl(ctx.r[5], 10) & 0x1C00) | (word & 0xFFFFE3FF);
    word = (Rotl(ctx.r[6], 7) & 0x380) | (word & 0xFFFFFC7F);
    word = (Rotl(ctx.r[7], 4) & 0x70) | (word & 0xFFFFFF8F);
    if (!DirectOrOriginalBp(ctx, base, word, GA::GXSetTevAlphaIn, BpKind::Tev)) return;
    StoreGuestU32(ea, word);
    ClearDirty(gx);
}

PPC_OVERRIDE(GXSetTevColorOp)
{
    // @ 8036D580 — matches recomp branch on op<=1 vs else
    EnsureFlags();
    if (!s_directBp || GX_FifoRecordingToRAM())
    {
        if (s_directBp && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        PPC_ORIGINAL(GXSetTevColorOp);
        return;
    }
    const uint32_t gx = GxDataEa();
    const uint32_t stageOff = (ctx.r[3] << 2) & 0xFFFFFFFCu;
    const uint32_t ea = gx + stageOff + kGxTevColorEnvOff;
    uint32_t word = GuestU32(ea);
    const uint32_t op = ctx.r[4];
    word = (Rotl(op, 18) & 0x40000) | (word & 0xFFFBFFFF);
    if ((int32_t)op <= 1)
    {
        word = (Rotl(ctx.r[6], 20) & 0x300000) | (word & 0xFFCFFFFF);
        word = (Rotl(ctx.r[5], 16) & 0x30000) | (word & 0xFFFCFFFF);
    }
    else
    {
        word = (Rotl(op, 19) & 0x300000) | (word & 0xFFCFFFFF);
        word = word | 0x30000;
    }
    word = (Rotl(ctx.r[7], 19) & 0x80000) | (word & 0xFFF7FFFF);
    word = (Rotl(ctx.r[8], 22) & 0xC00000) | (word & 0xFF3FFFFF);
    if (!DirectOrOriginalBp(ctx, base, word, GA::GXSetTevColorOp, BpKind::Tev)) return;
    StoreGuestU32(ea, word);
    ClearDirty(gx);
}

PPC_OVERRIDE(GXSetTevAlphaOp)
{
    // @ 8036D5E0 — mirror of ColorOp against alpha env
    EnsureFlags();
    if (!s_directBp || GX_FifoRecordingToRAM())
    {
        if (s_directBp && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        PPC_ORIGINAL(GXSetTevAlphaOp);
        return;
    }
    const uint32_t gx = GxDataEa();
    const uint32_t stageOff = (ctx.r[3] << 2) & 0xFFFFFFFCu;
    const uint32_t ea = gx + stageOff + kGxTevAlphaEnvOff;
    uint32_t word = GuestU32(ea);
    const uint32_t op = ctx.r[4];
    word = (Rotl(op, 18) & 0x40000) | (word & 0xFFFBFFFF);
    if ((int32_t)op <= 1)
    {
        word = (Rotl(ctx.r[6], 20) & 0x300000) | (word & 0xFFCFFFFF);
        word = (Rotl(ctx.r[5], 16) & 0x30000) | (word & 0xFFFCFFFF);
    }
    else
    {
        word = (Rotl(op, 19) & 0x300000) | (word & 0xFFCFFFFF);
        word = word | 0x30000;
    }
    word = (Rotl(ctx.r[7], 19) & 0x80000) | (word & 0xFFF7FFFF);
    word = (Rotl(ctx.r[8], 22) & 0xC00000) | (word & 0xFF3FFFFF);
    if (!DirectOrOriginalBp(ctx, base, word, GA::GXSetTevAlphaOp, BpKind::Tev)) return;
    StoreGuestU32(ea, word);
    ClearDirty(gx);
}

PPC_OVERRIDE(GXSetBlendMode)
{
    // void GXSetBlendMode(GXBlendMode type, src, dst, logic); @ 8036DFD0
    EnsureFlags();
    if (!s_directBp || GX_FifoRecordingToRAM())
    {
        if (s_directBp && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        PPC_ORIGINAL(GXSetBlendMode);
        return;
    }
    const uint32_t gx = GxDataEa();
    const uint32_t ea = gx + kGxCmode0Off;
    uint32_t word = GuestU32(ea);
    const uint32_t type = ctx.r[3];
    const uint32_t tM3 = type + 0xFFFFFFFDu; // type - 3
    const uint32_t tM2 = type + 0xFFFFFFFEu; // type - 2
    word = (Rotl(Cntlzw(tM3), 6) & 0x800) | (word & 0xFFFFF7FF);
    word = (type & 0x1) | (word & 0xFFFFFFFE);
    word = (Rotl(Cntlzw(tM2), 28) & 0x2) | (word & 0xFFFFFFFD);
    word = (Rotl(ctx.r[6], 12) & 0xF000) | (word & 0xFFFF0FFF);
    word = (Rotl(ctx.r[4], 8) & 0x700) | (word & 0xFFFFF8FF);
    word = (Rotl(ctx.r[5], 5) & 0xE0) | (word & 0xFFFFFF1F);
    if (!DirectOrOriginalBp(ctx, base, word, GA::GXSetBlendMode, BpKind::Blend)) return;
    StoreGuestU32(ea, word);
    ClearDirty(gx);
}

PPC_OVERRIDE(GXSetColorUpdate)
{
    // @ 8036E020
    EnsureFlags();
    if (!s_directBp || GX_FifoRecordingToRAM())
    {
        if (s_directBp && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        PPC_ORIGINAL(GXSetColorUpdate);
        return;
    }
    const uint32_t gx = GxDataEa();
    const uint32_t ea = gx + kGxCmode0Off;
    uint32_t word = GuestU32(ea);
    word = (Rotl(ctx.r[3], 3) & 0x8) | (word & 0xFFFFFFF7);
    if (!DirectOrOriginalBp(ctx, base, word, GA::GXSetColorUpdate, BpKind::PeBit)) return;
    StoreGuestU32(ea, word);
    ClearDirty(gx);
}

PPC_OVERRIDE(GXSetAlphaUpdate)
{
    // @ 8036E050
    EnsureFlags();
    if (!s_directBp || GX_FifoRecordingToRAM())
    {
        if (s_directBp && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        PPC_ORIGINAL(GXSetAlphaUpdate);
        return;
    }
    const uint32_t gx = GxDataEa();
    const uint32_t ea = gx + kGxCmode0Off;
    uint32_t word = GuestU32(ea);
    word = (Rotl(ctx.r[3], 4) & 0x10) | (word & 0xFFFFFFEF);
    if (!DirectOrOriginalBp(ctx, base, word, GA::GXSetAlphaUpdate, BpKind::PeBit)) return;
    StoreGuestU32(ea, word);
    ClearDirty(gx);
}

PPC_OVERRIDE(GXSetZMode)
{
    // void GXSetZMode(GXBool compare_enable, GXCompare func, GXBool update_enable); @ 8036E080
    EnsureFlags();
    if (!s_directBp || GX_FifoRecordingToRAM())
    {
        if (s_directBp && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        PPC_ORIGINAL(GXSetZMode);
        return;
    }
    const uint32_t gx = GxDataEa();
    const uint32_t ea = gx + kGxZmodeOff;
    uint32_t word = GuestU32(ea);
    word = (ctx.r[3] & 0x1) | (word & 0xFFFFFFFE);
    word = (Rotl(ctx.r[4], 1) & 0xE) | (word & 0xFFFFFFF1);
    word = (Rotl(ctx.r[5], 4) & 0x10) | (word & 0xFFFFFFEF);
    if (!DirectOrOriginalBp(ctx, base, word, GA::GXSetZMode, BpKind::Z)) return;
    StoreGuestU32(ea, word);
    ClearDirty(gx);
}

// ---- A5d hot BP / KSEL ----

static bool HotOrOriginal(PPCContext& ctx, uint8_t* base, bool enabled, GA orig)
{
    EnsureFlags();
    if (!enabled || GX_FifoRecordingToRAM())
    {
        if (enabled && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
            if (GX_FifoRecordingToRAM()) s_stats.directDlRecord++;
        }
        g_ppcOriginals[(uint32_t)orig](ctx, base);
        return false;
    }
    return true;
}

static bool EnqueueBpOrFallback(PPCContext& ctx, uint8_t* base, uint32_t word, GA orig)
{
    if (EnqueueBpWord(word)) return true;
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directFallback++;
    }
    g_ppcOriginals[(uint32_t)orig](ctx, base);
    return false;
}

PPC_OVERRIDE(GXFlushTextureState)
{
    // __GXFlushTextureState @ 8036D430 — re-emits IND_IMASK (BP 0x0F) from gx+0x174.
    if (!HotOrOriginal(ctx, base, s_directHot != 0, GA::GXFlushTextureState)) return;
    const uint32_t gx = GxDataEa();
    const uint32_t word = GuestU32(gx + kGxIndImaskOff);
    if (GxShadow_IsValid(0x0F))
    {
        ClearDirty(gx);
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directBpFlushSkip++;
            s_stats.fifoBytesAvoided += 5;
        }
        GX_FifoSkipPipeBytes(5);
        return;
    }
    if (!EnqueueBpOrFallback(ctx, base, word, GA::GXFlushTextureState)) return;
    ClearDirty(gx);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directBpFlush++;
    }
}

PPC_OVERRIDE(GXSetIndirectMask)
{
    // __GXSetIndirectMask @ 8036D400
    if (!HotOrOriginal(ctx, base, s_directHot != 0, GA::GXSetIndirectMask)) return;
    const uint32_t gx = GxDataEa();
    const uint32_t ea = gx + kGxIndImaskOff;
    uint32_t word = GuestU32(ea);
    word = (ctx.r[3] & 0xFF) | (word & 0xFFFFFF00);
    if (!EnqueueBpOrFallback(ctx, base, word, GA::GXSetIndirectMask)) return;
    StoreGuestU32(ea, word);
    ClearDirty(gx);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directBpFlush++;
    }
}

PPC_OVERRIDE(GXInvalidateTexAll)
{
    // @ 8036C930 — Flush; BP 0x661000; BP 0x661100; Flush. Preserve that order.
    if (!HotOrOriginal(ctx, base, s_directHot != 0, GA::GXInvalidateTexAll)) return;
    const uint32_t gx = GxDataEa();
    const uint32_t imask = GuestU32(gx + kGxIndImaskOff);
    const bool canSkipFlush = GxShadow_IsValid(0x0F);
    auto emitFlush = [&]() -> bool {
        if (canSkipFlush)
        {
            GX_FifoSkipPipeBytes(5);
            if (GfxNative_ProfEnabled())
            {
                std::lock_guard lk(s_statsMu);
                s_stats.directBpFlushSkip++;
                s_stats.fifoBytesAvoided += 5;
            }
            return true;
        }
        if (!EnqueueBpWord(imask)) return false;
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directBpFlush++;
        }
        return true;
    };
    if (!emitFlush())
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        PPC_ORIGINAL(GXInvalidateTexAll);
        return;
    }
    if (!EnqueueBpWord(0x661000u) || !EnqueueBpWord(0x661100u))
    {
        // Flush may already be queued; still emit remaining inval if possible, never ORIGINAL.
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        ClearDirty(gx);
        return;
    }
    (void)emitFlush();
    ClearDirty(gx);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directBpInval++;
    }
}

PPC_OVERRIDE(GXSetTexCoordBias)
{
    // @ 8036A8B0 — bias bits on SU_SSIZE (gx+0x108+4*coord)
    if (!HotOrOriginal(ctx, base, s_directHot != 0, GA::GXSetTexCoordBias)) return;
    const uint32_t gx = GxDataEa();
    const uint32_t stageOff = (ctx.r[3] << 2) & 0xFFFFFFFCu;
    const uint32_t ea = gx + stageOff + kGxSuSsizeOff;
    uint32_t word = GuestU32(ea);
    word = (Rotl(ctx.r[4], 18) & 0x40000) | (word & 0xFFFBFFFF);
    word = (Rotl(ctx.r[5], 19) & 0x80000) | (word & 0xFFF7FFFF);
    if (!EnqueueBpOrFallback(ctx, base, word, GA::GXSetTexCoordBias)) return;
    StoreGuestU32(ea, word);
    ClearDirty(gx);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directBpScale++;
    }
}

PPC_OVERRIDE(GXSetTexCoordScale)
{
    // @ 8036C9C0 — void GXSetTexCoordScale(coord, enable, s, t); enable==0 → dirty only.
    if (!HotOrOriginal(ctx, base, s_directHot != 0, GA::GXSetTexCoordScale)) return;
    const uint32_t gx = GxDataEa();
    const uint32_t coord = ctx.r[3];
    const uint32_t enable = ctx.r[4];
    uint32_t dirty = GuestU32(gx + kGxTexScaleDirtyOff);
    const uint32_t bit = (coord & 0x20) ? 0u : (1u << (coord & 31));
    const uint32_t enBit = (coord & 0x20) ? 0u : (enable << (coord & 31));
    dirty = (dirty & ~bit) | enBit;
    StoreGuestU32(gx + kGxTexScaleDirtyOff, dirty);
    if (enable == 0)
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directBpScale++;
        }
        return;
    }
    const uint32_t stageOff = (coord << 2) & 0xFFFFFFFCu;
    const uint32_t eaS = gx + stageOff + kGxSuSsizeOff;
    const uint32_t eaT = gx + stageOff + kGxSuTsizeOff;
    const uint32_t sVal = (ctx.r[5] + 0xFFFFFFFFu) & 0xFFFF; // s-1
    const uint32_t tVal = (ctx.r[6] + 0xFFFFFFFFu) & 0xFFFF; // t-1
    uint32_t wordS = GuestU32(eaS);
    uint32_t wordT = GuestU32(eaT);
    wordS = sVal | (wordS & 0xFFFF0000);
    wordT = tVal | (wordT & 0xFFFF0000);
    if (!EnqueueBpWord(wordS) || !EnqueueBpWord(wordT))
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        PPC_ORIGINAL(GXSetTexCoordScale);
        return;
    }
    StoreGuestU32(eaS, wordS);
    StoreGuestU32(eaT, wordT);
    ClearDirty(gx);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directBpScale++;
    }
}

PPC_OVERRIDE(GXSetTexCoordCylWrap)
{
    // @ 8036CA40 — cyl-wrap bits on SU_SSIZE/TSIZE; BP only if dirty bit clear
    if (!HotOrOriginal(ctx, base, s_directHot != 0, GA::GXSetTexCoordCylWrap)) return;
    const uint32_t gx = GxDataEa();
    const uint32_t coord = ctx.r[3];
    const uint32_t stageOff = (coord << 2) & 0xFFFFFFFCu;
    const uint32_t eaS = gx + stageOff + kGxSuSsizeOff;
    const uint32_t eaT = gx + stageOff + kGxSuTsizeOff;
    uint32_t wordS = GuestU32(eaS);
    uint32_t wordT = GuestU32(eaT);
    wordS = (Rotl(ctx.r[4], 17) & 0x20000) | (wordS & 0xFFFDFFFF);
    wordT = (Rotl(ctx.r[5], 17) & 0x20000) | (wordT & 0xFFFDFFFF);
    StoreGuestU32(eaS, wordS);
    StoreGuestU32(eaT, wordT);
    const uint32_t bit = (coord & 0x20) ? 0u : (1u << (coord & 31));
    // Original: if ((dirty & bit) == 0) return without FIFO; else emit both BP words.
    if ((GuestU32(gx + kGxTexScaleDirtyOff) & bit) == 0)
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directBpScale++;
        }
        return;
    }
    if (!EnqueueBpWord(wordS) || !EnqueueBpWord(wordT))
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        PPC_ORIGINAL(GXSetTexCoordCylWrap);
        return;
    }
    ClearDirty(gx);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directBpScale++;
    }
}

PPC_OVERRIDE(GXSetTevSwapMode)
{
    // @ 8036D2B0 — packs ras/tex swap sels into KSEL0 (BP 0xF6) at gx+0x170; dirty |= 3
    if (!HotOrOriginal(ctx, base, s_directKsel != 0, GA::GXSetTevSwapMode)) return;
    const uint32_t gx = GxDataEa();
    uint32_t ras = ctx.r[4];
    uint32_t tex = ctx.r[5];
    if (tex == 255) tex = 0;
    if (ras == 255) ras = 0;
    const uint32_t stage = ctx.r[3];
    const uint32_t ea = gx + kGxTevKsel0Off;
    uint32_t word = GuestU32(ea);
    switch (stage)
    {
    case 0:
        word = (tex & 0x7) | (word & 0xFFFFFFF8);
        word = (Rotl(ras, 3) & 0x38) | (word & 0xFFFFFFC7);
        break;
    case 1:
        word = (Rotl(tex, 6) & 0x1C0) | (word & 0xFFFFFE3F);
        word = (Rotl(ras, 9) & 0xE00) | (word & 0xFFFFF1FF);
        break;
    case 2:
        word = (Rotl(tex, 12) & 0x7000) | (word & 0xFFFF8FFF);
        word = (Rotl(ras, 15) & 0x38000) | (word & 0xFFFC7FFF);
        break;
    case 3:
        word = (Rotl(tex, 18) & 0x1C0000) | (word & 0xFFE3FFFF);
        word = (Rotl(ras, 21) & 0xE00000) | (word & 0xFF1FFFFF);
        break;
    default:
        break;
    }
    if (!EnqueueBpOrFallback(ctx, base, word, GA::GXSetTevSwapMode)) return;
    StoreGuestU32(ea, word);
    StoreGuestU32(gx + kGxDirtyStateOff, GuestU32(gx + kGxDirtyStateOff) | 0x3u);
    ClearDirty(gx);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directBpKsel++;
    }
}

// ---- A5e texture-object / TLUT ----

inline uint8_t GuestU8(uint32_t ea) { return *PhysPtr(ea); }

// Guest callback with r3/r4; restores ctx, returns callee r3.
static uint32_t CallGuestR3(PPCContext& ctx, uint8_t* base, uint32_t func, uint32_t r3, uint32_t r4)
{
    PPCContext saved = ctx;
    ctx.r[1] -= 0x100;
    ctx.r[3] = r3;
    ctx.r[4] = r4;
    ctx.lr = 0;
    ppc_call_indirect(ctx, base, func);
    const uint32_t ret = ctx.r[3];
    ctx = saved;
    return ret;
}

static uint8_t TexBpRegId(uint32_t which, uint32_t map)
{
    // which: 0=mode0 1=mode1 2=img0 3=img1 4=img2 5=img3 6=tlut
    return GuestU8(kSncp8pGxTexBpIdTableEa + which * 8u + map);
}

static uint32_t PackTexBp(uint8_t regId, uint32_t low24)
{
    return (Rotl((uint32_t)regId, 24) & 0xFF000000u) | (low24 & 0xFFFFFFu);
}

// Returns BP words emitted, or 0 on failure (caller must not ORIGINAL after partial enqueue).
static uint32_t ApplyLoadTexObjPreLoaded(PPCContext& ctx, uint8_t* base, uint32_t obj, uint32_t region,
                                         uint32_t map)
{
    if (!obj || !region || map > 7u) return 0;

    const uint32_t mode0 = PackTexBp(TexBpRegId(0, map), GuestU32(obj));
    const uint32_t mode1 = PackTexBp(TexBpRegId(1, map), GuestU32(obj + 4));
    const uint32_t img0 = PackTexBp(TexBpRegId(2, map), GuestU32(obj + 8));
    const uint32_t img1 = PackTexBp(TexBpRegId(3, map), GuestU32(region));
    const uint32_t img2 = PackTexBp(TexBpRegId(4, map), GuestU32(region + 4));
    uint32_t img3 = PackTexBp(TexBpRegId(5, map), GuestU32(obj + 0xC));

    if (!EnqueueBpWord(mode0) || !EnqueueBpWord(mode1) || !EnqueueBpWord(img0) || !EnqueueBpWord(img1) ||
        !EnqueueBpWord(img2))
        return 0;

    StoreGuestU32(obj, mode0);
    StoreGuestU32(obj + 4, mode1);
    StoreGuestU32(obj + 8, img0);
    StoreGuestU32(region, img1);
    StoreGuestU32(region + 4, img2);
    StoreGuestU32(obj + 0xC, img3);

    if (!EnqueueBpWord(img3)) return 0;
    uint32_t emitted = 6;

    // flags+0x1F bit1 clear ⇒ colour-index: load TLUT BP via region callback @ gx+0x51C
    if ((GuestU8(obj + 0x1F) & 0x2) == 0)
    {
        const uint32_t gx = GxDataEa();
        const uint32_t cb = GuestU32(gx + kGxTlutRegionCbOff);
        if (!cb) return 0;
        const uint32_t tlutName = GuestU32(obj + 0x18);
        const uint32_t tlutRegion = CallGuestR3(ctx, base, cb, tlutName, 0);
        if (!tlutRegion) return 0;
        const uint32_t tlutWord = PackTexBp(TexBpRegId(6, map), GuestU32(tlutRegion + 4));
        StoreGuestU32(tlutRegion + 4, tlutWord);
        if (!EnqueueBpWord(tlutWord)) return 0;
        emitted++;
    }

    const uint32_t gx = GxDataEa();
    const uint32_t mapOff = (map << 2) & 0xFFFFFFFCu;
    StoreGuestU32(gx + kGxTexImg0CacheOff + mapOff, GuestU32(obj + 8));
    StoreGuestU32(gx + kGxTexMode0CacheOff + mapOff, GuestU32(obj));
    StoreGuestU32(gx + kGxDirtyStateOff, GuestU32(gx + kGxDirtyStateOff) | 0x1u);
    ClearDirty(gx);
    return emitted;
}

PPC_OVERRIDE(GXLoadTexObjPreLoaded)
{
    // void GXLoadTexObjPreLoaded(GXTexObj* obj, GXTexRegion* region, GXTexMapID id); @ 8036C5C0
    if (!HotOrOriginal(ctx, base, s_directTexObj != 0, GA::GXLoadTexObjPreLoaded)) return;
    const uint32_t obj = ctx.r[3];
    const uint32_t region = ctx.r[4];
    const uint32_t map = ctx.r[5];
    const uint32_t n = ApplyLoadTexObjPreLoaded(ctx, base, obj, region, map);
    if (!n)
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        // Partial BP may already be queued — do not ORIGINAL (would double-apply).
        return;
    }
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directTexObj++;
        s_stats.directTexObjBp += n;
    }
}

PPC_OVERRIDE(GXLoadTexObj)
{
    // void GXLoadTexObj(GXTexObj* obj, GXTexMapID id); @ 8036C730 — region via gx+0x518 then PreLoaded.
    if (!HotOrOriginal(ctx, base, s_directTexObj != 0, GA::GXLoadTexObj)) return;
    const uint32_t obj = ctx.r[3];
    const uint32_t map = ctx.r[4];
    if (!obj || map > 7u)
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        PPC_ORIGINAL(GXLoadTexObj);
        return;
    }
    const uint32_t gx = GxDataEa();
    const uint32_t cb = GuestU32(gx + kGxTexRegionCbOff);
    if (!cb)
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        PPC_ORIGINAL(GXLoadTexObj);
        return;
    }
    const uint32_t region = CallGuestR3(ctx, base, cb, obj, map);
    if (!region)
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        PPC_ORIGINAL(GXLoadTexObj);
        return;
    }
    const uint32_t n = ApplyLoadTexObjPreLoaded(ctx, base, obj, region, map);
    if (!n)
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        return;
    }
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directTexObj++;
        s_stats.directTexObjBp += n;
    }
}

static bool EmitIndImaskFlush()
{
    const uint32_t imask = GuestU32(GxDataEa() + kGxIndImaskOff);
    if (GxShadow_IsValid(0x0F))
    {
        GX_FifoSkipPipeBytes(5);
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directBpFlushSkip++;
            s_stats.fifoBytesAvoided += 5;
        }
        return true;
    }
    if (!EnqueueBpWord(imask)) return false;
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directBpFlush++;
    }
    return true;
}

PPC_OVERRIDE(GXLoadTlut)
{
    // void GXLoadTlut(GXTlutObj* obj, u32 tlut); @ 8036C7C0
    // Flush; BP obj+4; BP region+0; Flush; merge TMEM into obj; copy to region.
    if (!HotOrOriginal(ctx, base, s_directTexObj != 0, GA::GXLoadTlut)) return;
    const uint32_t obj = ctx.r[3];
    const uint32_t tlut = ctx.r[4];
    if (!obj)
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        PPC_ORIGINAL(GXLoadTlut);
        return;
    }
    const uint32_t gx = GxDataEa();
    const uint32_t cb = GuestU32(gx + kGxTlutRegionCbOff);
    if (!cb)
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        PPC_ORIGINAL(GXLoadTlut);
        return;
    }
    const uint32_t region = CallGuestR3(ctx, base, cb, tlut, 0);
    if (!region)
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        PPC_ORIGINAL(GXLoadTlut);
        return;
    }
    if (!EmitIndImaskFlush())
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        return;
    }
    const uint32_t wObj = GuestU32(obj + 4);
    const uint32_t wReg = GuestU32(region);
    if (!EnqueueBpWord(wObj) || !EnqueueBpWord(wReg))
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        return;
    }
    if (!EmitIndImaskFlush())
    {
        if (GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        return;
    }
    // Merge TMEM address bits from region into obj, then mirror into region (+0,+4,+8,+C).
    uint32_t merged = GuestU32(obj);
    merged = (GuestU32(region) & 0x3FFu) | (merged & 0xFFFFFC00u);
    StoreGuestU32(region + 4, merged);
    StoreGuestU32(region + 8, GuestU32(obj + 4));
    StoreGuestU32(region + 0xC, GuestU32(obj + 8));
    StoreGuestU32(obj, merged);
    ClearDirty(gx);
    if (GfxNative_ProfEnabled())
    {
        std::lock_guard lk(s_statsMu);
        s_stats.directTlut++;
        s_stats.directTexObjBp += 2; // the two TLUT BP words (flushes counted separately)
    }
}
