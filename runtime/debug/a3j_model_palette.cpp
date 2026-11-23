// Approach A3j — player→ScnMdl ownership + dual-palette provenance.
#include "a3j_model_palette.h"
#include "a1b_pose_capture.h"
#include "../game_addrs.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../perf.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_own{ -1 };
std::atomic<int> s_palA{ -1 };
std::atomic<int> s_palB{ -1 };
std::atomic<int> s_root{ -1 };

std::mutex s_mu;
A3jFrameSnap s_ring[kA3jRingSize]{};
uint32_t s_ringCount = 0;
uint32_t s_ringWrite = 0;
uint64_t s_seq = 0;
uint32_t s_dumps = 0;
uint64_t s_lastDrainXfb = ~0ull;

std::mutex s_regMu;
struct RegEntry
{
    uint32_t scn = 0;
    uint32_t res = 0;
    uint32_t world = 0;
    uint32_t wattr = 0;
    uint32_t viewPos = 0;
    uint32_t viewNrm = 0;
    uint32_t viewEnv = 0;
    uint16_t nView = 0;
    uint8_t b100 = 0;
    uint8_t b101 = 0;
    uint32_t slotBytes = 0;
    uint32_t createLr = 0;
};
RegEntry s_reg[kA3jRegistrySize]{};
uint32_t s_regN = 0;
uint32_t s_regOvf = 0;

std::mutex s_viewMu;
A3jViewWrite s_buildViews[kA3jMaxViewWrites]{};
uint32_t s_buildViewN = 0;
uint32_t s_buildViewOvf = 0;
uint64_t s_buildViewXfb = 0;

std::atomic<uint64_t> s_viewHits{ 0 };
std::atomic<uint64_t> s_ctorHits{ 0 };
uint32_t s_prevPrimary = 0;
uint8_t s_prevStable = 0;

bool EnvFlag(const char* name)
{
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

bool OwnOn()
{
    int v = s_own.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3J_PLAYER_MODEL_OWNER") ? 1 : 0;
        s_own.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3j: player model ownership enabled (SCR_A3J_PLAYER_MODEL_OWNER)");
    }
    return v != 0;
}

bool PalAOn()
{
    int v = s_palA.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3J_PALETTE_A_TRACE") ? 1 : 0;
        s_palA.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3j: palette A (ViewPos) trace enabled (SCR_A3J_PALETTE_A_TRACE)");
    }
    return v != 0;
}

bool PalBOn()
{
    int v = s_palB.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3J_PALETTE_B_TRACE") ? 1 : 0;
        s_palB.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3j: palette B (ViewNrm/+0xF8 twin) enabled (SCR_A3J_PALETTE_B_TRACE)");
    }
    return v != 0;
}

bool RootOn()
{
    int v = s_root.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3J_ROOT_COMPOSITION") ? 1 : 0;
        s_root.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3j: root composition samples enabled (SCR_A3J_ROOT_COMPOSITION)");
    }
    return v != 0;
}

bool AnyOn() { return OwnOn() || PalAOn() || PalBOn() || RootOn(); }

bool GuestOk(uint32_t ea)
{
    if (!ea) return false;
    const uint32_t hi = ea >> 28;
    if (hi != 0x8 && hi != 0x9) return false;
    const uint32_t phys = ea & 0x3FFFFFFFu;
    if (hi == 0x8 && phys < MEM1_SIZE) return true;
    if (hi == 0x9 && phys >= 0x10000000u && phys + 4u <= 0x10000000u + MEM2_SIZE)
        return true;
    return false;
}

uint32_t GuestCanon(uint32_t ea)
{
    if (!ea) return 0;
    if ((ea >> 28) == 0x8 || (ea >> 28) == 0x9) return ea;
    if (ea < MEM1_SIZE) return ea | 0x80000000u;
    if (ea >= 0x10000000u && ea < 0x10000000u + MEM2_SIZE) return ea | 0x90000000u;
    return ea;
}

uint32_t Align32(uint32_t n) { return (n + 31u) & ~31u; }

uint32_t SlotBytes(uint16_t nView)
{
    if (!nView) nView = 1;
    return Align32((uint32_t)nView * 0x30u);
}

bool LooksLikeScn(uint32_t scn, uint16_t* outN)
{
    scn = GuestCanon(scn);
    if (!scn || !GuestOk(scn + kA3jScn_NumView)) return false;
    const uint16_t n = Read16(scn + kA3jScn_NumView);
    if (n == 0 || n > 512) return false;
    const uint32_t res = GuestCanon(Read32(scn + kA3jScn_ResMdl));
    const uint32_t world = GuestCanon(Read32(scn + kA3jScn_World));
    const uint32_t view = GuestCanon(Read32(scn + kA3jScn_ViewPos));
    if (!GuestOk(res) || !GuestOk(world)) return false;
    if (view && !GuestOk(view)) return false;
    if (outN) *outN = n;
    return true;
}

void FillFromGuest(uint32_t scn, RegEntry& e)
{
    e.scn = scn;
    e.res = GuestCanon(Read32(scn + kA3jScn_ResMdl));
    e.world = GuestCanon(Read32(scn + kA3jScn_World));
    e.wattr = GuestCanon(Read32(scn + kA3jScn_WorldAttr));
    e.viewPos = GuestCanon(Read32(scn + kA3jScn_ViewPos));
    e.viewNrm = GuestCanon(Read32(scn + kA3jScn_ViewNrm));
    e.viewEnv = GuestCanon(Read32(scn + kA3jScn_ViewEnv));
    e.nView = Read16(scn + kA3jScn_NumView);
    e.b100 = Read8(scn + kA3jScn_Byte100);
    e.b101 = Read8(scn + kA3jScn_Byte101);
    e.slotBytes = SlotBytes(e.nView);
}

void RegistryUpsert(uint32_t scnRaw, uint32_t createLr)
{
    const uint32_t scn = GuestCanon(scnRaw);
    uint16_t n = 0;
    if (!scn || !LooksLikeScn(scn, &n)) return;
    std::lock_guard<std::mutex> lk(s_regMu);
    for (uint32_t i = 0; i < s_regN; i++)
    {
        if (s_reg[i].scn == scn)
        {
            FillFromGuest(scn, s_reg[i]);
            if (createLr) s_reg[i].createLr = createLr;
            return;
        }
    }
    if (s_regN >= (uint32_t)kA3jRegistrySize)
    {
        s_regOvf++;
        return;
    }
    RegEntry& e = s_reg[s_regN++];
    e = {};
    FillFromGuest(scn, e);
    e.createLr = createLr;
}

// Match palette base against array with multi-buffer span (typically 2 slots).
bool MatchArray(uint32_t pal, uint32_t base, uint32_t slotBytes, uint8_t maxSlots,
                uint8_t* outSlot)
{
    pal = GuestCanon(pal);
    base = GuestCanon(base);
    if (!pal || !base || !slotBytes) return false;
    if (pal == base)
    {
        if (outSlot) *outSlot = 0;
        return true;
    }
    if (pal < base) return false;
    const uint32_t span = slotBytes * (uint32_t)(maxSlots ? maxSlots : 2);
    if (pal >= base + span) return false;
    const uint32_t off = pal - base;
    if (off % slotBytes) return false;
    if (outSlot) *outSlot = (uint8_t)(off / slotBytes);
    return true;
}

uint8_t MaxSlots(uint8_t b100, uint8_t b101)
{
    // Observed: +0x101 often holds curView index (0/1) or init count; +0x100 companion.
    // Always allow at least 2 slots — double-buffer is the TR1 pattern for large models.
    uint8_t m = 2;
    if (b100 > m && b100 <= 16) m = b100;
    if (b101 > m && b101 <= 16) m = b101;
    if (m < 2) m = 2;
    return m;
}

struct ArrHit
{
    uint32_t scn = 0;
    uint32_t arrayBase = 0;
    uint8_t kind = A3jArr_Unknown;
    uint8_t slot = 0;
};

bool ResolvePalette(uint32_t pal, ArrHit& hit)
{
    pal = GuestCanon(pal);
    if (!pal) return false;
    std::lock_guard<std::mutex> lk(s_regMu);
    for (uint32_t i = 0; i < s_regN; i++)
    {
        const RegEntry& e = s_reg[i];
        const uint8_t slots = MaxSlots(e.b100, e.b101);
        uint8_t slot = 0;
        if (MatchArray(pal, e.viewPos, e.slotBytes, slots, &slot))
        {
            hit = { e.scn, e.viewPos, slot ? A3jArr_ViewSlot : A3jArr_ViewPos, slot };
            return true;
        }
        if (e.viewNrm && MatchArray(pal, e.viewNrm, e.slotBytes, slots, &slot))
        {
            hit = { e.scn, e.viewNrm, slot ? A3jArr_ViewSlot : A3jArr_ViewNrm, slot };
            // Tag ViewSlot-from-Nrm distinctly via kind still ViewNrm if slot0; slot>0 keep ViewSlot
            if (slot == 0) hit.kind = A3jArr_ViewNrm;
            else
            {
                hit.kind = A3jArr_ViewNrm;  // still Nrm family
                hit.slot = slot;
            }
            return true;
        }
        if (e.viewEnv && MatchArray(pal, e.viewEnv, e.slotBytes, slots, &slot))
        {
            hit = { e.scn, e.viewEnv, A3jArr_ViewEnv, slot };
            return true;
        }
        if (MatchArray(pal, e.world, e.slotBytes, slots, &slot))
        {
            hit = { e.scn, e.world, A3jArr_World, slot };
            return true;
        }
    }
    return false;
}

bool FindPtrSlot(uint32_t holder, uint32_t target, uint32_t bytes, uint32_t* outOff)
{
    holder = GuestCanon(holder);
    target = GuestCanon(target);
    if (!holder || !target || !GuestOk(holder)) return false;
    for (uint32_t off = 0; off + 4 <= bytes; off += 4)
    {
        if (!GuestOk(holder + off)) break;
        if (GuestCanon(Read32(holder + off)) == target)
        {
            if (outOff) *outOff = off;
            return true;
        }
    }
    return false;
}

// Bounded BFS: body/shell → components → CModelImpl(+0x1C) → ScnMdl.
bool ProveOwnership(uint32_t shell, uint32_t body, uint32_t scn, A3jModelRec& m)
{
    scn = GuestCanon(scn);
    shell = GuestCanon(shell);
    body = GuestCanon(body);
    if (!scn || (!shell && !body)) return false;

    struct Node
    {
        uint32_t ea;
        uint8_t depth;
        uint8_t fromKind;
        uint16_t fromOff;
        uint32_t parentEa;
    };
    Node q[96]{};
    uint32_t qn = 0;
    uint32_t seen[96]{};
    uint32_t sn = 0;
    auto push = [&](uint32_t ea, uint8_t depth, uint8_t kind, uint16_t off, uint32_t parent) {
        ea = GuestCanon(ea);
        if (!ea || !GuestOk(ea) || depth > 4 || qn >= 96) return;
        for (uint32_t i = 0; i < sn; i++)
            if (seen[i] == ea) return;
        if (sn < 96) seen[sn++] = ea;
        q[qn++] = { ea, depth, kind, off, parent };
    };

    if (body) push(body, 0, 1, 0, 0);
    if (shell)
    {
        push(shell, 0, 3, 0, 0);
        if (GuestOk(shell + 0x28))
        {
            const uint32_t inner = GuestCanon(Read32(shell + 0x28));
            if (inner) push(inner, 1, 2, 0x28, shell);
        }
    }

    for (uint32_t qi = 0; qi < qn; qi++)
    {
        const Node& n = q[qi];
        // Direct ScnMdl* slot.
        uint32_t off = 0;
        if (FindPtrSlot(n.ea, scn, n.depth == 0 ? 0x300u : 0x180u, &off))
        {
            m.ownClass = A3jOwn_PlayerProven;
            m.pathHops = 0;
            auto addHop = [&](uint32_t ea, uint16_t o, uint8_t k) {
                if (m.pathHops >= kA3jMaxPathHops) return;
                m.path[m.pathHops++] = { ea, o, k, 0 };
            };
            // Reconstruct coarse path from BFS parent chain.
            if (n.parentEa)
                addHop(n.parentEa, n.fromOff, n.fromKind);
            addHop(n.ea, (uint16_t)off, n.fromKind ? n.fromKind : 4);
            addHop(scn, 0, 0);
            return true;
        }
        // CModelImpl: obj+0x1C == ScnMdl.
        if (GuestOk(n.ea + kA3jModelImpl_ScnMdl))
        {
            const uint32_t held = GuestCanon(Read32(n.ea + kA3jModelImpl_ScnMdl));
            if (held == scn)
            {
                m.ownClass = A3jOwn_ModelImplProven;
                m.modelImplEa = n.ea;
                m.pathHops = 0;
                if (n.parentEa && m.pathHops < kA3jMaxPathHops)
                    m.path[m.pathHops++] = { n.parentEa, n.fromOff, n.fromKind, 0 };
                if (m.pathHops < kA3jMaxPathHops)
                    m.path[m.pathHops++] = { n.ea, (uint16_t)kA3jModelImpl_ScnMdl, 5, 0 };
                if (m.pathHops < kA3jMaxPathHops)
                    m.path[m.pathHops++] = { scn, 0, 0, 0 };
                return true;
            }
            // Follow ModelImpl → ScnMdl as next hop candidate object.
            if (held && LooksLikeScn(held, nullptr) && n.depth < 4)
                push(held, (uint8_t)(n.depth + 1), 5, (uint16_t)kA3jModelImpl_ScnMdl, n.ea);
        }
        // Expand pointer children (bounded).
        if (n.depth >= 3) continue;
        const uint32_t scan = n.depth == 0 ? 0x280u : 0x100u;
        for (uint32_t o = 0; o + 4 <= scan; o += 4)
        {
            if (!GuestOk(n.ea + o)) break;
            const uint32_t p = GuestCanon(Read32(n.ea + o));
            if (!p || p == n.ea || p == scn) continue;
            if ((p >> 28) != 0x8 && (p >> 28) != 0x9) continue;
            push(p, (uint8_t)(n.depth + 1), 4, (uint16_t)o, n.ea);
        }
    }
    return false;
}

void PushViewWrite(A3jViewWrite& w)
{
    std::lock_guard<std::mutex> lk(s_viewMu);
    const uint64_t xfb = Perf_XfbFrameIndex();
    if (xfb != s_buildViewXfb)
    {
        s_buildViewXfb = xfb;
        s_buildViewN = 0;
        s_buildViewOvf = 0;
    }
    if (s_buildViewN >= (uint32_t)kA3jMaxViewWrites)
    {
        s_buildViewOvf++;
        return;
    }
    s_buildViews[s_buildViewN++] = w;
}

void OnViewCalc(uint8_t hook, uint32_t dest, uint32_t a4, uint32_t world, uint32_t attr,
                uint32_t count, uint32_t lr)
{
    (void)a4;
    if (!AnyOn()) return;
    // Guest-thread: record args only. Resolve ScnMdl/array kind at XFB drain.
    const uint64_t hits = s_viewHits.fetch_add(1, std::memory_order_relaxed) + 1;
    if (hits <= 6)
        LOG_INFO("A3j View{} dest={:08x} world={:08x} attr={:08x} n={} lr={:08x}",
                 (unsigned)hook, dest, world, attr, count, lr);

    A3jViewWrite w{};
    w.destEa = GuestCanon(dest);
    w.worldEa = GuestCanon(world);
    w.attrEa = GuestCanon(attr);
    w.count = (uint16_t)(count > 0xFFFF ? 0xFFFF : count);
    w.lr = lr;
    w.hook = hook;
    PushViewWrite(w);
}

float ReadF32(uint32_t ea)
{
    const uint32_t u = Read32(ea);
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

void ReadMtxT(uint32_t ea, float outT[3], uint8_t* ok)
{
    *ok = 0;
    ea = GuestCanon(ea);
    if (!ea || !GuestOk(ea + 0x2C)) return;
    // nw4r Mtx 3x4: T at column 3 → +0x0C, +0x1C, +0x2C.
    outT[0] = ReadF32(ea + 0x0C);
    outT[1] = ReadF32(ea + 0x1C);
    outT[2] = ReadF32(ea + 0x2C);
    *ok = 1;
}

float Dist3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

A3jModelRec* FindOrAdd(A3jFrameSnap& snap, uint32_t scn)
{
    for (uint16_t i = 0; i < snap.modelCount; i++)
        if (snap.models[i].scnMdlEa == scn) return &snap.models[i];
    if (snap.modelCount >= kA3jMaxModels)
    {
        snap.modelOverflow++;
        return nullptr;
    }
    A3jModelRec& m = snap.models[snap.modelCount++];
    m = {};
    m.scnMdlEa = scn;
    return &m;
}

void BuildFromRegistry(A3jFrameSnap& snap, uint32_t shell, uint32_t body)
{
    RegEntry local[kA3jRegistrySize];
    uint32_t n = 0;
    {
        std::lock_guard<std::mutex> lk(s_regMu);
        n = s_regN;
        if (n > (uint32_t)kA3jRegistrySize) n = kA3jRegistrySize;
        std::memcpy(local, s_reg, n * sizeof(RegEntry));
    }
    snap.registryCount = (uint16_t)n;
    for (uint32_t i = 0; i < n; i++)
    {
        const RegEntry& e = local[i];
        // Refresh live fields.
        if (!LooksLikeScn(e.scn, nullptr)) continue;
        RegEntry live = e;
        FillFromGuest(e.scn, live);
        A3jModelRec* m = FindOrAdd(snap, live.scn);
        if (!m) continue;
        m->resMdlEa = live.res;
        m->worldEa = live.world;
        m->worldAttrEa = live.wattr;
        m->viewPosEa = live.viewPos;
        m->viewNrmEa = live.viewNrm;
        m->viewEnvEa = live.viewEnv;
        m->numViewMtx = live.nView;
        m->byte100 = live.b100;
        m->byte101 = live.b101;
        m->slotBytes = live.slotBytes;
        if (OwnOn() && (shell || body) && live.nView >= 32)
        {
            if (ProveOwnership(shell, body, live.scn, *m))
                snap.provenCount++;
            else if (shell && body && live.nView >= 64)
                m->ownClass = A3jOwn_Associated;  // large model under live player; ptr open
        }
    }
}

void JoinPals(A3jFrameSnap& snap, const A3hFrameSnap* a3h)
{
    if (!a3h || (!PalAOn() && !PalBOn())) return;
    for (uint16_t pi = 0; pi < a3h->paletteCount && snap.palCount < kA3jMaxPals; pi++)
    {
        const A3hPalette& ap = a3h->pals[pi];
        if (!ap.baseEa || ap.loadHits == 0) continue;
        A3jPalLink link{};
        link.paletteBase = ap.baseEa;
        link.loadHits = ap.loadHits;
        ArrHit hit{};
        if (ResolvePalette(ap.baseEa, hit))
        {
            link.scnMdlEa = hit.scn;
            link.arrayBase = hit.arrayBase;
            link.arrKind = hit.kind;
            link.slotIndex = hit.slot;
            // Group classification by array family + hit scale.
            if (hit.kind == A3jArr_ViewPos ||
                (hit.kind == A3jArr_ViewSlot && hit.arrayBase &&
                 [&]() {
                     for (uint16_t i = 0; i < snap.modelCount; i++)
                         if (snap.models[i].scnMdlEa == hit.scn &&
                             snap.models[i].viewPosEa == hit.arrayBase)
                             return true;
                     return false;
                 }()))
                link.group = 1;
            else if (hit.kind == A3jArr_ViewNrm || hit.kind == A3jArr_ViewEnv)
                link.group = 2;

            for (uint16_t i = 0; i < snap.modelCount; i++)
            {
                if (snap.models[i].scnMdlEa != hit.scn) continue;
                link.ownClass = snap.models[i].ownClass;
                if (hit.kind == A3jArr_ViewPos ||
                    (hit.kind == A3jArr_ViewSlot && snap.models[i].viewPosEa == hit.arrayBase))
                    link.group = 1;
                else if (hit.kind == A3jArr_ViewNrm ||
                         (hit.arrayBase && snap.models[i].viewNrmEa == hit.arrayBase))
                    link.group = 2;
                else if (hit.kind == A3jArr_ViewEnv ||
                         (hit.arrayBase && snap.models[i].viewEnvEa == hit.arrayBase))
                    link.group = 2;
                break;
            }
        }
        else
            snap.palUnresolved++;

        for (uint16_t ri = 0; ri < a3h->rowMapCount; ri++)
        {
            const A3hRowMap& r = a3h->rows[ri];
            if (r.paletteBase != ap.baseEa) continue;
            link.mtxID = r.mtxID;
            link.xfRow = r.xfRow;
            break;
        }
        if (link.group == 1) snap.groupACount++;
        else if (link.group == 2) snap.groupBCount++;
        snap.pals[snap.palCount++] = link;
    }
}

void PickPrimary(A3jFrameSnap& snap)
{
    int best = -1, bestScore = -1;
    for (uint16_t i = 0; i < snap.modelCount; i++)
    {
        const A3jModelRec& m = snap.models[i];
        int score = (int)m.numViewMtx * 10;
        if (m.ownClass == A3jOwn_PlayerProven || m.ownClass == A3jOwn_ModelImplProven)
            score += 100000;
        else if (m.ownClass == A3jOwn_PaletteMatch) score += 1000;
        else if (m.ownClass == A3jOwn_Associated) score += 100;
        // Prefer models that own high-hit pals.
        for (uint16_t p = 0; p < snap.palCount; p++)
            if (snap.pals[p].scnMdlEa == m.scnMdlEa)
                score += (int)snap.pals[p].loadHits;
        if (score > bestScore)
        {
            bestScore = score;
            best = (int)i;
        }
    }
    if (best < 0) return;
    const A3jModelRec& m = snap.models[best];
    snap.primaryScn = m.scnMdlEa;
    snap.primaryViewPos = m.viewPosEa;
    snap.primaryViewNrm = m.viewNrmEa;
    snap.primaryWorld = m.worldEa;
    snap.primaryNView = m.numViewMtx;
    snap.primaryOwn = m.ownClass;
    if (snap.primaryScn == s_prevPrimary)
        s_prevStable = (uint8_t)(s_prevStable < 255 ? s_prevStable + 1 : 255);
    else
    {
        s_prevPrimary = snap.primaryScn;
        s_prevStable = 1;
    }
    snap.models[best].stableFrames = s_prevStable;
}

void SampleRoot(A3jFrameSnap& snap)
{
    if (!RootOn() || !snap.primaryScn) return;
    A3jRootSample& r = snap.root;
    r.scnMdlEa = snap.primaryScn;
    // Prefer A1b's already-validated body world (same source as A1c).
    A1bPoseSnap ply{};
    if (A1b_GetCurrent(ply) && ply.playerValid)
    {
        r.bodyMtxEa = ply.playerBodyEa ? ply.playerBodyEa + 0x0C : 0;
        r.bodyT[0] = ply.playerWorld.m[3];
        r.bodyT[1] = ply.playerWorld.m[7];
        r.bodyT[2] = ply.playerWorld.m[11];
        r.bodyOk = 1;
    }
    else if (snap.bodyEa && GuestOk(snap.bodyEa + 0x0C + 0x2C))
    {
        r.bodyMtxEa = snap.bodyEa + 0x0C;
        ReadMtxT(r.bodyMtxEa, r.bodyT, &r.bodyOk);
    }
    if (snap.primaryWorld)
    {
        r.world0Ea = snap.primaryWorld;
        ReadMtxT(r.world0Ea, r.worldT, &r.worldOk);
    }
    // Prefer current view slot: ViewPos + byte101 * slotBytes when byte101 is 0/1.
    uint32_t viewBase = snap.primaryViewPos;
    for (uint16_t i = 0; i < snap.modelCount; i++)
    {
        if (snap.models[i].scnMdlEa != snap.primaryScn) continue;
        const uint32_t slot = snap.models[i].slotBytes;
        const uint8_t idx = snap.models[i].byte101;
        if (viewBase && slot && idx <= 1)
            viewBase = snap.primaryViewPos + (uint32_t)idx * slot;
        break;
    }
    if (viewBase)
    {
        r.view0Ea = viewBase;
        ReadMtxT(r.view0Ea, r.viewT, &r.viewOk);
    }
    if (r.bodyOk && r.worldOk && Dist3(r.bodyT, r.worldT) < 1e-4f)
        r.vwRelation = 1;
    else if (r.bodyOk && r.viewOk && Dist3(r.bodyT, r.viewT) < 1e-4f)
        r.vwRelation = 2;
    else if (r.bodyOk)
        r.vwRelation = 3;
}

void Score(A3jFrameSnap& snap)
{
    snap.ownershipStatus = A3jStatus_Fail;
    snap.paletteAStatus = A3jStatus_Fail;
    snap.paletteBStatus = A3jStatus_Fail;
    snap.rootStatus = A3jStatus_Unverified;

    if (snap.primaryOwn == A3jOwn_PlayerProven || snap.primaryOwn == A3jOwn_ModelImplProven)
        snap.ownershipStatus = A3jStatus_Pass;
    else if (snap.primaryScn)
        snap.ownershipStatus = A3jStatus_Partial;

    uint16_t aMapped = 0, bMapped = 0, aTot = 0, bTot = 0;
    for (uint16_t i = 0; i < snap.palCount; i++)
    {
        const A3jPalLink& p = snap.pals[i];
        if (p.group == 1 || (p.arrKind == A3jArr_ViewPos ||
                             (p.arrKind == A3jArr_ViewSlot && p.group != 2)))
        {
            // High-hit ViewPos family (~600+) counts as group A.
            if (p.loadHits >= 200 || p.group == 1)
            {
                aTot++;
                if (p.scnMdlEa) aMapped++;
            }
        }
        if (p.group == 2 || p.arrKind == A3jArr_ViewNrm || p.arrKind == A3jArr_ViewEnv)
        {
            if (p.loadHits >= 200 || p.group == 2)
            {
                bTot++;
                if (p.scnMdlEa) bMapped++;
            }
        }
        // Unresolved high-hit twin: still attribute via slot pairing with known A.
        if (!p.scnMdlEa && p.loadHits >= 200)
        {
            for (uint16_t j = 0; j < snap.palCount; j++)
            {
                const A3jPalLink& q = snap.pals[j];
                if (!q.scnMdlEa || q.loadHits < 200) continue;
                const uint32_t d =
                    p.paletteBase > q.paletteBase ? p.paletteBase - q.paletteBase
                                                  : q.paletteBase - p.paletteBase;
                // Same family if delta is exactly one slot of primary.
                uint32_t slot = 0;
                for (uint16_t mi = 0; mi < snap.modelCount; mi++)
                    if (snap.models[mi].scnMdlEa == q.scnMdlEa)
                    {
                        slot = snap.models[mi].slotBytes;
                        break;
                    }
                if (slot && d == slot)
                {
                    // Will be fixed by ResolvePalette when registry has arrays; mark partial.
                }
            }
        }
    }
    if (aTot == 0) snap.paletteAStatus = A3jStatus_Unverified;
    else if (aMapped == aTot) snap.paletteAStatus = A3jStatus_Pass;
    else if (aMapped > 0) snap.paletteAStatus = A3jStatus_Partial;

    if (bTot == 0) snap.paletteBStatus = A3jStatus_Unverified;
    else if (bMapped == bTot) snap.paletteBStatus = A3jStatus_Pass;
    else if (bMapped > 0) snap.paletteBStatus = A3jStatus_Partial;

    if (RootOn())
    {
        if (snap.root.bodyOk && snap.root.worldOk && snap.root.viewOk)
            snap.rootStatus =
                (snap.root.vwRelation == 1) ? A3jStatus_Partial : A3jStatus_Partial;
        else
            snap.rootStatus = A3jStatus_Partial;
    }
}

void Dump(const A3jFrameSnap& s)
{
    const bool inStage = s.shellEa != 0 && s.primaryNView >= 64;
    const bool interesting =
        inStage || s.provenCount > 0 ||
        (s.primaryNView >= 100) ||
        (s.groupACount + s.groupBCount > 0 && s.primaryNView >= 36);
    if (!interesting) return;
    // Prefer in-stage dumps; keep a small title budget.
    if (!inStage)
    {
        if (s_dumps >= 12) return;
    }
    else if (s_dumps >= 80)
        return;
    else if ((s_dumps >= 16) && (s.xfbIndex % 2) != 0)
        return;
    s_dumps++;
    LOG_INFO("A3j xfb={} shell={:08x} body={:08x} models={} reg={} proven={} "
             "pals={} unres={} grpA={} grpB={} views={} ovfV={}",
             s.xfbIndex, s.shellEa, s.bodyEa, s.modelCount, s.registryCount, s.provenCount,
             s.palCount, s.palUnresolved, s.groupACount, s.groupBCount, s.viewWriteCount,
             s.viewWriteOvf);
    uint32_t primVe = 0;
    for (uint16_t i = 0; i < s.modelCount; i++)
        if (s.models[i].scnMdlEa == s.primaryScn)
        {
            primVe = s.models[i].viewEnvEa;
            break;
        }
    LOG_INFO("A3j primary scn={:08x} W={:08x} Vp={:08x} Vn={:08x} Ve={:08x} n={} own={} "
             "stable={}",
             s.primaryScn, s.primaryWorld, s.primaryViewPos, s.primaryViewNrm, primVe,
             s.primaryNView, s.primaryOwn, s_prevStable);
    LOG_INFO("A3j status own={} palA={} palB={} root={} rootRel={}",
             s.ownershipStatus, s.paletteAStatus, s.paletteBStatus, s.rootStatus,
             s.root.vwRelation);
    const uint16_t nm = s.modelCount < 12 ? s.modelCount : 12;
    for (uint16_t i = 0; i < nm; i++)
    {
        const A3jModelRec& m = s.models[i];
        LOG_INFO("A3j mdl[{}] scn={:08x} W={:08x} Vp={:08x} Vn={:08x} Ve={:08x} n={} "
                 "slot={:x} b100={} b101={} own={} impl={:08x} hops={}",
                 i, m.scnMdlEa, m.worldEa, m.viewPosEa, m.viewNrmEa, m.viewEnvEa,
                 m.numViewMtx, m.slotBytes, m.byte100, m.byte101, m.ownClass, m.modelImplEa,
                 m.pathHops);
        for (uint8_t h = 0; h < m.pathHops && h < 4; h++)
            LOG_INFO("A3j   path[{}] ea={:08x} off={:x} kind={}", h, m.path[h].ea,
                     m.path[h].offset, m.path[h].kind);
    }
    const uint16_t np = s.palCount < 12 ? s.palCount : 12;
    for (uint16_t i = 0; i < np; i++)
    {
        const A3jPalLink& p = s.pals[i];
        if (p.loadHits < 8 && i >= 4) continue;
        LOG_INFO("A3j pal[{}] base={:08x} hits={} scn={:08x} arr={:08x} kind={} slot={} "
                 "grp={} own={} row={} mtxID={}",
                 i, p.paletteBase, p.loadHits, p.scnMdlEa, p.arrayBase, p.arrKind, p.slotIndex,
                 p.group, p.ownClass, p.xfRow, p.mtxID);
    }
    const uint16_t nv = s.viewWriteCount < 6 ? s.viewWriteCount : 6;
    for (uint16_t i = 0; i < nv; i++)
    {
        const A3jViewWrite& w = s.views[i];
        LOG_INFO("A3j view[{}] hook={} dest={:08x} world={:08x} scn={:08x} kind={} n={} "
                 "lr={:08x}",
                 i, w.hook, w.destEa, w.worldEa, w.scnMdlEa, w.arrKind, w.count, w.lr);
    }
    if (RootOn() && s.root.scnMdlEa)
        LOG_INFO("A3j root bodyT=({:.2f},{:.2f},{:.2f}) worldT=({:.2f},{:.2f},{:.2f}) "
                 "viewT=({:.2f},{:.2f},{:.2f}) rel={}",
                 s.root.bodyT[0], s.root.bodyT[1], s.root.bodyT[2], s.root.worldT[0],
                 s.root.worldT[1], s.root.worldT[2], s.root.viewT[0], s.root.viewT[1],
                 s.root.viewT[2], s.root.vwRelation);
}

}  // namespace

bool A3j_AnyEnabled() { return AnyOn(); }

void A3j_OnCalcWorld(uint32_t scnMdlEa, uint32_t /*param*/)
{
    if (!AnyOn()) return;
    RegistryUpsert(scnMdlEa, 0);
}

void A3j_OnFrame(const A3aFrameSnap& cur, const A3gPoseSnap* /*pose*/, const A3hFrameSnap* a3h,
                 const A3iFrameSnap* /*a3i*/)
{
    if (!AnyOn()) return;
    if (cur.xfbIndex == s_lastDrainXfb) return;
    s_lastDrainXfb = cur.xfbIndex;

    A3jFrameSnap snap{};
    snap.simFrameId = cur.simFrameId;
    snap.xfbIndex = cur.xfbIndex;
    snap.seq = ++s_seq;

    A1bPoseSnap ply{};
    if (A1b_GetCurrent(ply) && ply.playerValid)
    {
        snap.shellEa = ply.playerShellEa;
        snap.bodyEa = ply.playerBodyEa;
        snap.instanceKey = ply.playerShellEa ^ ply.playerBodyEa;
    }

    BuildFromRegistry(snap, snap.shellEa, snap.bodyEa);

    {
        std::lock_guard<std::mutex> lk(s_viewMu);
        snap.viewWriteCount =
            (uint16_t)(s_buildViewN > (uint32_t)kA3jMaxViewWrites ? kA3jMaxViewWrites
                                                                  : s_buildViewN);
        snap.viewWriteOvf = (uint16_t)s_buildViewOvf;
        for (uint16_t i = 0; i < snap.viewWriteCount; i++)
            snap.views[i] = s_buildViews[i];
    }
    // Resolve view destinations against registry (and world back-ref).
    for (uint16_t i = 0; i < snap.viewWriteCount; i++)
    {
        A3jViewWrite& w = snap.views[i];
        ArrHit hit{};
        if (ResolvePalette(w.destEa, hit))
        {
            w.scnMdlEa = hit.scn;
            w.arrKind = hit.kind;
        }
        else
        {
            std::lock_guard<std::mutex> lk(s_regMu);
            for (uint32_t ri = 0; ri < s_regN; ri++)
            {
                const RegEntry& e = s_reg[ri];
                if (!e.world || e.world != w.worldEa) continue;
                w.scnMdlEa = e.scn;
                const uint8_t slots = MaxSlots(e.b100, e.b101);
                uint8_t slot = 0;
                if (MatchArray(w.destEa, e.viewPos, e.slotBytes, slots, &slot))
                    w.arrKind = slot ? A3jArr_ViewSlot : A3jArr_ViewPos;
                else if (e.viewNrm && MatchArray(w.destEa, e.viewNrm, e.slotBytes, slots, &slot))
                    w.arrKind = A3jArr_ViewNrm;
                else if (e.viewEnv && MatchArray(w.destEa, e.viewEnv, e.slotBytes, slots, &slot))
                    w.arrKind = A3jArr_ViewEnv;
                break;
            }
        }
        if (w.scnMdlEa)
            RegistryUpsert(w.scnMdlEa, w.lr);
    }

    JoinPals(snap, a3h);
    // Retroactive PaletteMatch on models that own high-hit pals.
    for (uint16_t i = 0; i < snap.modelCount; i++)
    {
        A3jModelRec& m = snap.models[i];
        if (m.ownClass == A3jOwn_PlayerProven || m.ownClass == A3jOwn_ModelImplProven)
            continue;
        for (uint16_t p = 0; p < snap.palCount; p++)
        {
            if (snap.pals[p].scnMdlEa == m.scnMdlEa && snap.pals[p].loadHits >= 64)
            {
                if (m.ownClass == A3jOwn_Unclassified || m.ownClass == A3jOwn_Associated)
                    m.ownClass = A3jOwn_PaletteMatch;
                break;
            }
        }
    }
    PickPrimary(snap);
    SampleRoot(snap);
    Score(snap);

    {
        std::lock_guard<std::mutex> lk(s_mu);
        s_ring[s_ringWrite] = snap;
        s_ringWrite = (s_ringWrite + 1) % kA3jRingSize;
        if (s_ringCount < kA3jRingSize) s_ringCount++;
    }
    Dump(snap);
}

void A3j_OnXfbCopyStandalone()
{
    if (!AnyOn()) return;
    if (A3a_DrawAttrEnabled()) return;  // drained from A3a path
    A3aFrameSnap cur{};
    cur.xfbIndex = Perf_XfbFrameIndex();
    cur.simFrameId = Perf_GuestFrameIndex();
    A3hFrameSnap a3h{};
    const A3hFrameSnap* a3hPtr = A3h_GetLatest(a3h) ? &a3h : nullptr;
    A3j_OnFrame(cur, nullptr, a3hPtr, nullptr);
}

bool A3j_GetLatest(A3jFrameSnap& out)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (!s_ringCount) return false;
    const uint32_t idx = (s_ringWrite + kA3jRingSize - 1) % kA3jRingSize;
    out = s_ring[idx];
    return true;
}

void A3j_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    A3jFrameSnap s{};
    if (!A3j_GetLatest(s)) return;
    std::snprintf(buf, bufSize,
                  "A3j scn=%08x own=%u A=%u/%u B=%u/%u root=%u n=%u",
                  s.primaryScn, s.ownershipStatus, s.groupACount,
                  s.paletteAStatus, s.groupBCount, s.paletteBStatus, s.rootStatus,
                  s.primaryNView);
}

// Guest hooks: only observe via A3i's existing ScnMdlSimpleCalcWorld override
// (A3j_OnCalcWorld). PPC_OVERRIDE on G3dProc / ViewA–C / Construct late-faults
// under TR1 (guest EA 818000a8) — same class as A3i. ViewPos producer is still
// proven by G3dProc disasm (task5 → 805AE*) + dest matching ViewPos/ViewNrm slots.
