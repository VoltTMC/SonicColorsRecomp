// A5a: opt-in direct GX SDK → typed GX-thread commands (ordered vs FIFO).
#include "gfx_native.h"
#include "gx_state.h"
#include "../game_addrs.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../platform/platform.h"

#include <atomic>
#include <cstring>
#include <mutex>

// Defined in gx_fifo.cpp (A5a hooks).
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
            LOG_INFO("A5a: DIRECT=1 — GXLoadPosMtxImm@8036EE10 / GXLoadNrmMtxImm@8036EE90 → typed XF");
    }
    if (s_prof < 0)
    {
        const char* e = std::getenv("SCR_A5A_PROF");
        s_prof = (e && e[0] && e[0] != '0') || s_direct ? 1 : 0;
    }
}

bool EnqueueXf(uint32_t addr, uint32_t count, const uint32_t* hostWords)
{
    if (count == 0 || count > kXfWordsMax || !hostWords) return false;
    EnsureFlags();
    // Finish any published FIFO bytes so afterFifoW matches visible pipe progress.
    GX_FifoPublishPending();
    const size_t after = GX_FifoBytesWritten();
    uint32_t w = s_typedW.load(std::memory_order_relaxed);
    uint32_t r = s_typedR.load(std::memory_order_acquire);
    if (w - r >= kTypedCap)
    {
        // Backpressure: sync GPU so the typed ring drains.
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
    // Pipe bytes the SDK would have written: 0x10 + u32 header + count*u32.
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

const GfxNativeStats& GfxNative_Stats() { return s_stats; }

void GfxNative_StatsReset()
{
    std::lock_guard lk(s_statsMu);
    s_stats = {};
}

void GfxNative_LogStats(const char* tag)
{
    const GfxNativeStats& s = s_stats;
    LOG_INFO("A5a-{}: fifoIn={} decoded={} cmds={} draws={} dl={} xf={} bp={} cp={} "
             "directXf={} fallback={} avoided={} gatherMs={:.1f} decodeMs={:.1f} typedMs={:.1f}",
             tag ? tag : "stats", s.fifoBytesIn, s.fifoBytesDecoded, s.cmdsDecoded, s.drawOps, s.dlCalls,
             s.xfLoads, s.bpLoads, s.cpLoads, s.directXf, s.directFallback, s.fifoBytesAvoided,
             s.gatherNs / 1e6, s.decodeNs / 1e6, s.typedApplyNs / 1e6);
}

bool GfxNative_EnqueueXfImm(uint32_t addr, uint32_t count, const uint32_t* words, bool wordsAreBE)
{
    if (!words || count == 0 || count > kXfWordsMax) return false;
    uint32_t host[kXfWordsMax];
    if (wordsAreBE)
    {
        for (uint32_t i = 0; i < count; i++)
        {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(words + i);
            // Caller passed BE words already packed as host u32 from GuestU32 — treat as host value.
            host[i] = words[i];
            (void)p;
        }
    }
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

void GfxNative_NoteDecode(uint64_t bytes, uint64_t cmds, uint64_t ns)
{
    if (!GfxNative_ProfEnabled()) return;
    std::lock_guard lk(s_statsMu);
    s_stats.fifoBytesDecoded += bytes;
    s_stats.cmdsDecoded += cmds;
    s_stats.decodeNs += ns;
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

// ---- PPC overrides (only active bodies when DIRECT=1; else call original) ----

static void DirectOrOriginalXf(PPCContext& ctx, uint8_t* base, uint32_t xfAddr, uint32_t count, GA orig)
{
    EnsureFlags();
    if (!s_direct || GX_FifoRecordingToRAM())
    {
        if (s_direct && GfxNative_ProfEnabled())
        {
            std::lock_guard lk(s_statsMu);
            s_stats.directFallback++;
        }
        g_ppcOriginals[(uint32_t)orig](ctx, base);
        return;
    }
    // Guest matrices are big-endian f32 in MEM1/MEM2; GuestU32 yields host bit patterns.
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
    }
}

PPC_OVERRIDE(GXLoadPosMtxImm)
{
    // void GXLoadPosMtxImm(f32 mtx[3][4], u32 id); XF addr = id*4, 12 floats. SNCP8P @ 8036EE10.
    const uint32_t id = ctx.r[4] & 63u;
    DirectOrOriginalXf(ctx, base, id * 4u, 12u, GA::GXLoadPosMtxImm);
}

PPC_OVERRIDE(GXLoadNrmMtxImm)
{
    // void GXLoadNrmMtxImm(f32 mtx[3][4], u32 id); XF addr = 0x400+id*3, 9 floats. SNCP8P @ 8036EE90.
    // Wire uses id*4 then +0x400 (DOL); XF nrm slots are 3 words each — match SDK id*3.
    const uint32_t id = ctx.r[4] & 63u;
    DirectOrOriginalXf(ctx, base, 0x400u + id * 3u, 9u, GA::GXLoadNrmMtxImm);
}
