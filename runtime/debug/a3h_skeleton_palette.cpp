// Approach A3h — nw4r::g3d ScnMdlSimple / ResNode hierarchy + indexed XF provenance.
#include "a3h_skeleton_palette.h"
#include "a1b_pose_capture.h"
#include "../kernel/memory.h"
#include "../kernel/threads.h"
#include "../log.h"
#include "../perf.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_skel{ -1 };
std::atomic<int> s_pmap{ -1 };
std::atomic<int> s_dump{ -1 };

std::mutex s_mu;
A3hFrameSnap s_ring[kA3hRingSize]{};
uint32_t s_ringCount = 0;
uint32_t s_ringWrite = 0;
uint64_t s_seq = 0;
uint32_t s_dumps = 0;
uint32_t s_playerDumps = 0;

// Build-frame indexed XF loads (GPU thread → copied at OnFrame).
std::mutex s_loadMu;
A3hIdxLoad s_buildLoads[kA3hMaxIdxLoads]{};
uint32_t s_buildLoadN = 0;
uint32_t s_buildLoadOvf = 0;
uint64_t s_buildXfb = 0;

bool EnvFlag(const char* name)
{
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

bool SkelOn()
{
    int v = s_skel.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3H_SKELETON_TRACE") ? 1 : 0;
        s_skel.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3h: skeleton trace enabled (SCR_A3H_SKELETON_TRACE); "
                     "nw4r ScnMdlSimple/ResNode parents; no interpolation");
    }
    return v != 0;
}

bool PmapOn()
{
    int v = s_pmap.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3H_PALETTE_MAP") ? 1 : 0;
        s_pmap.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3h: palette map enabled (SCR_A3H_PALETTE_MAP); "
                     "indexed XF → guest EA / mtxID");
    }
    return v != 0;
}

bool DumpOn()
{
    int v = s_dump.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3H_HIERARCHY_DUMP") ? 1 : 0;
        s_dump.store(v, std::memory_order_relaxed);
    }
    return v != 0;
}

bool AnyOn() { return SkelOn() || PmapOn(); }

// Read32(ea) indexes g_base+ea — ea MUST already be canonical 0x8…/0x9….
// Never treat CanonAddr(raw)==mapped as license to Read32(raw): e.g. 0x4011fc14
// passes a naive check via Canon→0x8011fc14 but Read32(0x4011fc14) host-faults
// (was mis-attributed to CameraManagerTick).
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
    const uint32_t c = CanonAddr(ea);
    return GuestOk(c) ? c : 0;
}

float ReadF32(uint32_t ea)
{
    ea = GuestCanon(ea);
    if (!ea) return 0.f;
    uint32_t u = Read32(ea);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

uint32_t HashMtx12(const float* m)
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

uint32_t HashGuestMtx(uint32_t ea)
{
    ea = GuestCanon(ea);
    if (!ea) return 0;
    float m[12];
    for (int i = 0; i < 12; i++)
    {
        const uint32_t cell = ea + (uint32_t)(i * 4);
        if (!GuestOk(cell)) return 0;
        m[i] = ReadF32(cell);
    }
    return HashMtx12(m);
}

const char* StatusName(uint8_t s)
{
    switch (s)
    {
    case A3hStatus_Pass: return "PASS";
    case A3hStatus_Partial: return "PARTIAL";
    case A3hStatus_Fail: return "FAIL";
    case A3hStatus_NA: return "N/A";
    default: return "UNVERIFIED";
    }
}

// ResDic: data[idx+1].ofsData is relative to ResDicData base.
uint32_t ResDicEntryData(uint32_t dicEa, uint32_t idx)
{
    // ResDicNodeData at dic+8+(idx+1)*0x10; ofsData at +0xC
    const uint32_t nodeEa = dicEa + 8u + (idx + 1u) * 0x10u;
    const int32_t ofs = (int32_t)Read32(nodeEa + 0xC);
    if (ofs == 0) return 0;
    return dicEa + (uint32_t)ofs;
}

uint32_t ResDicNum(uint32_t dicEa)
{
    if (!GuestOk(dicEa)) return 0;
    return Read32(dicEa + 4);
}

bool LooksLikeScnMdlSimple(uint32_t scnEa, uint32_t paletteBase, uint8_t* outKind,
                           uint16_t* outNum)
{
    scnEa = GuestCanon(scnEa);
    paletteBase = GuestCanon(paletteBase);
    if (!scnEa || !paletteBase) return false;
    // Reject obviously non-object alignments.
    if (scnEa & 3) return false;
    const uint32_t viewArr = GuestCanon(Read32(scnEa + kA3hScnMdl_ViewPosMtxArray));
    const uint32_t worldArr = GuestCanon(Read32(scnEa + kA3hScnMdl_WorldMtxArray));
    const uint16_t num = Read16(scnEa + kA3hScnMdl_NumViewMtx);
    if (num == 0 || num > 512) return false;
    const uint32_t resMdl = GuestCanon(Read32(scnEa + kA3hScnMdl_ResMdl));
    if (!resMdl || (resMdl & 3)) return false;
    // Node dictionary offset must be sane; ResNode stride evidence is checked on capture.
    const int32_t toDic = (int32_t)Read32(resMdl + kA3hResMdl_ToNodeDic);
    if (toDic == 0 || toDic < 0 || toDic > 0x100000) return false;
    const uint32_t dicEa = GuestCanon(resMdl + (uint32_t)toDic);
    if (!dicEa) return false;
    const uint32_t dicN = Read32(dicEa + 4);
    if (dicN == 0 || dicN > 512) return false;
    // Soft: info.numViewMtx may differ for envelope models; only reject absurd values.
    const uint32_t infoNum = Read32(resMdl + kA3hResMdl_Info + kA3hResMdlInfo_NumViewMtx);
    if (infoNum > 512) return false;
    if (!viewArr || !worldArr) return false;
    if (viewArr == paletteBase)
    {
        if (outKind) *outKind = 1;
        if (outNum) *outNum = num;
        return true;
    }
    if (worldArr == paletteBase)
    {
        if (outKind) *outKind = 2;
        if (outNum) *outNum = num;
        return true;
    }
    // Draw path may set CP array base to a sub-range of the view/world array.
    const uint32_t viewBytes = (uint32_t)num * 0x30u;
    if (num >= 4 && paletteBase >= viewArr && paletteBase < viewArr + viewBytes &&
        ((paletteBase - viewArr) % 0x30u) == 0)
    {
        if (outKind) *outKind = 1;
        if (outNum) *outNum = num;
        return true;
    }
    if (num >= 4 && paletteBase >= worldArr && paletteBase < worldArr + viewBytes &&
        ((paletteBase - worldArr) % 0x30u) == 0)
    {
        if (outKind) *outKind = 2;
        if (outNum) *outNum = num;
        return true;
    }
    return false;
}

// True if scnEa looks like ScnMdlSimple (ResMdl + mtx arrays), ignoring palette ownership.
bool LooksLikeScnMdlAny(uint32_t scnEa, uint16_t* outNum)
{
    scnEa = GuestCanon(scnEa);
    if (!scnEa || (scnEa & 3)) return false;
    const uint32_t viewArr = GuestCanon(Read32(scnEa + kA3hScnMdl_ViewPosMtxArray));
    const uint32_t worldArr = GuestCanon(Read32(scnEa + kA3hScnMdl_WorldMtxArray));
    const uint16_t num = Read16(scnEa + kA3hScnMdl_NumViewMtx);
    // Sonic-scale skeletons are typically tens of bones, not hundreds of garbage.
    if (num < 4 || num > 256) return false;
    const uint32_t resMdl = GuestCanon(Read32(scnEa + kA3hScnMdl_ResMdl));
    // ResMdl lives in resource heaps (MEM1 mid/high or MEM2), never near null page.
    if (!resMdl || (resMdl & 3) || resMdl < 0x80010000u) return false;
    if (viewArr < 0x80010000u || worldArr < 0x80010000u) return false;
    if ((viewArr & 0xF) || (worldArr & 0xF)) return false;  // MTX34 arrays are aligned
    if (viewArr == worldArr) return false;
    const int32_t toDic = (int32_t)Read32(resMdl + kA3hResMdl_ToNodeDic);
    if (toDic == 0 || toDic < 0 || toDic > 0x100000) return false;
    const uint32_t dicEa = GuestCanon(resMdl + (uint32_t)toDic);
    if (!dicEa) return false;
    const uint32_t dicN = Read32(dicEa + 4);
    if (dicN < 4 || dicN > 256) return false;
    // Spot-check first ResNode: valid EA, mtxID in range, toParent sane.
    const uint32_t n0 = GuestCanon(ResDicEntryData(dicEa, 0));
    if (!n0) return false;
    const uint32_t mtx0 = Read32(n0 + kA3hResNode_MtxID);
    if (mtx0 > 512) return false;
    const int32_t tp0 = (int32_t)Read32(n0 + kA3hResNode_ToParent);
    if (tp0 != 0)
    {
        const uint32_t p0 = GuestCanon(n0 + (uint32_t)tp0);
        if (!p0) return false;
    }
    // Second node often contiguous (+0xD0) when ResNodes are packed — soft signal.
    if (dicN >= 2)
    {
        const uint32_t n1 = GuestCanon(ResDicEntryData(dicEa, 1));
        if (!n1) return false;
    }
    if (outNum) *outNum = num;
    return true;
}

// Depth-limited pointer walk from player shell looking for ScnMdlSimple owning paletteBase.
bool FindScnMdlFromPlayer(uint32_t shellEa, uint32_t paletteBase, uint32_t* outScn,
                          uint8_t* outKind, uint16_t* outNum)
{
    shellEa = GuestCanon(shellEa);
    paletteBase = GuestCanon(paletteBase);
    if (!shellEa || !paletteBase) return false;

    uint32_t stack[96];
    uint8_t depth[96];
    uint32_t n = 0;
    auto push = [&](uint32_t ea, uint8_t d) {
        ea = GuestCanon(ea);
        if (!ea || n >= 96) return;
        for (uint32_t i = 0; i < n; i++)
            if (stack[i] == ea) return;
        stack[n] = ea;
        depth[n] = d;
        n++;
    };

    push(shellEa, 0);
    const uint32_t inner = GuestCanon(Read32(shellEa + 0x28));
    if (inner)
    {
        push(inner, 1);
        // Known component slots on shell+0x28 object (body at +0x84).
        static const uint32_t kOffs[] = { 0x7C, 0x80, 0x84, 0x88, 0x8C, 0x90, 0x94, 0x98,
                                         0x9C, 0xA0, 0xA4, 0xA8, 0xAC, 0xB0 };
        for (uint32_t o : kOffs)
            push(Read32(inner + o), 2);
    }

    for (uint32_t i = 0; i < n; i++)
    {
        const uint32_t obj = stack[i];
        const uint8_t d = depth[i];
        if (LooksLikeScnMdlSimple(obj, paletteBase, outKind, outNum))
        {
            if (outScn) *outScn = obj;
            return true;
        }
        if (d < 5)
        {
            for (uint32_t off = 0; off < 0x180; off += 4)
            {
                const uint32_t p = GuestCanon(Read32(obj + off));
                if (!p) continue;
                if (LooksLikeScnMdlSimple(p, paletteBase, outKind, outNum))
                {
                    if (outScn) *outScn = p;
                    return true;
                }
                if (d < 4 && off < 0xC0)
                    push(p, (uint8_t)(d + 1));
            }
        }
    }
    return false;
}

// Largest ScnMdlSimple reachable from player shell (palette may be a draw-time copy).
bool FindLargestPlayerScnMdl(uint32_t shellEa, uint32_t* outScn, uint16_t* outNum)
{
    shellEa = GuestCanon(shellEa);
    if (!shellEa) return false;
    uint32_t stack[96];
    uint8_t depth[96];
    uint32_t n = 0;
    auto push = [&](uint32_t ea, uint8_t d) {
        ea = GuestCanon(ea);
        if (!ea || n >= 96) return;
        for (uint32_t i = 0; i < n; i++)
            if (stack[i] == ea) return;
        stack[n] = ea;
        depth[n] = d;
        n++;
    };
    push(shellEa, 0);
    const uint32_t inner = GuestCanon(Read32(shellEa + 0x28));
    if (inner)
    {
        push(inner, 1);
        static const uint32_t kOffs[] = { 0x7C, 0x80, 0x84, 0x88, 0x8C, 0x90, 0x94, 0x98,
                                         0x9C, 0xA0, 0xA4, 0xA8, 0xAC, 0xB0 };
        for (uint32_t o : kOffs)
            push(Read32(inner + o), 2);
    }
    uint32_t best = 0;
    uint16_t bestN = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        const uint32_t obj = stack[i];
        const uint8_t d = depth[i];
        uint16_t num = 0;
        if (LooksLikeScnMdlAny(obj, &num) && num > bestN)
        {
            best = obj;
            bestN = num;
        }
        if (d < 5)
        {
            for (uint32_t off = 0; off < 0x180; off += 4)
            {
                const uint32_t p = GuestCanon(Read32(obj + off));
                if (!p) continue;
                if (LooksLikeScnMdlAny(p, &num) && num > bestN)
                {
                    best = p;
                    bestN = num;
                }
                if (d < 4 && off < 0xC0)
                    push(p, (uint8_t)(d + 1));
            }
        }
    }
    if (!best || bestN < 8) return false;
    if (outScn) *outScn = best;
    if (outNum) *outNum = bestN;
    return true;
}

// Fallback: scan a narrow window of pointer-sized words around body for paletteBase backrefs.
bool FindScnMdlByBackref(uint32_t tipEa, uint32_t paletteBase, uint32_t* outScn,
                         uint8_t* outKind, uint16_t* outNum)
{
    paletteBase = GuestCanon(paletteBase);
    if (!paletteBase) return false;
    // Direct structural probes: ScnMdlSimple+0xF4/0xEC == paletteBase.
    const uint32_t direct[] = {
        paletteBase >= kA3hScnMdl_ViewPosMtxArray ? paletteBase - kA3hScnMdl_ViewPosMtxArray : 0,
        paletteBase >= kA3hScnMdl_WorldMtxArray ? paletteBase - kA3hScnMdl_WorldMtxArray : 0,
    };
    for (uint32_t cand : direct)
    {
        cand = GuestCanon(cand);
        if (!cand) continue;
        if (LooksLikeScnMdlSimple(cand, paletteBase, outKind, outNum))
        {
            if (outScn) *outScn = cand;
            return true;
        }
    }
    auto tryAt = [&](uint32_t ea) -> bool {
        if (!GuestOk(ea)) return false;
        // Pointer field stores palette base; ScnMdl starts at field_ea - member_off.
        if (GuestCanon(Read32(ea)) != paletteBase) return false;
        if (ea >= kA3hScnMdl_ViewPosMtxArray)
        {
            const uint32_t candV = GuestCanon(ea - kA3hScnMdl_ViewPosMtxArray);
            if (candV && LooksLikeScnMdlSimple(candV, paletteBase, outKind, outNum))
            {
                if (outScn) *outScn = candV;
                return true;
            }
        }
        if (ea >= kA3hScnMdl_WorldMtxArray)
        {
            const uint32_t candW = GuestCanon(ea - kA3hScnMdl_WorldMtxArray);
            if (candW && LooksLikeScnMdlSimple(candW, paletteBase, outKind, outNum))
            {
                if (outScn) *outScn = candW;
                return true;
            }
        }
        return false;
    };

    // Palette-local window: ScnMdl object often allocated near its mtx arrays.
    // (+0xF4 is a pointer field, NOT inline storage — never use paletteBase-0xF4 alone.)
    if (paletteBase > 0x8000)
    {
        const uint32_t win0 = GuestCanon(paletteBase - 0x8000);
        if (win0)
        {
            for (uint32_t off = 0; off < 0x8000; off += 4)
            {
                if (tryAt(win0 + off)) return true;
            }
        }
    }

    // Player tip window (shell/body) — may hold ScnMdl* or rare direct array ptrs.
    tipEa = GuestCanon(tipEa);
    if (tipEa)
    {
        for (uint32_t off = 0; off < 0x4000; off += 4)
        {
            if (!GuestOk(tipEa + off)) break;
            // Follow pointer slots: tip+off → maybe ScnMdl*.
            const uint32_t p = GuestCanon(Read32(tipEa + off));
            if (p && LooksLikeScnMdlSimple(p, paletteBase, outKind, outNum))
            {
                if (outScn) *outScn = p;
                return true;
            }
            if (tryAt(tipEa + off)) return true;
        }
    }
    return false;
}

bool CaptureHierarchy(uint32_t scnEa, A3hPalette& pal, A3hNode* nodes, uint16_t* nodeN,
                      uint16_t nodeCap)
{
    scnEa = GuestCanon(scnEa);
    if (!scnEa) return false;
    pal.scnMdlEa = scnEa;
    pal.resMdlEa = GuestCanon(Read32(scnEa + kA3hScnMdl_ResMdl));
    pal.numViewMtx = Read16(scnEa + kA3hScnMdl_NumViewMtx);
    if (!pal.resMdlEa || pal.numViewMtx == 0)
        return false;

    const uint32_t toDic = Read32(pal.resMdlEa + kA3hResMdl_ToNodeDic);
    if (toDic == 0) return false;
    const uint32_t dicEa = GuestCanon(pal.resMdlEa + toDic);
    if (!dicEa) return false;
    const uint32_t num = ResDicNum(dicEa);
    if (num == 0 || num > 512) return false;

    // Map ResNodeData EA → dic index for parent resolution.
    uint32_t nodeEas[kA3hMaxNodes];
    const uint16_t take = (uint16_t)std::min<uint32_t>(num, nodeCap);
    for (uint16_t i = 0; i < take; i++)
    {
        const uint32_t nea = GuestCanon(ResDicEntryData(dicEa, i));
        nodeEas[i] = nea;
        A3hNode& n = nodes[i];
        n = A3hNode{};
        n.nodeIdx = i;
        n.resNodeEa = nea;
        if (!nea)
        {
            n.parentIdx = -1;
            continue;
        }
        n.mtxID = (uint16_t)Read32(nea + kA3hResNode_MtxID);
        n.nameOff = Read32(nea + 0x8);
        const int32_t toParent = (int32_t)Read32(nea + kA3hResNode_ToParent);
        if (toParent == 0)
        {
            n.parentIdx = -1;
            n.flags |= A3hNode_Root | A3hNode_ParentOk;
            pal.rootCount++;
        }
        else
        {
            const uint32_t parentEa = nea + (uint32_t)toParent;
            int16_t pidx = -1;
            for (uint16_t j = 0; j < i; j++)
            {
                if (nodeEas[j] == parentEa)
                {
                    pidx = (int16_t)j;
                    break;
                }
            }
            // Parent may appear later in dict — second pass below.
            n.parentIdx = pidx;
            if (pidx >= 0)
            {
                n.flags |= A3hNode_ParentOk;
                pal.parentResolved++;
            }
        }
        if (n.mtxID < pal.numViewMtx)
            n.flags |= A3hNode_InPalette;
    }

    // Second pass: resolve parents that appear after children in dict order.
    for (uint16_t i = 0; i < take; i++)
    {
        A3hNode& n = nodes[i];
        if ((n.flags & A3hNode_ParentOk) || !n.resNodeEa) continue;
        const int32_t toParent = (int32_t)Read32(n.resNodeEa + kA3hResNode_ToParent);
        if (toParent == 0) continue;
        const uint32_t parentEa = n.resNodeEa + (uint32_t)toParent;
        for (uint16_t j = 0; j < take; j++)
        {
            if (nodeEas[j] == parentEa)
            {
                n.parentIdx = (int16_t)j;
                n.flags |= A3hNode_ParentOk;
                pal.parentResolved++;
                break;
            }
        }
    }

    *nodeN = take;
    pal.nodeCount = take;
    pal.hierarchyOk = (pal.parentResolved + pal.rootCount) >= take ? 1 : 0;
    return take > 0;
}

void PushRing(const A3hFrameSnap& snap)
{
    std::memcpy(&s_ring[s_ringWrite], &snap, sizeof(A3hFrameSnap));
    s_ringWrite = (s_ringWrite + 1) % kA3hRingSize;
    if (s_ringCount < kA3hRingSize) s_ringCount++;
}

}  // namespace

bool A3h_SkeletonTraceEnabled() { return SkelOn(); }
bool A3h_PaletteMapEnabled() { return PmapOn(); }
bool A3h_NeedsPlayerIdentity() { return AnyOn(); }

void A3h_OnIndexedXf(uint32_t guestEa, uint32_t arrayBase, uint32_t index,
                     uint32_t stride, uint32_t xfAddr, uint32_t sizeWords)
{
    if (!PmapOn() && !SkelOn()) return;
    // Position matrices live at XF addr < 0x100 (words); one 3×4 = 12 floats = 12 words,
    // but loads may be partial. Track any load into the pos-mtx region.
    if (xfAddr >= 0x100) return;
    if (sizeWords == 0) return;

    // FIFO path must stay cheap: no guest matrix hashing, no g_curThread touch, no mutex
    // during content reads (those were implicated in CameraManagerTick SIGSEGV under load).
    A3hIdxLoad L{};
    L.guestEa = GuestCanon(guestEa);
    L.arrayBase = GuestCanon(arrayBase);
    L.index = (uint16_t)index;
    L.stride = (uint16_t)stride;
    L.xfAddr = (uint16_t)xfAddr;
    L.xfRow = (uint8_t)(xfAddr / 4);
    L.contentHash = 0;  // filled in A3h_OnFrame
    L.lr = 0;

    std::lock_guard lk(s_loadMu);
    if (s_buildLoadN >= (uint32_t)kA3hMaxIdxLoads)
    {
        s_buildLoadOvf++;
        return;
    }
    s_buildLoads[s_buildLoadN++] = L;
}

void A3h_OnFrame(const A3aFrameSnap& cur, const A3gPoseSnap* pose)
{
    if (!AnyOn()) return;

    static A3hFrameSnap s_build{};
    const A3gPoseSnap* posePtr = pose;
    if (!posePtr)
        posePtr = A3g_PeekLastBuild();
    uint32_t idInst = 0, idShell = 0, idBody = 0;
    uint16_t idObs = 0;
    const bool idOk = A3g_GetLatestIdentity(&idInst, &idShell, &idBody, &idObs);

    A3hFrameSnap& snap = s_build;
    // Clear without stack temporaries (sizeof ~70KiB).
    std::memset(&snap, 0, sizeof(snap));
    snap.simFrameId = cur.simFrameId;
    snap.xfbIndex = cur.xfbIndex;
    snap.seq = ++s_seq;

    // Drain indexed XF loads for this XFB window.
    {
        std::lock_guard lk(s_loadMu);
        snap.idxLoadOverflow = (uint16_t)std::min<uint32_t>(s_buildLoadOvf, 65535u);
        snap.idxLoadCount = (uint16_t)s_buildLoadN;
        if (snap.idxLoadCount > 0)
            std::memcpy(snap.loads, s_buildLoads, snap.idxLoadCount * sizeof(A3hIdxLoad));
        s_buildLoadN = 0;
        s_buildLoadOvf = 0;
        s_buildXfb = cur.xfbIndex;
    }
    // Deferred content hashes (safe: XFB boundary, not mid-FIFO).
    for (uint16_t i = 0; i < snap.idxLoadCount; i++)
    {
        A3hIdxLoad& L = snap.loads[i];
        if (!L.guestEa || L.contentHash) continue;
        if (L.stride >= 48)
            L.contentHash = HashGuestMtx(L.guestEa);
        else
        {
            uint32_t h = 2166136261u;
            for (int k = 0; k < 12; k++)
            {
                if (!GuestOk(L.guestEa + (uint32_t)(k * 4))) break;
                h ^= Read32(L.guestEa + (uint32_t)(k * 4));
                h *= 16777619u;
            }
            L.contentHash = h ? h : 1u;
        }
    }

    if (posePtr && posePtr->playerValid)
    {
        snap.playerInstanceKey = posePtr->playerInstanceKey;
        snap.playerShellEa = posePtr->playerShellEa;
        snap.playerBodyEa = posePtr->playerBodyEa;
    }
    else if (idOk)
    {
        snap.playerInstanceKey = idInst;
        snap.playerShellEa = idShell;
        snap.playerBodyEa = idBody;
    }
    else
    {
        A1bPoseSnap ply{};
        if (A1b_GetCurrent(ply) && ply.playerValid)
        {
            snap.playerShellEa = ply.playerShellEa;
            snap.playerBodyEa = ply.playerBodyEa;
            uint32_t h = 2166136261u;
            h ^= ply.playerShellEa;
            h *= 16777619u;
            h ^= ply.playerBodyEa;
            h *= 16777619u;
            snap.playerInstanceKey = h ? h : 1u;
        }
    }

    // Collect unique palette bases from indexed loads (stride 0x30 typical).
    for (uint16_t i = 0; i < snap.idxLoadCount; i++)
    {
        const A3hIdxLoad& L = snap.loads[i];
        uint32_t base = L.arrayBase;
        if (!base && L.guestEa && L.stride)
            base = L.guestEa - (uint32_t)L.index * (uint32_t)L.stride;
        base = GuestCanon(base);
        if (!base) continue;
        bool found = false;
        for (uint16_t p = 0; p < snap.paletteCount; p++)
        {
            if (snap.pals[p].baseEa == base)
            {
                snap.pals[p].loadHits++;
                found = true;
                break;
            }
        }
        if (found) continue;
        if (snap.paletteCount >= kA3hMaxPalettes) continue;
        A3hPalette& pal = snap.pals[snap.paletteCount++];
        pal.baseEa = base;
        pal.loadHits = 1;
    }

    // Resolve ScnMdlSimple + ResNode hierarchy for palettes (prefer player-linked).
    // Process high-hit palettes first so primary selection sees the player skeleton.
    if (SkelOn())
    {
        // Always try to capture the largest player-reachable skeleton as primary,
        // even when GX palette bases are draw-time copies (not ViewPosMtxArray).
        if (snap.playerShellEa && snap.nodeCount < kA3hMaxNodes)
        {
            uint32_t plyScn = 0;
            uint16_t plyNum = 0;
            if (FindLargestPlayerScnMdl(snap.playerShellEa, &plyScn, &plyNum))
            {
                A3hPalette plyPal{};
                plyPal.baseEa = GuestCanon(Read32(plyScn + kA3hScnMdl_ViewPosMtxArray));
                plyPal.arrayKind = 1;
                plyPal.loadHits = 0;
                uint16_t got = 0;
                const uint16_t nodeCap =
                    (uint16_t)std::min<uint32_t>(kA3hMaxNodes - snap.nodeCount, kA3hMaxNodes);
                if (nodeCap && CaptureHierarchy(plyScn, plyPal, snap.nodes + snap.nodeCount, &got,
                                                nodeCap))
                {
                    snap.nodeCount = (uint16_t)(snap.nodeCount + got);
                    snap.nodesWithParent =
                        (uint16_t)(snap.nodesWithParent + plyPal.parentResolved);
                    snap.rootNodes = (uint16_t)(snap.rootNodes + plyPal.rootCount);
                    snap.scnMdlResolved = 1;
                    snap.primaryScnMdlEa = plyScn;
                    snap.primaryResMdlEa = plyPal.resMdlEa;
                    snap.primaryPaletteBase = plyPal.baseEa;
                    snap.primaryNumViewMtx = plyPal.numViewMtx;
                    snap.primaryNodeCount = plyPal.nodeCount;
                    if (DumpOn() && snap.playerInstanceKey && s_playerDumps < 48)
                        LOG_INFO("A3h player-scn scn={:08x} res={:08x} viewArr={:08x} "
                                 "numMtx={} nodes={} parents={} roots={}",
                                 plyScn, plyPal.resMdlEa, plyPal.baseEa, plyPal.numViewMtx,
                                 plyPal.nodeCount, plyPal.parentResolved, plyPal.rootCount);
                }
            }
        }

        uint16_t order[kA3hMaxPalettes];
        for (uint16_t p = 0; p < snap.paletteCount; p++)
            order[p] = p;
        for (uint16_t a = 0; a < snap.paletteCount; a++)
        {
            for (uint16_t b = a + 1; b < snap.paletteCount; b++)
            {
                if (snap.pals[order[b]].loadHits > snap.pals[order[a]].loadHits)
                {
                    const uint16_t t = order[a];
                    order[a] = order[b];
                    order[b] = t;
                }
            }
        }
        for (uint16_t oi = 0; oi < snap.paletteCount; oi++)
        {
            A3hPalette& pal = snap.pals[order[oi]];
            uint32_t scn = 0;
            uint8_t kind = 0;
            uint16_t num = 0;
            bool ok = false;
            if (snap.playerShellEa)
                ok = FindScnMdlFromPlayer(snap.playerShellEa, pal.baseEa, &scn, &kind, &num);
            // Backref only with a verified player tip — title-screen palette storms +
            // wide MEM walks were correlated with CameraManagerTick SIGSEGV @ xfb~2040.
            if (!ok && snap.playerBodyEa)
                ok = FindScnMdlByBackref(snap.playerBodyEa, pal.baseEa, &scn, &kind, &num);
            if (!ok && snap.playerShellEa)
                ok = FindScnMdlByBackref(snap.playerShellEa, pal.baseEa, &scn, &kind, &num);
            // Direct structural probe only (tip=0 → no MEM walk; see FindScnMdlByBackref).
            if (!ok)
                ok = FindScnMdlByBackref(0, pal.baseEa, &scn, &kind, &num);
            if (!ok) continue;

            pal.arrayKind = kind;
            const uint16_t nodeCap =
                (uint16_t)std::min<uint32_t>(kA3hMaxNodes - snap.nodeCount, kA3hMaxNodes);
            if (nodeCap == 0) break;
            uint16_t got = 0;
            if (CaptureHierarchy(scn, pal, snap.nodes + snap.nodeCount, &got, nodeCap))
            {
                snap.nodeCount = (uint16_t)(snap.nodeCount + got);
                snap.nodesWithParent = (uint16_t)(snap.nodesWithParent + pal.parentResolved);
                snap.rootNodes = (uint16_t)(snap.rootNodes + pal.rootCount);
                snap.scnMdlResolved = 1;
                // Prefer the richest skeleton (player body), not a 1-node HUD ScnMdl.
                const bool better =
                    snap.primaryScnMdlEa == 0 ||
                    pal.numViewMtx > snap.primaryNumViewMtx ||
                    (pal.numViewMtx == snap.primaryNumViewMtx &&
                     pal.loadHits > 0 &&
                     (pal.nodeCount > snap.primaryNodeCount ||
                      pal.loadHits > snap.pals[0].loadHits));
                // Do not displace a verified player primary with a weaker HUD/prop.
                if (better && pal.hierarchyOk &&
                    (snap.primaryNumViewMtx == 0 || pal.numViewMtx >= snap.primaryNumViewMtx))
                {
                    snap.primaryScnMdlEa = scn;
                    snap.primaryResMdlEa = pal.resMdlEa;
                    snap.primaryPaletteBase = pal.baseEa;
                    snap.primaryNumViewMtx = pal.numViewMtx;
                    snap.primaryNodeCount = pal.nodeCount;
                }
            }
        }
    }

    // Map A3g observations / draw matrices → guest EA via indexed load hash/row.
    if (PmapOn() && posePtr)
    {
        for (uint16_t oi = 0; oi < posePtr->obsCount; oi++)
        {
            const A3gMtxObs& o = posePtr->obs[oi];
            if (snap.rowMapCount >= kA3hMaxRowMaps)
            {
                snap.rowMapOverflow++;
                continue;
            }
            A3hRowMap& rm = snap.rows[snap.rowMapCount];
            rm = A3hRowMap{};
            rm.xfRow = o.rowIdx;
            rm.contentHash = o.contentHash;
            rm.drawSeq = o.drawSeq;
            rm.obsKey = o.obsKey;

            // Prefer exact content hash match against indexed loads.
            const A3hIdxLoad* hit = nullptr;
            for (uint16_t li = 0; li < snap.idxLoadCount; li++)
            {
                const A3hIdxLoad& L = snap.loads[li];
                if (L.contentHash && L.contentHash == o.contentHash)
                {
                    hit = &L;
                    break;
                }
            }
            // Fallback: last load into this XF row address (rowIdx*3).
            if (!hit)
            {
                const uint16_t wantAddr = (uint16_t)(o.rowIdx * 4u);
                for (int li = (int)snap.idxLoadCount - 1; li >= 0; li--)
                {
                    const A3hIdxLoad& L = snap.loads[li];
                    if (L.xfAddr == wantAddr || L.xfRow == o.rowIdx)
                    {
                        hit = &L;
                        break;
                    }
                }
            }

            if (hit)
            {
                rm.guestEa = hit->guestEa;
                rm.mtxID = hit->index;
                rm.paletteBase = hit->arrayBase ? hit->arrayBase
                                                : (hit->stride ? hit->guestEa -
                                                                     (uint32_t)hit->index *
                                                                         (uint32_t)hit->stride
                                                               : 0);
                rm.stage = 1;  // view-pos palette candidate (indexed XF)
                // Attach node if hierarchy known.
                for (uint16_t ni = 0; ni < snap.nodeCount; ni++)
                {
                    if (snap.nodes[ni].mtxID == rm.mtxID)
                    {
                        rm.nodeIdx = (int16_t)snap.nodes[ni].nodeIdx;
                        snap.nodes[ni].flags |= A3hNode_MatchedObs;
                        break;
                    }
                }
                snap.mappedRows++;
            }
            else
            {
                rm.stage = 0;
                snap.unresolvedRows++;
            }
            snap.rowMapCount++;
        }
    }

    // Temporal: same primary ScnMdl as previous ring entry.
    if (s_ringCount > 0 && snap.primaryScnMdlEa)
    {
        const uint32_t prevIdx = (s_ringWrite + kA3hRingSize - 1) % kA3hRingSize;
        const A3hFrameSnap& prev = s_ring[prevIdx];
        snap.sameSkelPrev =
            (prev.primaryScnMdlEa == snap.primaryScnMdlEa &&
             prev.primaryResMdlEa == snap.primaryResMdlEa)
                ? 1
                : 0;
    }

    // Status aggregation — judge primary skeleton only (sums across pals inflate roots).
    uint16_t primParents = 0, primRoots = 0;
    for (uint16_t i = 0; i < snap.nodeCount && i < snap.primaryNodeCount; i++)
    {
        if (snap.nodes[i].flags & A3hNode_Root) primRoots++;
        else if (snap.nodes[i].flags & A3hNode_ParentOk) primParents++;
    }
    if (snap.primaryNodeCount > 0)
    {
        snap.nodesWithParent = primParents;
        snap.rootNodes = primRoots;
    }
    if (snap.scnMdlResolved && snap.primaryNodeCount > 0 &&
        primParents + primRoots >= snap.primaryNodeCount && primRoots >= 1)
        snap.hierarchyStatus = A3hStatus_Pass;
    else if (snap.scnMdlResolved && primParents > 0)
        snap.hierarchyStatus = A3hStatus_Partial;
    else if (snap.scnMdlResolved)
        snap.hierarchyStatus = A3hStatus_Partial;
    else if (SkelOn())
        snap.hierarchyStatus = A3hStatus_Fail;
    else
        snap.hierarchyStatus = A3hStatus_NA;

    if (snap.mappedRows > 0 && snap.unresolvedRows == 0)
        snap.paletteMapStatus = A3hStatus_Pass;
    else if (snap.mappedRows > 0)
        snap.paletteMapStatus = A3hStatus_Partial;
    else if (PmapOn() && snap.idxLoadCount > 0)
        snap.paletteMapStatus = A3hStatus_Fail;
    else if (PmapOn())
        snap.paletteMapStatus = A3hStatus_Fail;
    else
        snap.paletteMapStatus = A3hStatus_NA;

    if (snap.hierarchyStatus == A3hStatus_Pass &&
        (snap.paletteMapStatus == A3hStatus_Pass ||
         snap.paletteMapStatus == A3hStatus_Partial))
        snap.completeness = A3hStatus_Pass;
    else if (snap.scnMdlResolved || snap.mappedRows > 0)
        snap.completeness = A3hStatus_Partial;
    else
        snap.completeness = A3hStatus_Fail;

    {
        std::lock_guard lk(s_mu);
        PushRing(snap);
    }

    const bool dump = DumpOn() || AnyOn();
    // Prefer gameplay evidence. Keep a dedicated budget for player-owned frames so
    // title-screen palette spam cannot exhaust dumps before TR1 gameplay.
    const bool hasPlayer = snap.playerInstanceKey != 0;
    const bool interesting =
        hasPlayer || snap.scnMdlResolved || snap.mappedRows > 0 ||
        (!hasPlayer && (snap.paletteCount >= 3 || snap.idxLoadCount >= 64));
    const bool underBudget =
        hasPlayer ? (s_playerDumps < 48) : (s_dumps < 24);
    const bool cadence =
        hasPlayer
            ? (s_playerDumps < 16 || (cur.xfbIndex % 10) == 0 || snap.scnMdlResolved ||
               snap.completeness == A3hStatus_Pass)
            : (s_dumps < 8 || (cur.xfbIndex % 40) == 0);
    if (dump && interesting && underBudget && cadence)
    {
        LOG_INFO("A3h frame: xfb={} sim={} inst={:08x} idxLoads={}/{}+ovf{} pals={} "
                 "scn={:08x} res={:08x} palBase={:08x} numMtx={} nodes={} "
                 "parents={} roots={} rowMap={}/{} mapped={} unres={} "
                 "hier={} pmap={} complete={} sameSkel={}",
                 snap.xfbIndex, snap.simFrameId, snap.playerInstanceKey,
                 snap.idxLoadCount, kA3hMaxIdxLoads, snap.idxLoadOverflow, snap.paletteCount,
                 snap.primaryScnMdlEa, snap.primaryResMdlEa, snap.primaryPaletteBase,
                 snap.primaryNumViewMtx, snap.primaryNodeCount, snap.nodesWithParent,
                 snap.rootNodes, snap.rowMapCount, kA3hMaxRowMaps, snap.mappedRows,
                 snap.unresolvedRows, StatusName(snap.hierarchyStatus),
                 StatusName(snap.paletteMapStatus), StatusName(snap.completeness),
                 snap.sameSkelPrev);
        for (uint16_t i = 0; i < snap.paletteCount && i < 6; i++)
        {
            const A3hPalette& pal = snap.pals[i];
            LOG_INFO("A3h pal[{}] base={:08x} scn={:08x} kind={} numMtx={} nodes={} "
                     "hits={} hierOk={}",
                     i, pal.baseEa, pal.scnMdlEa, pal.arrayKind, pal.numViewMtx, pal.nodeCount,
                     pal.loadHits, pal.hierarchyOk);
        }
        for (uint16_t i = 0; i < snap.primaryNodeCount && i < 16 && i < snap.nodeCount; i++)
        {
            const A3hNode& n = snap.nodes[i];
            LOG_INFO("A3h node[{}] ea={:08x} mtxID={} parent={} flags={:02x}",
                     n.nodeIdx, n.resNodeEa, n.mtxID, n.parentIdx, n.flags);
        }
        for (uint16_t i = 0; i < snap.rowMapCount && i < 10; i++)
        {
            const A3hRowMap& r = snap.rows[i];
            LOG_INFO("A3h row[{}] xfRow={} mtxID={} node={} guest={:08x} stage={} "
                     "hash={:08x}",
                     i, r.xfRow, r.mtxID, r.nodeIdx, r.guestEa, r.stage, r.contentHash);
        }
        if (hasPlayer)
            s_playerDumps++;
        else
            s_dumps++;
    }
}

bool A3h_GetLatest(A3hFrameSnap& out)
{
    std::lock_guard lk(s_mu);
    if (s_ringCount == 0) return false;
    const uint32_t idx = (s_ringWrite + kA3hRingSize - 1) % kA3hRingSize;
    // Never assign huge snaps via operator= on a stack destination — memcpy only.
    std::memcpy(&out, &s_ring[idx], sizeof(A3hFrameSnap));
    return true;
}

void A3h_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!AnyOn()) return;
    // A3hFrameSnap is ~70KiB — must not live on the overlay/UI stack (SIGSEGV).
    static A3hFrameSnap s_overlay{};
    if (!A3h_GetLatest(s_overlay))
    {
        std::snprintf(buf, bufSize, "A3h: waiting");
        return;
    }
    std::snprintf(buf, bufSize, "A3h %s hier=%s map=%s nodes=%u mapped=%u scn=%08x",
                  StatusName(s_overlay.completeness), StatusName(s_overlay.hierarchyStatus),
                  StatusName(s_overlay.paletteMapStatus), s_overlay.primaryNodeCount,
                  s_overlay.mappedRows, s_overlay.primaryScnMdlEa);
}
