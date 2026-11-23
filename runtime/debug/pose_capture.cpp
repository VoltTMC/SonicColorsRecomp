// Approach A3l — production-bounded pose capture (SCR_POSE_CAPTURE).
#include "pose_capture.h"
#include "a1b_pose_capture.h"
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

std::atomic<int> s_on{ -1 };
std::mutex s_mu;
PoseSnap s_ring[kPoseRingSlots]{};
uint32_t s_write = 0;
uint32_t s_count = 0;
uint64_t s_pubSeq = 0;

PoseStats s_stats{};

// Guest-thread CalcWorld association for the live player model (after original).
std::atomic<uint32_t> s_liveScn{ 0 };
std::atomic<uint32_t> s_worldGen{ 0 };
std::atomic<uint32_t> s_worldGenScn{ 0 };

uint32_t s_prevScn = 0;
uint32_t s_prevRes = 0;
uint32_t s_modelGen = 0;

// Cached ResNode hierarchy for current ResMdl (resolved once per modelGen).
struct HierNode
{
    uint16_t nodeIdx = 0;
    int16_t parentIdx = -1;
    uint16_t mtxID = 0xFFFF;
    uint8_t parentOk = 0;
    uint32_t resNodeEa = 0;
};
HierNode s_hier[kPoseMaxNodes]{};
uint16_t s_hierN = 0;
uint16_t s_hierResolved = 0;
uint32_t s_hierResMdl = 0;
uint8_t s_hierOk = 0;

uint64_t s_lastXfb = ~0ull;
uint32_t s_logN = 0;

bool EnvFlag(const char* name)
{
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

bool On()
{
    int v = s_on.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = (EnvFlag("SCR_POSE_CAPTURE") || EnvFlag("SCR_POSE_QUALIFY") ||
             EnvFlag("SCR_POSE_INTERP"))
                ? 1
                : 0;
        s_on.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("PoseCapture: enabled (SCR_POSE_CAPTURE/QUALIFY/INTERP); ring={} nodes≤{} "
                     "bytes/snap≈{}",
                     kPoseRingSlots, kPoseMaxNodes,
                     (unsigned)(sizeof(PoseSnap)));
    }
    return v != 0;
}

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
    if (GuestOk(ea)) return ea;
    return 0;
}

uint32_t Align32(uint32_t n) { return (n + 31u) & ~31u; }

bool ReadMtx(uint32_t ea, PoseMtx3x4& out)
{
    ea = GuestCanon(ea);
    if (!ea || !GuestOk(ea + 0x2C)) return false;
    for (int i = 0; i < 12; i++)
    {
        const uint32_t u = Read32(ea + (uint32_t)(i * 4));
        std::memcpy(&out.m[i], &u, 4);
    }
    return Pose_MtxFinite(out);
}

uint32_t ResDicNum(uint32_t dicEa)
{
    if (!GuestOk(dicEa)) return 0;
    return Read32(dicEa + 4);
}

uint32_t ResDicEntryData(uint32_t dicEa, uint32_t idx)
{
    const uint32_t nodeEa = dicEa + 8u + (idx + 1u) * 0x10u;
    const int32_t ofs = (int32_t)Read32(nodeEa + 0xC);
    if (ofs == 0) return 0;
    return GuestCanon(dicEa + (uint32_t)ofs);
}

bool LooksLikePlayerScn(uint32_t scn, uint16_t* outN)
{
    scn = GuestCanon(scn);
    if (!scn || (scn & 3)) return false;
    if (!GuestOk(scn + kPoseScn_NumView + 1)) return false;
    const uint16_t n = Read16(scn + kPoseScn_NumView);
    // Player Sonic scale (A3k TR1 nView=193). Reject tiny / oversize.
    if (n < 32 || n > (uint16_t)kPoseMaxNodes) return false;
    const uint32_t world = GuestCanon(Read32(scn + kPoseScn_World));
    const uint32_t view = GuestCanon(Read32(scn + kPoseScn_ViewPos));
    const uint32_t res = GuestCanon(Read32(scn + kPoseScn_ResMdl));
    if (!world || !view || !res) return false;
    if (!GuestOk(world) || !GuestOk(view) || !GuestOk(res)) return false;
    if (outN) *outN = n;
    return true;
}

void InvalidateHier()
{
    s_hierN = 0;
    s_hierResolved = 0;
    s_hierResMdl = 0;
    s_hierOk = 0;
    for (int i = 0; i < kPoseMaxNodes; i++) s_hier[i] = {};
}

// A3h-style ResDic → parentIdx / mtxID cache. Fail closed if node count exceeds capacity.
bool ResolveHierarchy(uint32_t resMdl)
{
    resMdl = GuestCanon(resMdl);
    if (!resMdl) return false;
    if (resMdl == s_hierResMdl && s_hierOk) return true;

    InvalidateHier();
    s_hierResMdl = resMdl;

    const int32_t toDic = (int32_t)Read32(resMdl + kPoseResMdl_ToNodeDic);
    if (toDic == 0 || toDic < 0 || toDic > 0x100000) return false;
    const uint32_t dicEa = GuestCanon(resMdl + (uint32_t)toDic);
    if (!dicEa) return false;
    const uint32_t dicN = ResDicNum(dicEa);
    if (dicN == 0) return false;
    if (dicN > (uint32_t)kPoseMaxNodes)
    {
        s_stats.overflow++;
        return false;
    }

    uint32_t nodeEas[kPoseMaxNodes]{};
    const uint16_t take = (uint16_t)dicN;
    for (uint16_t i = 0; i < take; i++)
    {
        const uint32_t nea = GuestCanon(ResDicEntryData(dicEa, i));
        nodeEas[i] = nea;
        HierNode& n = s_hier[i];
        n.nodeIdx = i;
        n.resNodeEa = nea;
        n.parentIdx = -1;
        n.parentOk = 0;
        n.mtxID = 0xFFFF;
        if (!nea || !GuestOk(nea + kPoseResNode_ToParent + 3)) continue;
        n.mtxID = (uint16_t)Read32(nea + kPoseResNode_MtxID);
        const int32_t tp = (int32_t)Read32(nea + kPoseResNode_ToParent);
        if (tp == 0)
        {
            n.parentIdx = -1;
            n.parentOk = 1;
            s_hierResolved++;
        }
        else
        {
            const uint32_t pea = GuestCanon(nea + (uint32_t)tp);
            for (uint16_t j = 0; j < i; j++)
            {
                if (nodeEas[j] == pea)
                {
                    n.parentIdx = (int16_t)j;
                    n.parentOk = 1;
                    s_hierResolved++;
                    break;
                }
            }
        }
    }

    // Second pass for parents that appear later in the dict.
    for (uint16_t i = 0; i < take; i++)
    {
        HierNode& n = s_hier[i];
        if (n.parentOk || !n.resNodeEa) continue;
        const int32_t tp = (int32_t)Read32(n.resNodeEa + kPoseResNode_ToParent);
        if (tp == 0) continue;
        const uint32_t pea = GuestCanon(n.resNodeEa + (uint32_t)tp);
        for (uint16_t j = 0; j < take; j++)
        {
            if (nodeEas[j] == pea)
            {
                n.parentIdx = (int16_t)j;
                n.parentOk = 1;
                s_hierResolved++;
                break;
            }
        }
    }

    s_hierN = take;
    s_hierOk = (s_hierResolved >= take) ? 1 : 0;
    return s_hierOk != 0;
}

void Reject(PoseSnap& s, PoseReject why, PoseStats& st)
{
    s.flags |= PoseFlag_Rejected;
    s.reject = why;
    st.incomplete++;
    switch (why)
    {
    case PoseReject_NoOwner: st.ownFail++; break;
    case PoseReject_BadHier: st.hierFail++; break;
    case PoseReject_Overflow: st.overflow++; break;
    case PoseReject_GenMismatch: st.genMismatch++; break;
    case PoseReject_RelFail: st.relFail++; break;
    case PoseReject_ModelChange: st.modelChange++; break;
    default: break;
    }
}

void Publish(PoseSnap& snap)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (s_count == (uint32_t)kPoseRingSlots) s_stats.ringOverwrite++;
    snap.seq = ++s_pubSeq;
    s_ring[s_write] = snap;
    s_write = (s_write + 1) % kPoseRingSlots;
    if (s_count < (uint32_t)kPoseRingSlots) s_count++;
}

}  // namespace

bool PoseCapture_Enabled() { return On(); }

void PoseCapture_OnCalcWorld(uint32_t scnMdlEa)
{
    if (!On()) return;
    uint16_t n = 0;
    scnMdlEa = GuestCanon(scnMdlEa);
    if (!LooksLikePlayerScn(scnMdlEa, &n)) return;

    A1bPoseSnap ply{};
    if (!(A1b_GetCurrent(ply) && ply.playerValid)) return;

    // Prefer player-scale models (A3k: nView=193).
    if (n < 64) return;

    s_liveScn.store(scnMdlEa, std::memory_order_relaxed);
    s_worldGenScn.store(scnMdlEa, std::memory_order_relaxed);
    s_worldGen.fetch_add(1, std::memory_order_relaxed);
}

void PoseCapture_OnXfb()
{
    if (!On()) return;
    const uint64_t xfb = Perf_XfbFrameIndex();
    if (xfb == s_lastXfb) return;
    s_lastXfb = xfb;
    s_stats.attempts++;

    PoseSnap snap{};
    snap.formatVersion = kPoseFmtVersion;
    snap.simFrameId = Perf_GuestFrameIndex();
    snap.xfbIndex = xfb;
    snap.nodeCapacity = kPoseMaxNodes;

    A1bPoseSnap ply{};
    if (!(A1b_GetCurrent(ply) && ply.playerValid))
    {
        Reject(snap, PoseReject_NoOwner, s_stats);
        return;
    }
    snap.shellEa = ply.playerShellEa;
    snap.bodyEa = ply.playerBodyEa;
    snap.instanceKey = ply.playerShellEa ^ ply.playerBodyEa;
    snap.flags |= PoseFlag_OwnOk;

    const uint32_t scn = GuestCanon(s_liveScn.load(std::memory_order_relaxed));
    uint16_t nView = 0;
    if (!scn || !LooksLikePlayerScn(scn, &nView))
    {
        Reject(snap, PoseReject_NoScn, s_stats);
        return;
    }
    snap.scnMdlEa = scn;
    snap.resMdlEa = GuestCanon(Read32(scn + kPoseScn_ResMdl));
    snap.worldGen = s_worldGen.load(std::memory_order_relaxed);
    if (GuestCanon(s_worldGenScn.load(std::memory_order_relaxed)) != scn)
    {
        Reject(snap, PoseReject_GenMismatch, s_stats);
        return;
    }

    if (scn != s_prevScn || snap.resMdlEa != s_prevRes)
    {
        if (s_prevScn) snap.flags |= PoseFlag_ModelChanged;
        s_modelGen++;
        s_prevScn = scn;
        s_prevRes = snap.resMdlEa;
        InvalidateHier();
    }
    snap.modelGen = s_modelGen;

    if (!ResolveHierarchy(snap.resMdlEa))
    {
        Reject(snap, PoseReject_BadHier, s_stats);
        return;
    }
    snap.flags |= PoseFlag_HierOk;
    snap.parentResolved = s_hierResolved;

    if (s_hierN > (uint16_t)kPoseMaxNodes || nView > (uint16_t)kPoseMaxNodes)
    {
        snap.flags |= PoseFlag_Overflow;
        Reject(snap, PoseReject_Overflow, s_stats);
        return;
    }
    snap.nodeCount = s_hierN;
    snap.inPaletteCount = 0;
    if (s_hierN > s_stats.peakNodes) s_stats.peakNodes = s_hierN;

    const uint8_t b101 = Read8(scn + kPoseScn_Byte101);
    snap.viewSlot = (b101 <= 1) ? b101 : 0;
    snap.slotBytes = Align32((uint32_t)nView * 0x30u);

    const uint32_t worldBase = GuestCanon(Read32(scn + kPoseScn_World));
    const uint32_t viewBase = GuestCanon(Read32(scn + kPoseScn_ViewPos));
    if (!worldBase || !viewBase)
    {
        Reject(snap, PoseReject_WorldIncomplete, s_stats);
        return;
    }
    const uint32_t viewActive = viewBase + snap.viewSlot * snap.slotBytes;

    snap.rootOk = ReadMtx(scn + kPoseScn_RootMtx, snap.root) ? 1 : 0;

    // Copy World/ViewPos for each ResNode via mtxID. Non-palette nodes (mtxID≥nView)
    // are kept for hierarchy identity but do not block completeness.
    uint16_t inPal = 0, worldOkN = 0, viewOkN = 0;
    uint32_t copied = sizeof(PoseMtx3x4);  // root
    for (uint16_t i = 0; i < s_hierN; i++)
    {
        const HierNode& h = s_hier[i];
        PoseNode& nd = snap.nodes[i];
        nd.nodeIdx = h.nodeIdx;
        nd.parentIdx = h.parentIdx;
        nd.mtxID = h.mtxID;
        nd.worldOk = 0;
        nd.viewOk = 0;
        if (h.mtxID >= nView) continue;
        inPal++;
        snap.inPaletteCount = inPal;
        nd.worldOk = ReadMtx(worldBase + (uint32_t)h.mtxID * 0x30u, nd.world) ? 1 : 0;
        nd.viewOk = ReadMtx(viewActive + (uint32_t)h.mtxID * 0x30u, nd.viewPos) ? 1 : 0;
        if (nd.worldOk) worldOkN++;
        else s_stats.invalidMtx++;
        if (nd.viewOk) viewOkN++;
        else s_stats.invalidMtx++;
        copied += 2u * 12u * 4u;
    }
    s_stats.bytesCopied += copied;

    if (inPal == 0 || worldOkN != inPal)
    {
        Reject(snap, PoseReject_WorldIncomplete, s_stats);
        return;
    }
    snap.flags |= PoseFlag_WorldOk;
    if (viewOkN != inPal)
    {
        Reject(snap, PoseReject_ViewIncomplete, s_stats);
        return;
    }
    snap.flags |= PoseFlag_ViewOk;

    // V̂ from palette index 0 (A3k reference), not ResDic index 0.
    PoseMtx3x4 w0{}, v0{};
    if (!ReadMtx(worldBase, w0) || !ReadMtx(viewActive, v0) || !Pose_DeriveVHat(v0, w0, snap.vHat))
    {
        Reject(snap, PoseReject_SingularW0, s_stats);
        return;
    }
    snap.vHatOk = 1;
    snap.flags |= PoseFlag_VHatOk;

    snap.relResMax = 0;
    snap.relResSum = 0;
    snap.relPassCount = 0;
    uint16_t relTried = 0;
    for (uint16_t i = 0; i < s_hierN; i++)
    {
        if (!snap.nodes[i].worldOk || !snap.nodes[i].viewOk) continue;
        float r = 0;
        relTried++;
        if (Pose_ValidateNodeRel(snap.vHat, snap.nodes[i].world, snap.nodes[i].viewPos, kPoseRelTol,
                                 &r))
            snap.relPassCount++;
        snap.relResSum += r;
        if (r > snap.relResMax) snap.relResMax = r;
    }
    if (relTried == 0 || snap.relPassCount != relTried)
    {
        Reject(snap, PoseReject_RelFail, s_stats);
        return;
    }
    snap.relOk = 1;
    snap.flags |= PoseFlag_RelOk;
    snap.flags |= PoseFlag_Complete;

    {
        std::lock_guard<std::mutex> lk(s_mu);
        if (s_count == 0) snap.flags |= PoseFlag_Warmup;
    }

    Publish(snap);
    s_stats.complete++;

    if (s_logN < 8 || (s_logN % 120) == 0)
    {
        LOG_INFO("PoseCapture xfb={} scn={:08x} nodes={}/{} nView={} slot={} gen={} "
                 "relMax={:.5f} pass={}/{} flags={:x} complete={}",
                 xfb, scn, snap.inPaletteCount, s_hierN, nView, snap.viewSlot, snap.modelGen,
                 snap.relResMax, snap.relPassCount, relTried, snap.flags, s_stats.complete);
    }
    s_logN++;
}

bool PoseCapture_GetLatest(PoseSnap& out)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (!s_count) return false;
    const uint32_t idx = (s_write + kPoseRingSlots - 1) % kPoseRingSlots;
    out = s_ring[idx];
    return (out.flags & PoseFlag_Complete) != 0;
}

bool PoseCapture_GetPrevious(PoseSnap& out)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (s_count < 2) return false;
    const uint32_t idx = (s_write + kPoseRingSlots - 2) % kPoseRingSlots;
    out = s_ring[idx];
    return (out.flags & PoseFlag_Complete) != 0;
}

void PoseCapture_GetStats(PoseStats& out)
{
    std::lock_guard<std::mutex> lk(s_mu);
    out = s_stats;
}

void PoseCapture_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!On()) return;
    PoseSnap s{};
    PoseStats st{};
    PoseCapture_GetStats(st);
    if (!PoseCapture_GetLatest(s))
    {
        std::snprintf(buf, bufSize, "Pose cap=%llu inc=%llu",
                      (unsigned long long)st.complete, (unsigned long long)st.incomplete);
        return;
    }
    std::snprintf(buf, bufSize, "Pose scn=%08x n=%u rel=%.4f ok=%llu", s.scnMdlEa, s.nodeCount,
                  s.relResMax, (unsigned long long)st.complete);
}
