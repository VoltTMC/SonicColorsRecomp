// Approach A3i — live Sonic ScnMdl ownership + world/view calculation provenance.
#include "a3i_model_ownership.h"
#include "a3j_model_palette.h"
#include "a3k_owner_transform.h"
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
std::atomic<int> s_world{ -1 };
std::atomic<int> s_view{ -1 };
std::atomic<int> s_pal{ -1 };

std::mutex s_mu;
A3iFrameSnap s_ring[kA3iRingSize]{};
uint32_t s_ringCount = 0;
uint32_t s_ringWrite = 0;
uint64_t s_seq = 0;
uint32_t s_dumps = 0;
uint64_t s_lastDrainXfb = ~0ull;
std::atomic<uint64_t> s_hookHits{ 0 };
std::atomic<uint64_t> s_hookRejected{ 0 };

// Build-frame calc call buffer (guest thread → copied at OnFrame).
std::mutex s_callMu;
A3iCalcCall s_buildCalls[kA3iMaxCalls]{};
uint32_t s_buildCallN = 0;
uint32_t s_buildCallOvf = 0;
uint64_t s_buildXfb = 0;

// Prior primary for stability tracking.
uint32_t s_prevPrimary = 0;
uint8_t s_prevPrimaryStable = 0;

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
        v = EnvFlag("SCR_A3I_MODEL_OWNERSHIP") ? 1 : 0;
        s_own.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3i: model ownership enabled (SCR_A3I_MODEL_OWNERSHIP); "
                     "G3dProc this ↔ live shell/body");
    }
    return v != 0;
}

bool WorldOn()
{
    int v = s_world.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3I_CALC_WORLD") ? 1 : 0;
        s_world.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3i: CalcWorld capture enabled (SCR_A3I_CALC_WORLD)");
    }
    return v != 0;
}

bool ViewOn()
{
    int v = s_view.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3I_CALC_VIEW") ? 1 : 0;
        s_view.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3i: CalcView/ViewDraw capture enabled (SCR_A3I_CALC_VIEW)");
    }
    return v != 0;
}

bool PalOn()
{
    int v = s_pal.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3I_PALETTE_PROVENANCE") ? 1 : 0;
        s_pal.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3i: palette provenance enabled (SCR_A3I_PALETTE_PROVENANCE); "
                     "joins A3h indexed-XF → ScnMdl/stage");
    }
    return v != 0;
}

bool AnyOn() { return OwnOn() || WorldOn() || ViewOn() || PalOn(); }

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
    // Already canonical MEM1/MEM2 effective.
    if (GuestOk(ea)) return ea;
    return 0;
}

uint32_t HashU32(uint32_t a, uint32_t b)
{
    uint32_t h = 2166136261u;
    h ^= a;
    h *= 16777619u;
    h ^= b;
    h *= 16777619u;
    return h;
}

A3iCalcStage StageFromTask(uint32_t task, uint8_t hook)
{
    if (hook == A3iHook_CalcWorld) return A3iStage_CalcWorld;
    switch (task)
    {
    case kA3iTaskCalcWorld: return A3iStage_CalcWorld;
    case kA3iTaskCalcMat: return A3iStage_CalcMat;
    case kA3iTaskCalcViewCb: return A3iStage_CalcViewCb;
    case kA3iTaskViewDraw: return A3iStage_ViewDraw;
    default: return A3iStage_Other;
    }
}

bool WantTask(uint32_t task, uint8_t hook)
{
    if (hook == A3iHook_CalcWorld) return WorldOn() || OwnOn();
    if (task == kA3iTaskCalcWorld) return WorldOn() || OwnOn();
    if (task == kA3iTaskCalcViewCb || task == kA3iTaskViewDraw)
        return ViewOn() || OwnOn();
    // Ownership mode also samples a few other tasks lightly via OwnOn filter below.
    return OwnOn() && (task == kA3iTaskCalcMat || task == kA3iTaskDrawXlu);
}

// Scan holder [0, maxOff) for a word equal to target; return first hit offset.
bool FindPtrSlot(uint32_t holder, uint32_t target, uint32_t maxOff, uint32_t* outOff)
{
    holder = GuestCanon(holder);
    target = GuestCanon(target);
    if (!holder || !target || !outOff) return false;
    const uint32_t lim = maxOff & ~3u;
    for (uint32_t off = 0; off < lim; off += 4)
    {
        if (!GuestOk(holder + off)) break;
        if (GuestCanon(Read32(holder + off)) == target)
        {
            *outOff = off;
            return true;
        }
    }
    return false;
}

bool LooksLikeScnMdl(uint32_t scn, uint16_t* outNum)
{
    scn = GuestCanon(scn);
    if (!scn || (scn & 3)) return false;
    if (!GuestOk(scn + kA3hScnMdl_NumViewMtx)) return false;
    const uint16_t num = Read16(scn + kA3hScnMdl_NumViewMtx);
    if (num < 4 || num > 256) return false;
    const uint32_t res = GuestCanon(Read32(scn + kA3hScnMdl_ResMdl));
    const uint32_t world = GuestCanon(Read32(scn + kA3hScnMdl_WorldMtxArray));
    const uint32_t view = GuestCanon(Read32(scn + kA3hScnMdl_ViewPosMtxArray));
    if (!res || res < 0x80010000u || (res & 3)) return false;
    if (!world || !view || world == view) return false;
    if ((world & 0xF) || (view & 0xF)) return false;
    if (outNum) *outNum = num;
    return true;
}

void FillScnFields(uint32_t scn, A3iCalcCall& c)
{
    scn = GuestCanon(scn);
    if (!scn || !GuestOk(scn + kA3hScnMdl_NumViewMtx)) return;
    c.resMdlEa = GuestCanon(Read32(scn + kA3hScnMdl_ResMdl));
    c.worldMtxEa = GuestCanon(Read32(scn + kA3hScnMdl_WorldMtxArray));
    c.viewPosEa = GuestCanon(Read32(scn + kA3hScnMdl_ViewPosMtxArray));
    c.numViewMtx = Read16(scn + kA3hScnMdl_NumViewMtx);
}

void ClassifyOwnership(A3iCalcCall& c)
{
    c.ownClass = A3iOwn_Unclassified;
    c.playerAssociated = 0;
    c.pathOffset = 0;
    c.pathHolderKind = 0;
    if (!c.scnMdlEa || !c.bodyEa) return;

    uint32_t off = 0;
    // Body first — strongest evidence (player body is large; scan common component range).
    if (FindPtrSlot(c.bodyEa, c.scnMdlEa, 0x2E00, &off))
    {
        c.ownClass = A3iOwn_PlayerProven;
        c.playerAssociated = 1;
        c.pathOffset = off;
        c.pathHolderKind = 1;
        return;
    }
    // shell+0x28 inner object (component hub).
    if (c.shellEa && GuestOk(c.shellEa + 0x28))
    {
        const uint32_t inner = GuestCanon(Read32(c.shellEa + 0x28));
        if (inner && FindPtrSlot(inner, c.scnMdlEa, 0x200, &off))
        {
            c.ownClass = A3iOwn_PlayerProven;
            c.playerAssociated = 1;
            c.pathOffset = off;
            c.pathHolderKind = 2;
            return;
        }
    }
    // Direct shell (rare).
    if (c.shellEa && FindPtrSlot(c.shellEa, c.scnMdlEa, 0x200, &off))
    {
        c.ownClass = A3iOwn_PlayerProven;
        c.playerAssociated = 1;
        c.pathOffset = off;
        c.pathHolderKind = 3;
        return;
    }
    // One-level indirect: body[slot] → component[sub] == ScnMdl*.
    if (c.bodyEa)
    {
        for (uint32_t off1 = 0; off1 < 0x200; off1 += 4)
        {
            if (!GuestOk(c.bodyEa + off1)) break;
            const uint32_t comp = GuestCanon(Read32(c.bodyEa + off1));
            if (!comp || comp == c.bodyEa || comp == c.shellEa) continue;
            uint32_t off2 = 0;
            if (FindPtrSlot(comp, c.scnMdlEa, 0x180, &off2))
            {
                c.ownClass = A3iOwn_PlayerProven;
                c.playerAssociated = 1;
                c.pathOffset = (off1 << 16) | (off2 & 0xFFFF);
                c.pathHolderKind = 1;  // body→comp→scn
                return;
            }
        }
    }
}

void PushCall(A3iCalcCall& c)
{
    std::lock_guard<std::mutex> lk(s_callMu);
    const uint64_t xfb = Perf_XfbFrameIndex();
    if (xfb != s_buildXfb)
    {
        s_buildXfb = xfb;
        s_buildCallN = 0;
        s_buildCallOvf = 0;
    }
    if (s_buildCallN >= (uint32_t)kA3iMaxCalls)
    {
        s_buildCallOvf++;
        return;
    }
    s_buildCalls[s_buildCallN++] = c;
}

void CaptureCall(uint32_t scnMdlEaRaw, uint32_t task, uint32_t param, uint8_t hook)
{
    if (!AnyOn()) return;
    const uint64_t hits = s_hookHits.fetch_add(1, std::memory_order_relaxed) + 1;
    if (hits <= 8)
    {
        LOG_INFO("A3i hook hit #{} hook={} rawThis={:08x} task={} param={:08x}",
                 hits, (unsigned)hook, scnMdlEaRaw, task, param);
    }
    if (!WantTask(task, hook))
    {
        s_hookRejected.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    uint32_t scnMdlEa = GuestCanon(scnMdlEaRaw);
    if (!scnMdlEa || !LooksLikeScnMdl(scnMdlEa, nullptr))
    {
        s_hookRejected.fetch_add(1, std::memory_order_relaxed);
        if (hits <= 16)
            LOG_INFO("A3i reject not-ScnMdl rawThis={:08x} hook={} task={}",
                     scnMdlEaRaw, (unsigned)hook, task);
        return;
    }

    A3iCalcCall c{};
    c.scnMdlEa = scnMdlEa;
    c.task = task;
    c.param = param;
    c.hook = hook;
    c.stage = (uint8_t)StageFromTask(task, hook);
    FillScnFields(scnMdlEa, c);

    A1bPoseSnap ply{};
    if (A1b_GetCurrent(ply) && ply.playerValid)
    {
        c.shellEa = ply.playerShellEa;
        c.bodyEa = ply.playerBodyEa;
        c.instanceKey = HashU32(c.shellEa, c.bodyEa);
    }

    if (OwnOn() && c.bodyEa)
        ClassifyOwnership(c);

    PushCall(c);
}

A3iModelRec* FindOrAddModel(A3iFrameSnap& snap, uint32_t scn)
{
    for (uint16_t i = 0; i < snap.modelCount; i++)
        if (snap.models[i].scnMdlEa == scn) return &snap.models[i];
    if (snap.modelCount >= kA3iMaxModels)
    {
        snap.modelOverflow++;
        return nullptr;
    }
    A3iModelRec& m = snap.models[snap.modelCount++];
    m = {};
    m.scnMdlEa = scn;
    return &m;
}

void BuildModels(A3iFrameSnap& snap)
{
    for (uint16_t i = 0; i < snap.callCount; i++)
    {
        const A3iCalcCall& c = snap.calls[i];
        A3iModelRec* m = FindOrAddModel(snap, c.scnMdlEa);
        if (!m) continue;
        if (!m->resMdlEa) m->resMdlEa = c.resMdlEa;
        if (!m->worldMtxEa) m->worldMtxEa = c.worldMtxEa;
        if (!m->viewPosEa) m->viewPosEa = c.viewPosEa;
        if (!m->viewNrmEa && GuestOk(c.scnMdlEa + 0xF8))
            m->viewNrmEa = GuestCanon(Read32(c.scnMdlEa + 0xF8));
        if (!m->viewEnvEa && GuestOk(c.scnMdlEa + 0xFC))
            m->viewEnvEa = GuestCanon(Read32(c.scnMdlEa + 0xFC));
        if (!m->numViewMtx) m->numViewMtx = c.numViewMtx;
        m->shellEa = c.shellEa;
        m->bodyEa = c.bodyEa;
        m->instanceKey = c.instanceKey;
        if (c.stage == A3iStage_CalcWorld) m->worldCalls++;
        else if (c.stage == A3iStage_CalcViewCb || c.stage == A3iStage_ViewDraw)
            m->viewCalls++;
        else m->otherCalls++;

        // Prefer proven over weaker classifications.
        if (c.ownClass == A3iOwn_PlayerProven ||
            (c.ownClass != A3iOwn_Unclassified && m->ownClass == A3iOwn_Unclassified))
        {
            m->ownClass = c.ownClass;
            m->playerAssociated = c.playerAssociated;
            m->pathOffset = c.pathOffset;
            m->pathHolderKind = c.pathHolderKind;
        }
        if (c.playerAssociated) m->playerAssociated = 1;
    }
}

void PickPrimary(A3iFrameSnap& snap)
{
    // Prefer PlayerProven with largest numViewMtx; else reachable with most world+view calls.
    int bestScore = -1;
    int bestIdx = -1;
    for (uint16_t i = 0; i < snap.modelCount; i++)
    {
        const A3iModelRec& m = snap.models[i];
        int score = 0;
        if (m.ownClass == A3iOwn_PlayerProven) score += 10000;
        else if (m.ownClass == A3iOwn_PaletteMatch) score += 5000;
        else if (m.playerAssociated) score += 1000;
        score += (int)m.numViewMtx * 10;
        score += (int)m.worldCalls * 3 + (int)m.viewCalls * 2;
        if (score > bestScore)
        {
            bestScore = score;
            bestIdx = (int)i;
        }
    }
    if (bestIdx < 0) return;
    const A3iModelRec& m = snap.models[bestIdx];
    snap.primaryScnMdlEa = m.scnMdlEa;
    snap.primaryWorldEa = m.worldMtxEa;
    snap.primaryViewPosEa = m.viewPosEa;
    snap.primaryNumViewMtx = m.numViewMtx;
    snap.primaryOwnClass = m.ownClass;
    if (m.ownClass == A3iOwn_PlayerProven)
    {
        snap.provenPath.scnMdlEa = m.scnMdlEa;
        snap.provenPath.offset = m.pathOffset;
        snap.provenPath.holderKind = m.pathHolderKind;
        if (m.pathHolderKind == 1) snap.provenPath.holderEa = m.bodyEa;
        else if (m.pathHolderKind == 2 && m.shellEa && GuestOk(m.shellEa + 0x28))
            snap.provenPath.holderEa = GuestCanon(Read32(m.shellEa + 0x28));
        else snap.provenPath.holderEa = m.shellEa;
    }
    if (snap.primaryScnMdlEa == s_prevPrimary)
        s_prevPrimaryStable = (uint8_t)(s_prevPrimaryStable < 255 ? s_prevPrimaryStable + 1 : 255);
    else
    {
        s_prevPrimary = snap.primaryScnMdlEa;
        s_prevPrimaryStable = 1;
    }
    // Stamp stability onto matching model.
    for (uint16_t i = 0; i < snap.modelCount; i++)
        if (snap.models[i].scnMdlEa == snap.primaryScnMdlEa)
            snap.models[i].stableFrames = s_prevPrimaryStable;
}

void JoinPalettes(A3iFrameSnap& snap, const A3hFrameSnap* a3h)
{
    if (!PalOn() || !a3h) return;
    for (uint16_t pi = 0; pi < a3h->paletteCount && snap.palLinkCount < kA3iMaxPalLinks; pi++)
    {
        const A3hPalette& ap = a3h->pals[pi];
        if (!ap.baseEa || ap.loadHits == 0) continue;

        A3iPalLink link{};
        link.paletteBase = ap.baseEa;
        link.loadHits = ap.loadHits;
        link.arrayKind = ap.arrayKind;
        link.scnMdlEa = ap.scnMdlEa; // may be 0 from A3h miss

        // Match against captured models' World / ViewPos arrays (exact or sub-range).
        for (uint16_t mi = 0; mi < snap.modelCount; mi++)
        {
            A3iModelRec& m = snap.models[mi];
            uint8_t kind = 0;
            // Double-buffer: G3dProc writes ViewPos + k*align32(n*0x30); slot1 was
            // previously missed when span == n*0x30 (strict < excluded the twin).
            const uint32_t raw = m.numViewMtx ? (uint32_t)m.numViewMtx * 0x30u : 0x30u * 64u;
            const uint32_t slot = (raw + 31u) & ~31u;
            const uint32_t span = slot * 2u;
            auto inSlots = [&](uint32_t base) -> bool {
                return base &&
                       (base == ap.baseEa ||
                        (ap.baseEa >= base && ap.baseEa < base + span &&
                         ((ap.baseEa - base) % (slot ? slot : 0x30u)) == 0));
            };
            if (inSlots(m.viewPosEa))
                kind = 1;
            else if (inSlots(m.viewNrmEa) || inSlots(m.viewEnvEa))
                kind = 1;  // view-family (Nrm/Env); still stage ViewDraw
            else if (inSlots(m.worldMtxEa))
                kind = 2;
            if (!kind) continue;

            link.scnMdlEa = m.scnMdlEa;
            link.arrayKind = kind;
            link.ownClass = m.ownClass;
            link.stage = (kind == 1) ? (uint8_t)A3iStage_ViewDraw : (uint8_t)A3iStage_CalcWorld;
            link.confidence = (m.ownClass == A3iOwn_PlayerProven) ? 3
                              : (m.playerAssociated ? 2 : 1);
            if (m.ownClass == A3iOwn_Unclassified)
            {
                m.ownClass = A3iOwn_PaletteMatch;
                link.ownClass = A3iOwn_PaletteMatch;
            }
            break;
        }

        // Sample first matching row map for mtxID/xfRow.
        for (uint16_t ri = 0; ri < a3h->rowMapCount; ri++)
        {
            const A3hRowMap& r = a3h->rows[ri];
            if (r.paletteBase != ap.baseEa) continue;
            link.guestEaSample = r.guestEa;
            link.mtxID = r.mtxID;
            link.xfRow = r.xfRow;
            if (r.stage == 1 && !link.arrayKind) link.arrayKind = 1;
            if (r.stage == 2 && !link.arrayKind) link.arrayKind = 2;
            break;
        }

        if (!link.scnMdlEa) snap.palUnresolved++;
        snap.pals[snap.palLinkCount++] = link;
    }

    // Retroactively mark models whose arrays appear as high-hit pals.
    for (uint16_t mi = 0; mi < snap.modelCount; mi++)
    {
        A3iModelRec& m = snap.models[mi];
        if (m.ownClass != A3iOwn_Unclassified) continue;
        for (uint16_t li = 0; li < snap.palLinkCount; li++)
        {
            if (snap.pals[li].scnMdlEa == m.scnMdlEa && snap.pals[li].loadHits >= 16)
            {
                m.ownClass = A3iOwn_PaletteMatch;
                break;
            }
        }
    }
}

void ScoreStatus(A3iFrameSnap& snap)
{
    snap.ownershipStatus = A3iStatus_Fail;
    snap.calcWorldStatus = A3iStatus_Fail;
    snap.calcViewStatus = A3iStatus_Fail;
    snap.paletteStatus = A3iStatus_Fail;
    snap.rootCompStatus = A3iStatus_Unverified; // never claimed resolved in A3i alone

    if (snap.primaryOwnClass == A3iOwn_PlayerProven && snap.primaryScnMdlEa)
        snap.ownershipStatus = A3iStatus_Pass;
    else if (snap.primaryScnMdlEa &&
             (snap.primaryOwnClass == A3iOwn_PaletteMatch || snap.playerOwnedCalls > 0))
        snap.ownershipStatus = A3iStatus_Partial;
    else if (snap.modelCount > 0)
        snap.ownershipStatus = A3iStatus_Partial;

    if (snap.worldCalls > 0)
        snap.calcWorldStatus =
            (snap.primaryScnMdlEa && snap.primaryWorldEa) ? A3iStatus_Pass : A3iStatus_Partial;
    // View: G3dProc task5 not hooked; ViewPos array base + A3h join is the evidence.
    if (snap.primaryViewPosEa)
        snap.calcViewStatus = A3iStatus_Pass;
    else if (snap.viewCalls > 0)
        snap.calcViewStatus = A3iStatus_Partial;

    uint16_t mapped = 0;
    for (uint16_t i = 0; i < snap.palLinkCount; i++)
        if (snap.pals[i].scnMdlEa) mapped++;
    if (snap.palLinkCount == 0)
        snap.paletteStatus = A3iStatus_Unverified;
    else if (mapped > 0 && snap.palUnresolved == 0 &&
             snap.primaryOwnClass == A3iOwn_PlayerProven)
        snap.paletteStatus = A3iStatus_Pass;
    else if (mapped > 0)
        snap.paletteStatus = A3iStatus_Partial;
}

void DumpSnap(const A3iFrameSnap& snap)
{
    // Prefer gameplay frames (live shell) and non-empty calc captures.
    const bool hasShell = snap.playerShellEa != 0;
    const bool interesting = snap.callCount > 0 || hasShell || snap.primaryScnMdlEa != 0;
    if (!interesting && s_dumps >= 4) return;
    // Separate budgets: menu-only vs in-stage with shell.
    static uint32_t s_shellDumps = 0;
    if (hasShell)
    {
        if (s_shellDumps >= 32) return;
        s_shellDumps++;
    }
    else if (s_dumps >= 24) return;
    s_dumps++;
    LOG_INFO("A3i xfb={} sim={} shell={:08x} body={:08x} inst={:08x} "
             "calls={} ovf={} models={} world={} view={} plyCalls={} unclass={} "
             "hookHits={} hookRej={}",
             snap.xfbIndex, snap.simFrameId, snap.playerShellEa, snap.playerBodyEa,
             snap.playerInstanceKey, snap.callCount, snap.callOverflow, snap.modelCount,
             snap.worldCalls, snap.viewCalls, snap.playerOwnedCalls, snap.unclassifiedCalls,
             s_hookHits.load(std::memory_order_relaxed),
             s_hookRejected.load(std::memory_order_relaxed));
    LOG_INFO("A3i primary scn={:08x} world={:08x} viewPos={:08x} nView={} own={} "
             "pathKind={} pathOff={:x} holder={:08x} stable={}",
             snap.primaryScnMdlEa, snap.primaryWorldEa, snap.primaryViewPosEa,
             snap.primaryNumViewMtx, snap.primaryOwnClass,
             snap.provenPath.holderKind, snap.provenPath.offset, snap.provenPath.holderEa,
             s_prevPrimaryStable);
    LOG_INFO("A3i status own={} world={} view={} pal={} root={} palLinks={} unresolved={}",
             snap.ownershipStatus, snap.calcWorldStatus, snap.calcViewStatus,
             snap.paletteStatus, snap.rootCompStatus, snap.palLinkCount, snap.palUnresolved);

    const uint16_t nDump = snap.modelCount < 8 ? snap.modelCount : 8;
    for (uint16_t i = 0; i < nDump; i++)
    {
        const A3iModelRec& m = snap.models[i];
        LOG_INFO("A3i mdl[{}] scn={:08x} res={:08x} W={:08x} Vp={:08x} Vn={:08x} Ve={:08x} "
                 "n={} wC={} vC={} own={} path={}/{:x}",
                 i, m.scnMdlEa, m.resMdlEa, m.worldMtxEa, m.viewPosEa, m.viewNrmEa,
                 m.viewEnvEa, m.numViewMtx, m.worldCalls, m.viewCalls, m.ownClass,
                 m.pathHolderKind, m.pathOffset);
    }
    const uint16_t nPal = snap.palLinkCount < 8 ? snap.palLinkCount : 8;
    for (uint16_t i = 0; i < nPal; i++)
    {
        const A3iPalLink& p = snap.pals[i];
        LOG_INFO("A3i pal[{}] base={:08x} hits={} scn={:08x} kind={} stage={} "
                 "own={} conf={} row={} mtxID={} ea={:08x}",
                 i, p.paletteBase, p.loadHits, p.scnMdlEa, p.arrayKind, p.stage,
                 p.ownClass, p.confidence, p.xfRow, p.mtxID, p.guestEaSample);
    }
}

} // namespace

bool A3i_AnyEnabled() { return AnyOn(); }

bool A3i_NeedsPlayerIdentity() { return AnyOn(); }

void A3i_OnG3dProc(uint32_t scnMdlEa, uint32_t task, uint32_t param, uint8_t hook)
{
    CaptureCall(scnMdlEa, task, param, hook);
}

void A3i_OnCalcWorld(uint32_t scnMdlEa, uint32_t param)
{
    CaptureCall(scnMdlEa, kA3iTaskCalcWorld, param, A3iHook_CalcWorld);
}

void A3i_OnFrame(const A3aFrameSnap& cur, const A3gPoseSnap* pose, const A3hFrameSnap* a3h)
{
    if (!AnyOn()) return;
    if (cur.xfbIndex == s_lastDrainXfb) return;  // already drained this XFB
    s_lastDrainXfb = cur.xfbIndex;

    A3iFrameSnap snap{};
    snap.simFrameId = cur.simFrameId;
    snap.xfbIndex = cur.xfbIndex;
    snap.attempted = 1;

    if (pose && pose->playerShellEa)
    {
        snap.playerShellEa = pose->playerShellEa;
        snap.playerBodyEa = pose->playerBodyEa;
        snap.playerInstanceKey = pose->playerInstanceKey;
    }
    else
    {
        A1bPoseSnap ply{};
        if (A1b_GetCurrent(ply) && ply.playerValid)
        {
            snap.playerShellEa = ply.playerShellEa;
            snap.playerBodyEa = ply.playerBodyEa;
            snap.playerInstanceKey = HashU32(ply.playerShellEa, ply.playerBodyEa);
        }
    }

    {
        std::lock_guard<std::mutex> lk(s_callMu);
        const uint32_t n = s_buildCallN;
        snap.callOverflow = (uint16_t)(s_buildCallOvf > 0xFFFF ? 0xFFFF : s_buildCallOvf);
        snap.callCount = (uint16_t)(n > (uint32_t)kA3iMaxCalls ? kA3iMaxCalls : n);
        for (uint16_t i = 0; i < snap.callCount; i++)
        {
            snap.calls[i] = s_buildCalls[i];
            // Re-classify shell/body if call lacked them.
            if (!snap.calls[i].shellEa && snap.playerShellEa)
            {
                snap.calls[i].shellEa = snap.playerShellEa;
                snap.calls[i].bodyEa = snap.playerBodyEa;
                snap.calls[i].instanceKey = snap.playerInstanceKey;
                if (OwnOn()) ClassifyOwnership(snap.calls[i]);
            }
            const A3iCalcCall& c = snap.calls[i];
            if (c.stage == A3iStage_CalcWorld) snap.worldCalls++;
            else if (c.stage == A3iStage_CalcViewCb || c.stage == A3iStage_ViewDraw)
                snap.viewCalls++;
            if (c.ownClass == A3iOwn_PlayerProven || c.playerAssociated)
                snap.playerOwnedCalls++;
            else snap.unclassifiedCalls++;
        }
        s_buildCallN = 0;
        s_buildCallOvf = 0;
    }

    snap.captured = snap.callCount;
    BuildModels(snap);
    JoinPalettes(snap, a3h);
    PickPrimary(snap);
    ScoreStatus(snap);
    snap.seq = ++s_seq;

    {
        std::lock_guard<std::mutex> lk(s_mu);
        s_ring[s_ringWrite] = snap;
        s_ringWrite = (s_ringWrite + 1) % kA3iRingSize;
        if (s_ringCount < kA3iRingSize) s_ringCount++;
    }

    DumpSnap(snap);
}

void A3i_OnXfbCopyStandalone()
{
    if (!AnyOn()) return;
    // A3a path already drained when SCR_A3A_DRAW_ATTR is on.
    if (std::getenv("SCR_A3A_DRAW_ATTR") && std::getenv("SCR_A3A_DRAW_ATTR")[0] &&
        std::getenv("SCR_A3A_DRAW_ATTR")[0] != '0')
        return;

    A3aFrameSnap cur{};
    cur.xfbIndex = Perf_XfbFrameIndex();
    cur.simFrameId = Perf_GuestFrameIndex();
    A3gPoseSnap pose{};
    const A3gPoseSnap* posePtr = A3g_GetLatest(pose) ? &pose : nullptr;
    A3hFrameSnap a3h{};
    const A3hFrameSnap* a3hPtr = A3h_GetLatest(a3h) ? &a3h : nullptr;
    A3i_OnFrame(cur, posePtr, a3hPtr);
}

bool A3i_GetLatest(A3iFrameSnap& out)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (!s_ringCount) return false;
    const uint32_t idx = (s_ringWrite + kA3iRingSize - 1) % kA3iRingSize;
    out = s_ring[idx];
    return true;
}

void A3i_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    if (!AnyOn())
    {
        std::snprintf(buf, bufSize, "A3i: off");
        return;
    }
    A3iFrameSnap s{};
    if (!A3i_GetLatest(s))
    {
        std::snprintf(buf, bufSize, "A3i: waiting");
        return;
    }
    std::snprintf(buf, bufSize,
                  "A3i scn=%08x own=%u W=%u V=%u pals=%u/%u nV=%u wC=%u vC=%u",
                  s.primaryScnMdlEa, s.primaryOwnClass, s.calcWorldStatus,
                  s.calcViewStatus, s.palLinkCount - s.palUnresolved, s.palLinkCount,
                  s.primaryNumViewMtx, s.worldCalls, s.viewCalls);
}

// --- Guest hooks -------------------------------------------------------------

PPC_OVERRIDE(ScnMdlSimpleCalcWorld)
{
    // Capture this before original (r3/r4 may be clobbered). ViewPos may be
    // filled later in G3dProc task5; array base at this+0xF4 is still valid for joins.
    const uint32_t th = ctx.r[3];
    const uint32_t param = ctx.r[4];
    if (AnyOn() && (WorldOn() || OwnOn() || ViewOn() || PalOn()))
        A3i_OnCalcWorld(th, param);
    if (A3j_AnyEnabled())
        A3j_OnCalcWorld(th, param);
    if (A3k_AnyEnabled())
        A3k_OnCalcWorld(th, param);
    PPC_ORIGINAL(ScnMdlSimpleCalcWorld);
}
