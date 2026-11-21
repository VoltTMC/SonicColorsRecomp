// Approach A1c — capture GX position-matrix loads + BodyGetWorldMtx refreshes.
//
// Player transform findings (fn_80053718 / text_80046CB8.c):
//   BodyGetWorldMtx(body): if dirty bit0 at body+0x2DD8, calls fn_800533E8 which
//   composes body+0xA4 and body+0xB0 into body+0x0C, then clears bit0.
//   Returns body+0x0C. PlayerGetWorldMtxPtr returns the same address WITHOUT refresh.
//   Capture after PPC_ORIGINAL(BodyGetWorldMtx) — never call refresh ourselves.
//
// GX: LoadXF / LoadIndexedXF → Render_OnXF. Pos mtx slots = XF addr/12 for addr<120.
#include "a1c_gx_capture.h"
#include "../game_addrs.h"
#include "../gx/gx_state.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../perf.h"
#include "../platform/platform.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {

bool EnvOn()
{
    static int c = -1;
    if (c < 0)
    {
        const char* v = std::getenv("SCR_A1C_GX_CAPTURE");
        c = (v && v[0] && v[0] != '0') ? 1 : 0;
        if (c)
            LOG_INFO("A1c: GX/body transform capture enabled (SCR_A1C_GX_CAPTURE); no extra unique XFB");
    }
    return c != 0;
}

bool DumpOn()
{
    static int c = -1;
    if (c < 0)
    {
        const char* v = std::getenv("SCR_A1C_GX_DUMP");
        c = (v && v[0] && v[0] != '0') ? 1 : 0;
    }
    return c != 0;
}

bool GuestPtrOk(uint32_t ea)
{
    if (!ea) return false;
    const uint32_t hi = ea >> 28;
    return hi == 0x8 || hi == 0x9;
}

float ReadF32(uint32_t addr)
{
    uint32_t u = Read32(addr);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

uint32_t HashMtx(const float m[12])
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < 12; i++)
    {
        uint32_t u;
        std::memcpy(&u, &m[i], 4);
        h ^= u;
        h *= 16777619u;
    }
    return h ? h : 1u;
}

bool FiniteMtx(const A1bMtx3x4& m)
{
    for (int i = 0; i < 12; i++)
        if (!std::isfinite(m.m[i])) return false;
    return true;
}

void ReadXfMtx(uint32_t addr, A1bMtx3x4& out)
{
    for (int i = 0; i < 12; i++)
    {
        union { uint32_t u; float f; } c;
        c.u = g_gx.xfmem[(addr + (uint32_t)i) & 0xFFF];
        out.m[i] = c.f;
    }
}

std::mutex s_mu;
A1cFrameSnap s_building{}, s_curr{}, s_prev{};
bool s_haveCurr = false, s_havePrev = false;
std::atomic<uint64_t> s_frames{ 0 };
std::atomic<uint32_t> s_seqInFrame{ 0 };

// Player body EA mirrored from A1b cache when available.
std::atomic<uint32_t> s_playerBody{ 0 };

void ResetBuilding(A1cFrameSnap& f)
{
    f = A1cFrameSnap{};
}

void RecordPosMtx(uint32_t addr, bool indexed)
{
    if (addr >= 120 || (addr % 12) != 0) return;
    const uint8_t slot = (uint8_t)(addr / 12);
    if (slot >= kA1cPosMtxSlots) return;

    A1bMtx3x4 mtx{};
    ReadXfMtx(addr, mtx);
    if (!FiniteMtx(mtx)) return;

    const uint32_t hash = HashMtx(mtx.m);
    const uint16_t seq = (uint16_t)s_seqInFrame.fetch_add(1, std::memory_order_relaxed);

    std::lock_guard lk(s_mu);
    s_building.loadCount++;
    s_building.slotLast[slot] = mtx;
    s_building.slotHash[slot] = hash;
    s_building.slotValid[slot] = true;

    if (s_building.loadStored < kA1cMaxLoadsPerFrame)
    {
        A1cMtxLoad& L = s_building.loads[s_building.loadStored++];
        L.seqInFrame = seq;
        L.xfAddr = (uint16_t)addr;
        L.slot = slot;
        L.flags = (GX_InDisplayList() ? 1u : 0u) | (indexed ? 2u : 0u);
        L.contentHash = hash;
        L.mtx = mtx;
    }
    else
    {
        s_building.loadsDropped++;
    }
}

int FindBody(A1cFrameSnap& f, uint32_t bodyEa)
{
    for (uint32_t i = 0; i < f.bodyCount; i++)
        if (f.bodies[i].bodyEa == bodyEa) return (int)i;
    return -1;
}

}  // namespace

bool A1c_GxCaptureEnabled() { return EnvOn(); }

void A1c_OnXfLoad(uint32_t addr, uint32_t count, bool indexed)
{
    if (!EnvOn() || count < 12) return;
    // Full aligned pos-mtx writes only (partial XF updates ignored for correspondence).
    if (addr >= 120 || (addr % 12) != 0) return;
    for (uint32_t off = 0; off + 12 <= count && addr + off < 120; off += 12)
        RecordPosMtx(addr + off, indexed);
}

void A1c_OnBodyWorldMtx(uint32_t bodyEa, const float* mtx12, uint32_t dirtyFlags)
{
    if (!EnvOn() || !mtx12 || !GuestPtrOk(bodyEa)) return;
    A1bMtx3x4 mtx{};
    std::memcpy(mtx.m, mtx12, sizeof(mtx.m));
    if (!FiniteMtx(mtx)) return;

    const uint32_t hash = HashMtx(mtx.m);
    const uint32_t playerBody = s_playerBody.load(std::memory_order_relaxed);
    const bool isPlayer = playerBody && bodyEa == playerBody;

    float pos[3] = {};
    if (GuestPtrOk(bodyEa))
    {
        pos[0] = ReadF32(bodyEa + 0x168);
        pos[1] = ReadF32(bodyEa + 0x168 + 4);
        pos[2] = ReadF32(bodyEa + 0x168 + 8);
    }

    std::lock_guard lk(s_mu);
    if (isPlayer) s_building.playerBodyRefreshes++;

    int idx = FindBody(s_building, bodyEa);
    if (idx < 0)
    {
        if (s_building.bodyCount >= kA1cMaxBodies)
        {
            // Prefer keeping player; drop a non-player if needed.
            if (!isPlayer) return;
            for (uint32_t i = 0; i < s_building.bodyCount; i++)
            {
                if (!s_building.bodies[i].isPlayer)
                {
                    idx = (int)i;
                    break;
                }
            }
            if (idx < 0) return;
        }
        else
        {
            idx = (int)s_building.bodyCount++;
        }
        s_building.bodies[idx] = A1cBodySample{};
        s_building.bodies[idx].bodyEa = bodyEa;
    }

    A1cBodySample& b = s_building.bodies[idx];
    b.bodyEa = bodyEa;
    b.dirtyFlags = dirtyFlags;
    b.contentHash = hash;
    b.refreshCount++;
    b.world = mtx;
    b.pos168[0] = pos[0];
    b.pos168[1] = pos[1];
    b.pos168[2] = pos[2];
    b.mtxValid = true;
    b.isPlayer = isPlayer || b.isPlayer;
}

void A1c_OnSimFrameEnd()
{
    if (!EnvOn()) return;
    // Sync player body pointer from A1b if available.
    A1bPoseSnap ply{};
    if (A1b_GetCurrent(ply) && ply.playerValid)
        s_playerBody.store(ply.playerBodyEa, std::memory_order_relaxed);

    std::lock_guard lk(s_mu);
    s_building.simFrameId = Perf_GuestFrameIndex();
}

void A1c_OnXfbCopy()
{
    if (!EnvOn()) return;

    std::lock_guard lk(s_mu);
    s_building.xfbIndex = Perf_XfbFrameIndex();

    uint32_t touched = 0;
    for (int i = 0; i < kA1cPosMtxSlots; i++)
        if (s_building.slotValid[i]) touched |= (1u << i);
    s_building.uniqueSlotsTouched = touched;

    uint32_t change = 0;
    if (s_haveCurr)
    {
        for (int i = 0; i < kA1cPosMtxSlots; i++)
        {
            if (!s_building.slotValid[i]) continue;
            if (!s_curr.slotValid[i] || s_curr.slotHash[i] != s_building.slotHash[i])
                change |= (1u << i);
        }
    }
    s_building.slotChangeMask = change;

    if (s_haveCurr)
    {
        s_prev = s_curr;
        s_havePrev = true;
    }
    s_curr = s_building;
    s_haveCurr = true;
    s_frames.fetch_add(1, std::memory_order_relaxed);

    if (DumpOn())
    {
        static uint64_t lastMs = 0;
        const uint64_t now = Platform_TickMs();
        if (now - lastMs >= 2500)
        {
            lastMs = now;
            const A1cBodySample* player = nullptr;
            for (uint32_t i = 0; i < s_curr.bodyCount; i++)
                if (s_curr.bodies[i].isPlayer && s_curr.bodies[i].mtxValid)
                {
                    player = &s_curr.bodies[i];
                    break;
                }

            LOG_INFO("A1c dump: xfb={} sim={} loads={}/{} drop={} slots={:03x} chg={:03x} "
                     "bodies={} plyRef={} pair={} "
                     "slot0T=({:.1f},{:.1f},{:.1f}){}",
                     s_curr.xfbIndex, s_curr.simFrameId,
                     s_curr.loadStored, s_curr.loadCount, s_curr.loadsDropped,
                     s_curr.uniqueSlotsTouched, s_curr.slotChangeMask,
                     s_curr.bodyCount, s_curr.playerBodyRefreshes,
                     s_havePrev ? "yes" : "no",
                     s_curr.slotValid[0] ? s_curr.slotLast[0].m[3] : 0.f,
                     s_curr.slotValid[0] ? s_curr.slotLast[0].m[7] : 0.f,
                     s_curr.slotValid[0] ? s_curr.slotLast[0].m[11] : 0.f,
                     "");

            if (player)
            {
                // body+0x0C often has T≈0 (local/orientation); world T is at +0x168 / GX.
                const float* m = player->world.m;
                LOG_INFO("A1c player body={:08x} dirty={:08x} hash={:08x} "
                         "mtxT=({:.2f},{:.2f},{:.2f}) R0=({:.2f},{:.2f},{:.2f}) "
                         "pos168=({:.2f},{:.2f},{:.2f}) refreshes={}",
                         player->bodyEa, player->dirtyFlags, player->contentHash,
                         m[3], m[7], m[11], m[0], m[1], m[2],
                         player->pos168[0], player->pos168[1], player->pos168[2],
                         player->refreshCount);
            }

            // Body EA correspondence vs previous frame (stable guest identity).
            if (s_havePrev)
            {
                int matched = 0, moved = 0;
                for (uint32_t i = 0; i < s_curr.bodyCount; i++)
                {
                    const A1cBodySample& cur = s_curr.bodies[i];
                    if (!cur.mtxValid) continue;
                    for (uint32_t j = 0; j < s_prev.bodyCount; j++)
                    {
                        const A1cBodySample& prev = s_prev.bodies[j];
                        if (prev.bodyEa != cur.bodyEa || !prev.mtxValid) continue;
                        matched++;
                        if (prev.contentHash != cur.contentHash) moved++;
                        break;
                    }
                }
                LOG_INFO("A1c bodyCorr: matched={} moved={} currBodies={} prevBodies={}",
                         matched, moved, s_curr.bodyCount, s_prev.bodyCount);
            }

            // Sample a few non-player bodies for scene coverage.
            int shown = 0;
            for (uint32_t i = 0; i < s_curr.bodyCount && shown < 3; i++)
            {
                const A1cBodySample& b = s_curr.bodies[i];
                if (b.isPlayer || !b.mtxValid) continue;
                LOG_INFO("A1c actor body={:08x} hash={:08x} mtxT=({:.2f},{:.2f},{:.2f}) "
                         "pos168=({:.2f},{:.2f},{:.2f}) n={}",
                         b.bodyEa, b.contentHash,
                         b.world.m[3], b.world.m[7], b.world.m[11],
                         b.pos168[0], b.pos168[1], b.pos168[2], b.refreshCount);
                shown++;
            }

            // Slot hash deltas vs previous when pair exists.
            if (s_havePrev)
            {
                for (int s = 0; s < kA1cPosMtxSlots; s++)
                {
                    if (!(s_curr.slotChangeMask & (1u << s))) continue;
                    LOG_INFO("A1c slot{} hash {:08x}->{:08x} T ({:.1f},{:.1f},{:.1f})->"
                             "({:.1f},{:.1f},{:.1f})",
                             s,
                             s_prev.slotValid[s] ? s_prev.slotHash[s] : 0u,
                             s_curr.slotHash[s],
                             s_prev.slotValid[s] ? s_prev.slotLast[s].m[3] : 0.f,
                             s_prev.slotValid[s] ? s_prev.slotLast[s].m[7] : 0.f,
                             s_prev.slotValid[s] ? s_prev.slotLast[s].m[11] : 0.f,
                             s_curr.slotLast[s].m[3], s_curr.slotLast[s].m[7],
                             s_curr.slotLast[s].m[11]);
                }
            }
        }
    }

    ResetBuilding(s_building);
    s_seqInFrame.store(0, std::memory_order_relaxed);
}

uint64_t A1c_FrameCount() { return s_frames.load(std::memory_order_relaxed); }

bool A1c_HasPair()
{
    std::lock_guard lk(s_mu);
    return s_havePrev && s_haveCurr;
}

bool A1c_GetCurrent(A1cFrameSnap& out)
{
    std::lock_guard lk(s_mu);
    if (!s_haveCurr) return false;
    out = s_curr;
    return true;
}

bool A1c_GetPrevious(A1cFrameSnap& out)
{
    std::lock_guard lk(s_mu);
    if (!s_havePrev) return false;
    out = s_prev;
    return true;
}

void A1c_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || bufSize == 0) return;
    buf[0] = 0;
    if (!EnvOn()) return;
    A1cFrameSnap cur{};
    if (!A1c_GetCurrent(cur))
    {
        std::snprintf(buf, bufSize, "A1c: waiting for XFB");
        return;
    }
    std::snprintf(buf, bufSize,
                  "A1c xfb %llu loads %u/%u slots %03x chg %03x bodies %u plyR %u",
                  (unsigned long long)cur.xfbIndex,
                  cur.loadStored, cur.loadCount,
                  cur.uniqueSlotsTouched, cur.slotChangeMask,
                  cur.bodyCount, cur.playerBodyRefreshes);
}

// --- Guest: capture after natural BodyGetWorldMtx refresh -----------------------------

PPC_OVERRIDE(BodyGetWorldMtx)
{
    const uint32_t body = ctx.r[3];
    uint32_t dirty = 0;
    if (EnvOn() && GuestPtrOk(body))
        dirty = Read32(body + 0x2DD8);

    PPC_ORIGINAL(BodyGetWorldMtx);

    if (!EnvOn() || !GuestPtrOk(body)) return;
    // After original: matrix at body+0x0C is authoritative (refreshed if dirty).
    float m[12];
    for (int i = 0; i < 12; i++)
        m[i] = ReadF32(body + 0x0C + (uint32_t)(i * 4));
    A1c_OnBodyWorldMtx(body, m, dirty);
}
