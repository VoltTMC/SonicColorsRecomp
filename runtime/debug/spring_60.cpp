// Spring soft-lock investigation / experimental guest-60 compatibility.
//
// Diag guest60c (stg110): soft-lock is CObjWideSpring — wact every guest frame
// while overlap persists (pattern A). CObjSpring path unused (col=act=0).
// Wide collide (fn_800B90E0) calls activate for type 0x9000 with no cooldown;
// +0xA8 bit0 is a one-frame arm flag, not a re-trigger gate.
#include "../game_addrs.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../perf.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" uint32_t g_cfgFrameRate;

namespace {

bool EnvOn(const char* name)
{
    const char* v = std::getenv(name);
    return v && v[0] && v[0] != '0';
}

bool GateEnabled()
{
    // Default on for frame_rate 60 / --guest-60; SCR_SPRING_CD_GATE still forces on;
    // SCR_SPRING_CD_GATE=0 disables even at 60.
    static int c = -1;
    if (c < 0)
    {
        const char* v = std::getenv("SCR_SPRING_CD_GATE");
        if (v && v[0] == '0') c = 0;
        else if (v && v[0] && v[0] != '0') c = 1;
        else c = -2;  // derive from fps / guest-60
    }
    if (c >= 0) return c != 0;
    return g_cfgFrameRate >= 60 || Perf_Guest60Forced();
}

bool DiagEnabled()
{
    static int c = -1;
    if (c < 0) c = EnvOn("SCR_SPRING_DIAG") ? 1 : 0;
    return c != 0;
}

bool IsGuestEa(uint32_t ea)
{
    const uint32_t top = ea >> 28;
    return ea && (top == 0x8 || top == 0x9);
}

float ReadF32(uint32_t addr)
{
    uint32_t u = Read32(addr);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

// Match CObjSpring native +0x188 hold (0.5s). Guest frames ≈ 0.5 / (wait/60).
uint32_t WideHoldFrames()
{
    const uint32_t w = std::max(1u, Perf_WaitFields());
    return (30u + w - 1u) / w;
}

std::atomic<uint32_t> s_colN{ 0 }, s_actN{ 0 }, s_actBlocked{ 0 }, s_launchN{ 0 }, s_updN{ 0 };
std::atomic<uint32_t> s_wideCol{ 0 }, s_wideAct{ 0 }, s_wideBlocked{ 0 }, s_wideUpd{ 0 }, s_poleCol{ 0 };
std::atomic<uint32_t> s_aliveLogged{ 0 };

struct WideCdSlot { uint32_t ea; uint64_t fr; };
WideCdSlot s_wideCd[16]{};

bool WideActivateBlocked(uint32_t self, uint64_t fr)
{
    const uint32_t hold = WideHoldFrames();
    for (WideCdSlot& s : s_wideCd)
    {
        if (s.ea == self)
            return fr < s.fr + hold;
    }
    return false;
}

void WideActivateArmed(uint32_t self, uint64_t fr)
{
    for (WideCdSlot& s : s_wideCd)
    {
        if (s.ea == self) { s.fr = fr; return; }
    }
    for (WideCdSlot& s : s_wideCd)
    {
        if (!s.ea) { s.ea = self; s.fr = fr; return; }
    }
    // Evict oldest.
    WideCdSlot* victim = &s_wideCd[0];
    for (WideCdSlot& s : s_wideCd)
        if (s.fr < victim->fr) victim = &s;
    victim->ea = self;
    victim->fr = fr;
}

void Diag(const char* tag, uint32_t self, uint32_t other, float cd, uint32_t flags,
          uint32_t typeOrExtra = 0)
{
    if (!DiagEnabled()) return;
    const uint64_t fr = Perf_GuestFrameIndex();
    static thread_local uint32_t perFrame = 0;
    static thread_local uint64_t lastFr = ~0ull;
    if (fr != lastFr) { lastFr = fr; perFrame = 0; }
    if (perFrame >= 8) return;
    ++perFrame;
    LOG_INFO("spring[{}]: fr={} self={:08x} other={:08x} cd={:.4f} f0={:08x} x={:08x} "
             "col={} act={} block={} launch={} upd={} wcol={} wact={} wblock={} wupd={} pcol={}",
             tag, fr, self, other, cd, flags, typeOrExtra,
             s_colN.load(std::memory_order_relaxed),
             s_actN.load(std::memory_order_relaxed),
             s_actBlocked.load(std::memory_order_relaxed),
             s_launchN.load(std::memory_order_relaxed),
             s_updN.load(std::memory_order_relaxed),
             s_wideCol.load(std::memory_order_relaxed),
             s_wideAct.load(std::memory_order_relaxed),
             s_wideBlocked.load(std::memory_order_relaxed),
             s_wideUpd.load(std::memory_order_relaxed),
             s_poleCol.load(std::memory_order_relaxed));
}

}  // namespace

// Vtable collision entry: dispatches on collider type (+4) to activate / alts.
PPC_OVERRIDE(ObjSpringCollide)
{
    const uint32_t self = ctx.r[3];
    const uint32_t other = ctx.r[4];
    s_colN.fetch_add(1, std::memory_order_relaxed);
    float cd = 0.f;
    uint32_t flags = 0, typ = 0;
    if (IsGuestEa(self))
    {
        cd = ReadF32(self + 0x188);
        flags = Read32(self + 0xF0);
    }
    if (IsGuestEa(other))
        typ = Read32(other + 0x4);
    Diag("col", self, other, cd, flags, typ);
    PPC_ORIGINAL(ObjSpringCollide);
}

PPC_OVERRIDE(ObjSpringActivate)
{
    const uint32_t self = ctx.r[3];
    const uint32_t other = ctx.r[4];
    float cd = 0.f;
    uint32_t flags = 0;
    bool bit15 = false;
    if (IsGuestEa(self))
    {
        cd = ReadF32(self + 0x188);
        flags = Read32(self + 0xF0);
        bit15 = (flags & 0x10000u) != 0;
    }

    s_actN.fetch_add(1, std::memory_order_relaxed);

    if (GateEnabled() && IsGuestEa(self) && !bit15)
        Write32(self + 0xF0, flags | 0x10000u);

    if ((bit15 || GateEnabled()) && cd > 0.f && IsGuestEa(self) && IsGuestEa(other))
    {
        if (Read32(self + 0x184) == Read32(other + 0x8))
            s_actBlocked.fetch_add(1, std::memory_order_relaxed);
    }

    Diag(GateEnabled() ? "act+gate" : "act", self, other, cd, flags);
    PPC_ORIGINAL(ObjSpringActivate);

    if (DiagEnabled() && IsGuestEa(self))
    {
        const float cd2 = ReadF32(self + 0x188);
        const uint32_t f2 = Read32(self + 0xF0);
        if (cd2 != cd || f2 != flags)
            Diag("act-out", self, other, cd2, f2);
    }
}

PPC_OVERRIDE(ObjSpringLaunch)
{
    s_launchN.fetch_add(1, std::memory_order_relaxed);
    const uint32_t self = ctx.r[3];
    float cd = IsGuestEa(self) ? ReadF32(self + 0x188) : 0.f;
    uint32_t flags = IsGuestEa(self) ? Read32(self + 0xF0) : 0;
    Diag("launch", self, 0, cd, flags);
    PPC_ORIGINAL(ObjSpringLaunch);
}

// Per-frame: proof-of-life for first few ticks, then only when active.
PPC_OVERRIDE(ObjSpringUpdate)
{
    s_updN.fetch_add(1, std::memory_order_relaxed);
    const uint32_t self = ctx.r[3];
    if (DiagEnabled() && IsGuestEa(self))
    {
        const float cd = ReadF32(self + 0x188);
        const uint32_t flags = Read32(self + 0xF0);
        const float phase = ReadF32(self + 0xE0);
        const uint32_t alive = s_aliveLogged.fetch_add(1, std::memory_order_relaxed);
        if (alive < 4)
            Diag("alive", self, 0, cd, flags, (uint32_t)(phase * 1000.f));
        else if (cd > 0.f || (flags & 0x3u) != 0)
        {
            static uint64_t lastUpdFr = 0;
            const uint64_t fr = Perf_GuestFrameIndex();
            if (fr >= lastUpdFr + 15)
            {
                lastUpdFr = fr;
                Diag("upd", self, 0, cd, flags, (uint32_t)(phase * 1000.f));
            }
        }
    }
    PPC_ORIGINAL(ObjSpringUpdate);
}

PPC_OVERRIDE(ObjWideSpringCollide)
{
    s_wideCol.fetch_add(1, std::memory_order_relaxed);
    const uint32_t self = ctx.r[3];
    const uint32_t other = ctx.r[4];
    uint32_t flags = IsGuestEa(self) ? Read32(self + 0xA8) : 0;
    uint32_t typ = IsGuestEa(other) ? Read32(other + 0x4) : 0;
    Diag("wcol", self, other, 0.f, flags, typ);
    PPC_ORIGINAL(ObjWideSpringCollide);
}

PPC_OVERRIDE(ObjWideSpringUpdate)
{
    s_wideUpd.fetch_add(1, std::memory_order_relaxed);
    const uint32_t self = ctx.r[3];
    if (DiagEnabled() && IsGuestEa(self))
    {
        const uint32_t flags = Read32(self + 0xA8);
        static std::atomic<uint32_t> alive{ 0 };
        if (alive.fetch_add(1, std::memory_order_relaxed) < 4)
            Diag("walive", self, 0, 0.f, flags);
        else if (flags & 1u)
        {
            static uint64_t lastFr = 0;
            const uint64_t fr = Perf_GuestFrameIndex();
            if (fr >= lastFr + 15)
            {
                lastFr = fr;
                Diag("wupd", self, 0, 0.f, flags);
            }
        }
    }
    PPC_ORIGINAL(ObjWideSpringUpdate);
}

PPC_OVERRIDE(ObjWideSpringActivate)
{
    s_wideAct.fetch_add(1, std::memory_order_relaxed);
    const uint32_t self = ctx.r[3];
    const uint32_t other = ctx.r[4];
    uint32_t flags = IsGuestEa(self) ? Read32(self + 0xA8) : 0;
    const uint64_t fr = Perf_GuestFrameIndex();

    // Guest-60 soft-lock: collide re-enters activate every frame while overlapped.
    // Native WideSpring has no activate cooldown (unlike CObjSpring +0x188).
    if (GateEnabled() && IsGuestEa(self) && WideActivateBlocked(self, fr))
    {
        s_wideBlocked.fetch_add(1, std::memory_order_relaxed);
        static uint64_t lastBlockLog = 0;
        if (fr >= lastBlockLog + 30)
        {
            lastBlockLog = fr;
            Diag("wact-block", self, other, 0.f, flags, WideHoldFrames());
        }
        return;
    }

    if (GateEnabled() && IsGuestEa(self))
        WideActivateArmed(self, fr);

    Diag(GateEnabled() ? "wact+gate" : "wact", self, other, 0.f, flags);
    PPC_ORIGINAL(ObjWideSpringActivate);
}

PPC_OVERRIDE(ObjSpringPoleCollide)
{
    s_poleCol.fetch_add(1, std::memory_order_relaxed);
    const uint32_t self = ctx.r[3];
    const uint32_t other = ctx.r[4];
    uint32_t typ = IsGuestEa(other) ? Read32(other + 0x4) : 0;
    Diag("pcol", self, other, 0.f, 0, typ);
    PPC_ORIGINAL(ObjSpringPoleCollide);
}
