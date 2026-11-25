// A5a/A5b: opt-in direct GX SDK → typed GX-thread commands (ordered vs FIFO).
#include "gfx_native.h"
#include "gx_state.h"
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

struct TypedXf
{
    size_t afterFifoW = 0;
    uint32_t addr = 0;
    uint32_t count = 0;
    uint32_t words[kXfWordsMax]{};
};

std::atomic<uint32_t> s_typedW{ 0 };
std::atomic<uint32_t> s_typedR{ 0 };
TypedXf s_typed[kTypedCap];

GfxNativeStats s_stats{};
std::mutex s_statsMu;

int s_direct = -1;
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
    if (s_prof < 0)
    {
        const char* e = std::getenv("SCR_A5A_PROF");
        const char* e2 = std::getenv("SCR_A5B_PROF");
        s_prof = (e && e[0] && e[0] != '0') || (e2 && e2[0] && e2[0] != '0') || s_direct ? 1 : 0;
        s_decodeProf = (e2 && e2[0] == '1') || s_prof ? 1 : 0;
    }
}

bool EnqueueXf(uint32_t addr, uint32_t count, const uint32_t* hostWords)
{
    if (count == 0 || count > kXfWordsMax || !hostWords) return false;
    EnsureFlags();
    GX_FifoPublishPending();
    const size_t after = GX_FifoBytesWritten();
    uint32_t w = s_typedW.load(std::memory_order_relaxed);
    uint32_t r = s_typedR.load(std::memory_order_acquire);
    if (w - r >= kTypedCap)
    {
        auto lk = GX_SyncGPU();
        (void)lk;
        r = s_typedR.load(std::memory_order_acquire);
        if (w - r >= kTypedCap) return false;
    }
    TypedXf& slot = s_typed[w % kTypedCap];
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

void ApplyXf(const TypedXf& c)
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

inline uint32_t GuestU32(uint32_t ea)
{
    const uint8_t* p = PhysPtr(ea);
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
} // namespace

bool GfxNative_DirectEnabled()
{
    EnsureFlags();
    return s_direct != 0;
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
}

void GfxNative_LogStats(const char* tag)
{
    const GfxNativeStats& s = s_stats;
    LOG_INFO("A5b-{}: fifoIn={} topDecoded={} dlDecoded={} cmds={} draws={} dl={} xf={} bp={} bpNop={} cp={} "
             "directXf={} pos={} nrm={} tex={} fallback={} dlRec={} avoided={} "
             "gatherMs={:.1f} topDecodeMs={:.1f} nestDecodeMs={:.1f} typedMs={:.1f} "
             "catMs cp={:.1f} xfImm={:.1f} xfIdx={:.1f} bp={:.1f} dl={:.1f} draw={:.1f}",
             tag ? tag : "stats", s.fifoBytesIn, s.fifoBytesDecoded, s.dlBytesDecoded, s.cmdsDecoded, s.drawOps,
             s.dlCalls, s.xfLoads, s.bpLoads, s.bpNop, s.cpLoads, s.directXf, s.directPos, s.directNrm, s.directTex,
             s.directFallback, s.directDlRecord, s.fifoBytesAvoided, s.gatherNs / 1e6, s.decodeNs / 1e6,
             s.decodeNestedNs / 1e6, s.typedApplyNs / 1e6,
             s.catNs[(size_t)GfxDecodeCat::Cp] / 1e6, s.catNs[(size_t)GfxDecodeCat::XfImm] / 1e6,
             s.catNs[(size_t)GfxDecodeCat::XfIdx] / 1e6, s.catNs[(size_t)GfxDecodeCat::Bp] / 1e6,
             s.catNs[(size_t)GfxDecodeCat::DlCall] / 1e6, s.catNs[(size_t)GfxDecodeCat::Draw] / 1e6);
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

void GfxNative_DrainReady(size_t fifoBytesConsumedTotal)
{
    for (;;)
    {
        uint32_t r = s_typedR.load(std::memory_order_relaxed);
        uint32_t w = s_typedW.load(std::memory_order_acquire);
        if (r == w) return;
        const TypedXf& c = s_typed[r % kTypedCap];
        if (c.afterFifoW > fifoBytesConsumedTotal) return;
        ApplyXf(c);
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

// ---- PPC overrides ----

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
    // id < 0x40 → XF addr id*4; else (id-0x40)*4 + 0x500. type==1 (2x4) → 8 floats else 12.
    const uint32_t id = ctx.r[4];
    const uint32_t type = ctx.r[5];
    const uint32_t addr = (id >= 0x40u) ? ((id - 0x40u) * 4u + 0x500u) : (id * 4u);
    const uint32_t count = (type == 1u) ? 8u : 12u;
    DirectOrOriginalXf(ctx, base, addr, count, GA::GXLoadTexMtxImm, XfKind::Tex);
}
