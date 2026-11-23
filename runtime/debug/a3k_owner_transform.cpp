// Approach A3k — ownership lifecycle + body/root/world/view composition (opt-in).
#include "a3k_owner_transform.h"
#include "a1b_pose_capture.h"
#include "../game_addrs.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../perf.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace
{

std::atomic<int> s_own{ -1 };
std::atomic<int> s_body{ -1 };
std::atomic<int> s_root{ -1 };
std::atomic<int> s_compose{ -1 };

std::mutex s_mu;
A3kFrameSnap s_ring[kA3kRingSize]{};
uint32_t s_ringCount = 0;
uint32_t s_ringWrite = 0;
uint64_t s_seq = 0;
uint32_t s_dumps = 0;
uint64_t s_lastDrainXfb = ~0ull;

std::mutex s_createMu;
A3kCreateRec s_creates[kA3kMaxCreates]{};
uint32_t s_createN = 0;
uint32_t s_createOvf = 0;
uint32_t s_createSeq = 0;

std::mutex s_bodyMu;
A3kBodySnap s_bodySnaps[kA3kMaxBodySnaps]{};
uint32_t s_bodyN = 0;
uint32_t s_bodyOvf = 0;

uint32_t s_prevPrimary = 0;
uint8_t s_prevStable = 0;

// Latest CalcWorld this under live player (guest thread).
std::atomic<uint32_t> s_liveScn{ 0 };
std::atomic<uint32_t> s_liveScnHits{ 0 };

constexpr float kVwTol = 5e-2f;       // ViewPos vs V·W residual (camera-relative)
constexpr float kVwEstTol = 5e-2f;    // ViewPos vs V̂·W (V̂ from node 0)
constexpr float kRootTol = 5e-2f;     // World[0] vs root exact
constexpr float kRootNearTol = 16.f;  // World[0] vs root same-space / small local
constexpr float kBodyTTol = 1.0f;     // body vs root translation (world units)

// Invert 3×4 affine (upper 3×3 + translation). Returns false if singular.
bool MtxInvert(const float m[12], float out[12])
{
    const float a00 = m[0], a01 = m[1], a02 = m[2];
    const float a10 = m[4], a11 = m[5], a12 = m[6];
    const float a20 = m[8], a21 = m[9], a22 = m[10];
    const float det =
        a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);
    if (!std::isfinite(det) || std::fabs(det) < 1e-8f) return false;
    const float inv = 1.f / det;
    out[0] = (a11 * a22 - a12 * a21) * inv;
    out[1] = (a02 * a21 - a01 * a22) * inv;
    out[2] = (a01 * a12 - a02 * a11) * inv;
    out[4] = (a12 * a20 - a10 * a22) * inv;
    out[5] = (a00 * a22 - a02 * a20) * inv;
    out[6] = (a02 * a10 - a00 * a12) * inv;
    out[8] = (a10 * a21 - a11 * a20) * inv;
    out[9] = (a01 * a20 - a00 * a21) * inv;
    out[10] = (a00 * a11 - a01 * a10) * inv;
    const float tx = m[3], ty = m[7], tz = m[11];
    out[3] = -(out[0] * tx + out[1] * ty + out[2] * tz);
    out[7] = -(out[4] * tx + out[5] * ty + out[6] * tz);
    out[11] = -(out[8] * tx + out[9] * ty + out[10] * tz);
    return true;
}

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
        v = EnvFlag("SCR_A3K_OWNER_LIFETIME") ? 1 : 0;
        s_own.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3k: owner lifetime enabled (SCR_A3K_OWNER_LIFETIME)");
    }
    return v != 0;
}

bool BodyOn()
{
    int v = s_body.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3K_BODY_TRANSFORM") ? 1 : 0;
        s_body.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3k: body transform enabled (SCR_A3K_BODY_TRANSFORM)");
    }
    return v != 0;
}

bool RootOn()
{
    int v = s_root.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3K_MODEL_ROOT") ? 1 : 0;
        s_root.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3k: model root enabled (SCR_A3K_MODEL_ROOT); ScnMdl+0x3C");
    }
    return v != 0;
}

bool ComposeOn()
{
    int v = s_compose.load(std::memory_order_relaxed);
    if (v < 0)
    {
        v = EnvFlag("SCR_A3K_WORLD_VIEW_COMPOSE") ? 1 : 0;
        s_compose.store(v, std::memory_order_relaxed);
        if (v)
            LOG_INFO("A3k: world/view compose enabled (SCR_A3K_WORLD_VIEW_COMPOSE)");
    }
    return v != 0;
}

bool AnyOn() { return OwnOn() || BodyOn() || RootOn() || ComposeOn(); }

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

float ReadF32(uint32_t ea)
{
    ea = GuestCanon(ea);
    if (!ea) return 0.f;
    const uint32_t u = Read32(ea);
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

bool ReadMtx(uint32_t ea, float out[12])
{
    ea = GuestCanon(ea);
    if (!ea || !GuestOk(ea + 0x2C)) return false;
    for (int i = 0; i < 12; i++)
        out[i] = ReadF32(ea + (uint32_t)(i * 4));
    return std::isfinite(out[0]) && std::isfinite(out[11]);
}

void Copy12(const float* src, float* dst)
{
    std::memcpy(dst, src, 12 * sizeof(float));
}

// GC MTX 3×4 row-major: C = A · B (PSMTXConcat convention).
void MtxConcat(const float a[12], const float b[12], float c[12])
{
    for (int r = 0; r < 3; r++)
    {
        const float* ar = a + r * 4;
        for (int col = 0; col < 3; col++)
        {
            c[r * 4 + col] = ar[0] * b[col] + ar[1] * b[4 + col] + ar[2] * b[8 + col];
        }
        c[r * 4 + 3] = ar[0] * b[3] + ar[1] * b[7] + ar[2] * b[11] + ar[3];
    }
}

float MtxMaxAbsDiff(const float a[12], const float b[12])
{
    float m = 0;
    for (int i = 0; i < 12; i++)
    {
        const float d = std::fabs(a[i] - b[i]);
        if (d > m) m = d;
    }
    return m;
}

float Dist3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void MtxT(const float m[12], float t[3])
{
    t[0] = m[3];
    t[1] = m[7];
    t[2] = m[11];
}

uint32_t Align32(uint32_t n) { return (n + 31u) & ~31u; }

bool LooksLikeScn(uint32_t scn)
{
    scn = GuestCanon(scn);
    if (!scn || !GuestOk(scn + kA3kScn_NumView + 1)) return false;
    const uint16_t n = Read16(scn + kA3kScn_NumView);
    if (n == 0 || n > 512) return false;
    const uint32_t world = GuestCanon(Read32(scn + kA3kScn_World));
    const uint32_t view = GuestCanon(Read32(scn + kA3kScn_ViewPos));
    return world && view && GuestOk(world) && GuestOk(view);
}

void RegisterCreate(uint32_t impl, uint32_t scn, uint32_t lr)
{
    impl = GuestCanon(impl);
    scn = GuestCanon(scn);
    if (!impl || !scn) return;
    std::lock_guard<std::mutex> lk(s_createMu);
    // Update existing.
    for (uint32_t i = 0; i < s_createN; i++)
    {
        if (s_creates[i].modelImplEa == impl)
        {
            s_creates[i].scnMdlEa = scn;
            s_creates[i].lr = lr;
            s_creates[i].xfbIndex = Perf_XfbFrameIndex();
            return;
        }
    }
    if (s_createN >= (uint32_t)kA3kMaxCreates)
    {
        s_createOvf++;
        return;
    }
    A3kCreateRec& r = s_creates[s_createN++];
    r = {};
    r.modelImplEa = impl;
    r.scnMdlEa = scn;
    r.lr = lr;
    r.xfbIndex = Perf_XfbFrameIndex();
    r.createIndex = ++s_createSeq;
    if (s_createSeq <= 8 || (s_createSeq % 32) == 0)
        LOG_INFO("A3k create impl={:08x} scn={:08x} lr={:08x} n={}", impl, scn, lr, s_createN);
}

bool FindImplForScn(uint32_t scn, A3kCreateRec& out)
{
    scn = GuestCanon(scn);
    std::lock_guard<std::mutex> lk(s_createMu);
    // Prefer live +0x1C match over stale create record.
    int best = -1;
    for (uint32_t i = 0; i < s_createN; i++)
    {
        const A3kCreateRec& r = s_creates[i];
        if (!r.modelImplEa || !GuestOk(r.modelImplEa + kA3kModelImpl_ScnMdl)) continue;
        const uint32_t held = GuestCanon(Read32(r.modelImplEa + kA3kModelImpl_ScnMdl));
        if (held == scn || r.scnMdlEa == scn)
        {
            best = (int)i;
            if (held == scn) break;
        }
    }
    if (best < 0) return false;
    out = s_creates[best];
    out.scnMdlEa = GuestCanon(Read32(out.modelImplEa + kA3kModelImpl_ScnMdl));
    return out.scnMdlEa == scn || GuestCanon(s_creates[best].scnMdlEa) == scn;
}

bool FindPtrSlot(uint32_t obj, uint32_t target, uint32_t bytes, uint32_t* outOff)
{
    obj = GuestCanon(obj);
    target = GuestCanon(target);
    if (!obj || !target) return false;
    for (uint32_t o = 0; o + 4 <= bytes; o += 4)
    {
        if (!GuestOk(obj + o)) break;
        if (GuestCanon(Read32(obj + o)) == target)
        {
            *outOff = o;
            return true;
        }
    }
    return false;
}

// BFS: shell/body → … → targetEa (ModelImpl or ScnMdl).
bool ProvePathTo(uint32_t shell, uint32_t body, uint32_t target, A3kFrameSnap& snap, uint8_t targetKind)
{
    target = GuestCanon(target);
    shell = GuestCanon(shell);
    body = GuestCanon(body);
    if (!target || (!shell && !body)) return false;

    struct Node
    {
        uint32_t ea;
        uint8_t depth;
        uint8_t fromKind;
        uint16_t fromOff;
        uint32_t parentEa;
    };
    Node q[128]{};
    uint32_t qn = 0;
    uint32_t seen[128]{};
    uint32_t sn = 0;
    auto push = [&](uint32_t ea, uint8_t depth, uint8_t kind, uint16_t off, uint32_t parent) {
        ea = GuestCanon(ea);
        if (!ea || !GuestOk(ea) || depth > 5 || qn >= 128) return;
        for (uint32_t i = 0; i < sn; i++)
            if (seen[i] == ea) return;
        if (sn < 128) seen[sn++] = ea;
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

    int hit = -1;
    for (uint32_t qi = 0; qi < qn; qi++)
    {
        const Node& n = q[qi];
        if (n.ea == target)
        {
            hit = (int)qi;
            break;
        }
        uint32_t off = 0;
        if (FindPtrSlot(n.ea, target, n.depth == 0 ? 0x400u : 0x200u, &off))
        {
            // Synthetic child representing the slot.
            push(target, (uint8_t)(n.depth + 1), targetKind, (uint16_t)off, n.ea);
            // Will be processed; also record immediately.
            hit = (int)qn;  // not yet — fall through after push
            // Reconstruct from current node + slot.
            snap.pathHops = 0;
            auto add = [&](uint32_t ea, uint16_t o, uint8_t k) {
                if (snap.pathHops >= kA3kMaxPathHops) return;
                snap.path[snap.pathHops++] = { ea, o, k, 0 };
            };
            if (n.parentEa) add(n.parentEa, n.fromOff, n.fromKind);
            add(n.ea, (uint16_t)off, n.fromKind ? n.fromKind : 4);
            add(target, 0, targetKind);
            return true;
        }
        if (n.depth >= 4) continue;
        const uint32_t scan = n.depth == 0 ? 0x300u : 0x140u;
        for (uint32_t o = 0; o + 4 <= scan; o += 4)
        {
            if (!GuestOk(n.ea + o)) break;
            const uint32_t p = GuestCanon(Read32(n.ea + o));
            if (!p || p == n.ea || p == target) continue;
            if ((p >> 28) != 0x8 && (p >> 28) != 0x9) continue;
            push(p, (uint8_t)(n.depth + 1), 4, (uint16_t)o, n.ea);
        }
    }
    (void)hit;
    return false;
}

void ResolveOwnership(A3kFrameSnap& snap)
{
    snap.ownKind = A3kOwn_Unresolved;
    snap.ownStatus = A3kStatus_Unverified;
    snap.cmodel1cStatus = A3kStatus_Unverified;
    snap.pathHops = 0;
    snap.modelImplEa = 0;

    if (!snap.primaryScn)
    {
        snap.ownStatus = A3kStatus_Fail;
        return;
    }

    // Direct body/shell pointer?
    uint32_t off = 0;
    if (snap.bodyEa && FindPtrSlot(snap.bodyEa, snap.primaryScn, 0x400u, &off))
    {
        snap.ownKind = A3kOwn_DirectPtr;
        snap.ownStatus = A3kStatus_Pass;
        snap.pathHops = 0;
        snap.path[snap.pathHops++] = { snap.bodyEa, (uint16_t)off, 1, 0 };
        snap.path[snap.pathHops++] = { snap.primaryScn, 0, 6, 0 };
        snap.cmodel1cStatus = A3kStatus_Rejected;  // not via ModelImpl
        return;
    }

    A3kCreateRec cre{};
    const bool haveImpl = FindImplForScn(snap.primaryScn, cre);
    if (haveImpl)
    {
        snap.modelImplEa = cre.modelImplEa;
        snap.createLr = cre.lr;
        const uint32_t held = GuestCanon(Read32(cre.modelImplEa + kA3kModelImpl_ScnMdl));
        if (held == snap.primaryScn)
            snap.cmodel1cStatus = A3kStatus_Pass;
        else
            snap.cmodel1cStatus = A3kStatus_Fail;

        if (ProvePathTo(snap.shellEa, snap.bodyEa, cre.modelImplEa, snap, 5))
        {
            // Append ScnMdl hop via +0x1C.
            if (snap.pathHops < kA3kMaxPathHops)
                snap.path[snap.pathHops++] = { snap.primaryScn, (uint16_t)kA3kModelImpl_ScnMdl, 6, 0 };
            snap.ownKind = A3kOwn_ModelImplPersistent;
            snap.ownStatus = A3kStatus_Pass;
            return;
        }
        // Registry proves ModelImpl+0x1C == ScnMdl, but body graph miss.
        snap.ownKind = A3kOwn_ModelImplOrphan;
        snap.ownStatus = A3kStatus_Partial;
        snap.pathHops = 0;
        if (snap.pathHops < kA3kMaxPathHops)
            snap.path[snap.pathHops++] = { cre.modelImplEa, (uint16_t)kA3kModelImpl_ScnMdl, 5, 0 };
        if (snap.pathHops < kA3kMaxPathHops)
            snap.path[snap.pathHops++] = { snap.primaryScn, 0, 6, 0 };
        return;
    }

    // Call-time only (CalcWorld this under live player).
    if (snap.shellEa || snap.bodyEa)
    {
        snap.ownKind = A3kOwn_CallTimeOnly;
        snap.ownStatus = A3kStatus_Partial;
        // Creates observed, but none hold this ScnMdl at +0x1C → path not used for player.
        uint32_t nCreate = 0;
        {
            std::lock_guard<std::mutex> lk(s_createMu);
            nCreate = s_createN;
        }
        snap.cmodel1cStatus = nCreate ? A3kStatus_Rejected : A3kStatus_Unverified;
    }
    else
        snap.ownStatus = A3kStatus_Fail;
}

void ComposeValidate(A3kFrameSnap& snap);

void SampleTransforms(A3kFrameSnap& snap)
{
    A1bPoseSnap ply{};
    const bool havePly = A1b_GetCurrent(ply) && ply.playerValid;

    if (BodyOn() || ComposeOn() || RootOn())
    {
        if (havePly)
        {
            Copy12(ply.playerWorld.m, snap.bodyWorld);
            snap.bodyWorldOk = 1;
            snap.bodyPos[0] = ply.playerPos[0];
            snap.bodyPos[1] = ply.playerPos[1];
            snap.bodyPos[2] = ply.playerPos[2];
            snap.bodyPosOk = 1;
            if (ply.viewMtxValid)
            {
                Copy12(ply.viewMtx.m, snap.viewCam);
                snap.viewCamOk = 1;
            }
        }
        // Prefer latest BodyGetWorldMtx snap for this body.
        {
            std::lock_guard<std::mutex> lk(s_bodyMu);
            for (uint32_t i = s_bodyN; i > 0; i--)
            {
                const A3kBodySnap& b = s_bodySnaps[i - 1];
                if (snap.bodyEa && b.bodyEa != snap.bodyEa) continue;
                if (b.worldOk)
                {
                    Copy12(b.world, snap.bodyWorld);
                    snap.bodyWorldOk = 1;
                }
                if (b.posOk)
                {
                    snap.bodyPos[0] = b.pos[0];
                    snap.bodyPos[1] = b.pos[1];
                    snap.bodyPos[2] = b.pos[2];
                    snap.bodyPosOk = 1;
                }
                break;
            }
            snap.bodySnapCount = (uint16_t)s_bodyN;
        }
    }

    if (!snap.primaryScn || !LooksLikeScn(snap.primaryScn)) return;

    snap.nView = Read16(snap.primaryScn + kA3kScn_NumView);
    snap.byte100 = Read8(snap.primaryScn + kA3kScn_Byte100);
    snap.byte101 = Read8(snap.primaryScn + kA3kScn_Byte101);
    snap.slotBytes = Align32((uint32_t)snap.nView * 0x30u);
    snap.viewPosBase = GuestCanon(Read32(snap.primaryScn + kA3kScn_ViewPos));
    snap.viewNrmBase = GuestCanon(Read32(snap.primaryScn + kA3kScn_ViewNrm));
    snap.viewEnvBase = GuestCanon(Read32(snap.primaryScn + kA3kScn_ViewEnv));
    snap.world0Ea = GuestCanon(Read32(snap.primaryScn + kA3kScn_World));
    snap.rootMtxEa = snap.primaryScn + kA3kScn_RootMtx;

    if (RootOn() || ComposeOn())
    {
        snap.rootOk = ReadMtx(snap.rootMtxEa, snap.rootMtx) ? 1 : 0;
        snap.world0Ok = ReadMtx(snap.world0Ea, snap.world0) ? 1 : 0;
        uint32_t view0 = snap.viewPosBase;
        if (view0 && snap.slotBytes && snap.byte101 <= 1)
            view0 = snap.viewPosBase + (uint32_t)snap.byte101 * snap.slotBytes;
        snap.view0Ea = view0;
        snap.view0Ok = ReadMtx(view0, snap.view0) ? 1 : 0;
    }

    if (ComposeOn())
        ComposeValidate(snap);
}

void ComposeValidate(A3kFrameSnap& snap)
{
    snap.composeStatus = A3kStatus_Unverified;
    snap.relViewWorld = A3kRel_Unknown;
    snap.relViewEst = A3kRel_Unknown;
    snap.relRootWorld = A3kRel_Unknown;
    snap.relBodyRoot = A3kRel_Unknown;
    snap.composeCount = 0;
    snap.vwResMax = 0;
    snap.vwEstResMax = 0;
    snap.estViewOk = 0;

    if (snap.rootOk && snap.world0Ok)
    {
        snap.rootWorldRes = MtxMaxAbsDiff(snap.rootMtx, snap.world0);
        if (snap.rootWorldRes <= kRootTol)
            snap.relRootWorld = A3kRel_WorldEqRoot;
        else if (snap.rootWorldRes <= kRootNearTol)
            snap.relRootWorld = A3kRel_WorldEqRootTimesLocal;
        else
            snap.relRootWorld = A3kRel_Rejected;
    }

    if (snap.rootOk && snap.bodyWorldOk)
    {
        float rt[3], bt[3];
        MtxT(snap.rootMtx, rt);
        MtxT(snap.bodyWorld, bt);
        snap.bodyRootTRes = Dist3(rt, bt);
        if (snap.bodyRootTRes <= kBodyTTol)
            snap.relBodyRoot = A3kRel_BodyTEqRootT;
    }
    if (snap.rootOk && snap.bodyPosOk)
    {
        float rt[3];
        MtxT(snap.rootMtx, rt);
        snap.bodyPosRootTRes = Dist3(rt, snap.bodyPos);
        if (snap.bodyPosRootTRes <= kBodyTTol)
            snap.relBodyRoot = A3kRel_BodyPosEqRootT;
    }

    if (!snap.world0Ea || !snap.view0Ea || !snap.nView)
    {
        snap.composeStatus = A3kStatus_Partial;
        return;
    }

    uint16_t idxs[kA3kMaxComposeNodes];
    uint16_t nIdx = 0;
    auto addIdx = [&](uint16_t i) {
        if (nIdx >= kA3kMaxComposeNodes || i >= snap.nView) return;
        for (uint16_t j = 0; j < nIdx; j++)
            if (idxs[j] == i) return;
        idxs[nIdx++] = i;
    };
    addIdx(0);
    if (snap.nView > 1) addIdx((uint16_t)(snap.nView / 4));
    if (snap.nView > 2) addIdx((uint16_t)(snap.nView / 2));
    if (snap.nView > 3) addIdx((uint16_t)(snap.nView - 1));
    for (uint16_t i = 1; i < snap.nView && nIdx < kA3kMaxComposeNodes; i++)
        addIdx(i);

    // Build node samples first.
    for (uint16_t ii = 0; ii < nIdx; ii++)
    {
        A3kComposeNode& cn = snap.nodes[snap.composeCount];
        cn = {};
        cn.nodeIndex = idxs[ii];
        cn.worldEa = snap.world0Ea + (uint32_t)idxs[ii] * 0x30u;
        cn.viewEa = snap.view0Ea + (uint32_t)idxs[ii] * 0x30u;
        cn.worldOk = ReadMtx(cn.worldEa, cn.world) ? 1 : 0;
        cn.viewOk = ReadMtx(cn.viewEa, cn.view) ? 1 : 0;
        snap.composeCount++;
    }

    // Estimate V̂ = ViewPos[0] · inv(World[0]) when both valid (PSMTXConcat order).
    float invW0[12]{};
    if (snap.composeCount > 0 && snap.nodes[0].worldOk && snap.nodes[0].viewOk &&
        MtxInvert(snap.nodes[0].world, invW0))
    {
        MtxConcat(snap.nodes[0].view, invW0, snap.estView);
        snap.estViewOk = 1;
    }

    uint16_t passCam = 0, passEst = 0;
    float maxCam = 0, maxEst = 0;
    for (uint16_t i = 0; i < snap.composeCount; i++)
    {
        A3kComposeNode& cn = snap.nodes[i];
        if (cn.worldOk && snap.viewCamOk)
        {
            MtxConcat(snap.viewCam, cn.world, cn.predView);
            cn.predOk = 1;
            cn.residualMax = cn.viewOk ? MtxMaxAbsDiff(cn.view, cn.predView) : 1e9f;
            if (cn.residualMax > maxCam) maxCam = cn.residualMax;
            if (cn.viewOk && cn.residualMax <= kVwTol)
            {
                cn.passVw = 1;
                passCam++;
            }
        }
        if (cn.worldOk && cn.viewOk && snap.estViewOk)
        {
            float pred[12];
            MtxConcat(snap.estView, cn.world, pred);
            const float r = MtxMaxAbsDiff(cn.view, pred);
            if (r > maxEst) maxEst = r;
            if (r <= kVwEstTol) passEst++;
            // Prefer estimated residual in residualMax when camera V fails.
            if (!cn.passVw)
            {
                Copy12(pred, cn.predView);
                cn.predOk = 1;
                cn.residualMax = r;
                if (r <= kVwEstTol) cn.passVw = 1;
            }
        }
    }
    snap.vwResMax = maxCam;
    snap.vwEstResMax = maxEst;

    if (passCam == snap.composeCount && snap.composeCount > 0)
    {
        snap.relViewWorld = A3kRel_ViewEqViewTimesWorld;
        snap.composeStatus = A3kStatus_Pass;
    }
    else if (passCam > 0)
    {
        snap.relViewWorld = A3kRel_ViewEqViewTimesWorld;
        snap.composeStatus = A3kStatus_Partial;
    }
    else if (snap.viewCamOk && snap.composeCount > 0)
        snap.relViewWorld = A3kRel_Rejected;

    if (snap.estViewOk && passEst == snap.composeCount && snap.composeCount > 0)
    {
        snap.relViewEst = A3kRel_ViewEqEstVTimesWorld;
        if (snap.composeStatus != A3kStatus_Pass)
            snap.composeStatus = A3kStatus_Pass;
    }
    else if (snap.estViewOk && passEst > 0)
    {
        snap.relViewEst = A3kRel_ViewEqEstVTimesWorld;
        if (snap.composeStatus == A3kStatus_Unverified || snap.composeStatus == A3kStatus_Fail)
            snap.composeStatus = A3kStatus_Partial;
    }
    else if (snap.estViewOk)
        snap.relViewEst = A3kRel_Rejected;

    if (snap.composeStatus == A3kStatus_Unverified && snap.composeCount > 0)
        snap.composeStatus = A3kStatus_Fail;
}

// Forward declaration fix: SampleTransforms calls ComposeValidate — define order.
// (ComposeValidate defined above after forward decl.)

void DumpSnap(const A3kFrameSnap& s)
{
    if (s_dumps >= 24) return;
    s_dumps++;
    LOG_INFO("A3k xfb={} scn={:08x} impl={:08x} own={} c1c={} hops={} "
             "root={} W0={} Vp0={} n={} b101={} slot={:x}",
             s.xfbIndex, s.primaryScn, s.modelImplEa, s.ownKind, s.cmodel1cStatus, s.pathHops,
             s.rootOk, s.world0Ok, s.view0Ok, s.nView, s.byte101, s.slotBytes);
    if (s.rootOk)
        LOG_INFO("A3k rootT=({:.2f},{:.2f},{:.2f}) world0T=({:.2f},{:.2f},{:.2f}) "
                 "bodyT=({:.2f},{:.2f},{:.2f}) pos=({:.2f},{:.2f},{:.2f})",
                 s.rootMtx[3], s.rootMtx[7], s.rootMtx[11], s.world0[3], s.world0[7], s.world0[11],
                 s.bodyWorld[3], s.bodyWorld[7], s.bodyWorld[11], s.bodyPos[0], s.bodyPos[1],
                 s.bodyPos[2]);
    uint16_t passN = 0;
    for (uint16_t i = 0; i < s.composeCount; i++)
        if (s.nodes[i].passVw) passN++;
    LOG_INFO("A3k compose status={} relVW={} relVWest={} relRW={} relBR={} "
             "vwRes={:.5f} vwEst={:.5f} rootWRes={:.5f} bodyRootT={:.3f} posRootT={:.3f} "
             "creates={} nodes={}/{}",
             s.composeStatus, s.relViewWorld, s.relViewEst, s.relRootWorld, s.relBodyRoot,
             s.vwResMax, s.vwEstResMax, s.rootWorldRes, s.bodyRootTRes, s.bodyPosRootTRes,
             s.createCount, passN, s.composeCount);
    for (uint8_t i = 0; i < s.pathHops && i < 4; i++)
        LOG_INFO("A3k path[{}] ea={:08x} off={:x} kind={}", i, s.path[i].ea, s.path[i].offset,
                 s.path[i].kind);
}

}  // namespace

bool A3k_AnyEnabled() { return AnyOn(); }

void A3k_OnCalcWorld(uint32_t scnMdlEa, uint32_t /*param*/)
{
    if (!AnyOn()) return;
    scnMdlEa = GuestCanon(scnMdlEa);
    if (!LooksLikeScn(scnMdlEa)) return;

    A1bPoseSnap ply{};
    if (A1b_GetCurrent(ply) && ply.playerValid)
    {
        // Prefer large player-like models (A3j: nView=193).
        const uint16_t n = Read16(scnMdlEa + kA3kScn_NumView);
        if (n >= 64)
        {
            s_liveScn.store(scnMdlEa, std::memory_order_relaxed);
            s_liveScnHits.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void A3k_OnBodyWorldMtx(uint32_t bodyEa, const float m[12], uint32_t dirty)
{
    if (!BodyOn() && !ComposeOn()) return;
    bodyEa = GuestCanon(bodyEa);
    if (!bodyEa || !GuestOk(bodyEa + kA3kBody_Pos + 8)) return;

    A3kBodySnap b{};
    b.bodyEa = bodyEa;
    b.dirty = dirty;
    if (m)
    {
        Copy12(m, b.world);
        b.worldOk = 1;
    }
    else if (ReadMtx(bodyEa + kA3kBody_WorldMtx, b.world))
        b.worldOk = 1;
    b.pos[0] = ReadF32(bodyEa + kA3kBody_Pos);
    b.pos[1] = ReadF32(bodyEa + kA3kBody_Pos + 4);
    b.pos[2] = ReadF32(bodyEa + kA3kBody_Pos + 8);
    b.posOk = std::isfinite(b.pos[0]) ? 1 : 0;

    std::lock_guard<std::mutex> lk(s_bodyMu);
    if (s_bodyN >= (uint32_t)kA3kMaxBodySnaps)
    {
        // Ring overwrite oldest.
        std::memmove(s_bodySnaps, s_bodySnaps + 1, (kA3kMaxBodySnaps - 1) * sizeof(A3kBodySnap));
        s_bodyN = kA3kMaxBodySnaps - 1;
        s_bodyOvf++;
    }
    s_bodySnaps[s_bodyN++] = b;
}

void A3k_OnFrame(const A3aFrameSnap& cur, const A3jFrameSnap* a3j)
{
    if (!AnyOn()) return;
    if (cur.xfbIndex == s_lastDrainXfb) return;
    s_lastDrainXfb = cur.xfbIndex;

    A3kFrameSnap snap{};
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

    // Primary ScnMdl: prefer A3j, else live CalcWorld, else A3j-known 809882e0 if live.
    if (a3j && a3j->primaryScn)
        snap.primaryScn = a3j->primaryScn;
    else
        snap.primaryScn = s_liveScn.load(std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lk(s_createMu);
        snap.createCount = (uint16_t)s_createN;
        if (s_createOvf) snap.overflow = (uint16_t)s_createOvf;
    }

    if (OwnOn())
        ResolveOwnership(snap);
    else if (snap.primaryScn)
        snap.ownKind = A3kOwn_CallTimeOnly;

    SampleTransforms(snap);

    if (snap.primaryScn == s_prevPrimary)
        s_prevStable = (uint8_t)(s_prevStable < 255 ? s_prevStable + 1 : 255);
    else
    {
        s_prevPrimary = snap.primaryScn;
        s_prevStable = 1;
    }
    snap.stableFrames = s_prevStable;

    {
        std::lock_guard<std::mutex> lk(s_mu);
        s_ring[s_ringWrite] = snap;
        s_ringWrite = (s_ringWrite + 1) % kA3kRingSize;
        if (s_ringCount < kA3kRingSize) s_ringCount++;
    }

    // Gate dumps: prefer in-stage (body live + large nView).
    if (snap.bodyEa && snap.nView >= 64)
        DumpSnap(snap);
}

void A3k_OnXfbCopyStandalone()
{
    if (!AnyOn()) return;
    // When A3a is on, A3a drains A3k. Standalone path builds a minimal A3a-like snap.
    if (std::getenv("SCR_A3A_DRAW_ATTR") && std::getenv("SCR_A3A_DRAW_ATTR")[0] &&
        std::getenv("SCR_A3A_DRAW_ATTR")[0] != '0')
        return;

    A3aFrameSnap cur{};
    cur.simFrameId = Perf_GuestFrameIndex();
    cur.xfbIndex = Perf_XfbFrameIndex();
    A3jFrameSnap a3j{};
    const A3jFrameSnap* a3jPtr = nullptr;
    if (A3j_AnyEnabled() && A3j_GetLatest(a3j))
        a3jPtr = &a3j;
    A3k_OnFrame(cur, a3jPtr);
}

bool A3k_GetLatest(A3kFrameSnap& out)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (!s_ringCount) return false;
    const uint32_t idx = (s_ringWrite + kA3kRingSize - 1) % kA3kRingSize;
    out = s_ring[idx];
    return true;
}

void A3k_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    A3kFrameSnap s{};
    if (!A3k_GetLatest(s)) return;
    std::snprintf(buf, bufSize,
                  "A3k scn=%08x impl=%08x own=%u c1c=%u VWest=%u RW=%u est=%.4f",
                  s.primaryScn, s.modelImplEa, s.ownKind, s.cmodel1cStatus, s.relViewEst,
                  s.relRootWorld, s.vwEstResMax);
}

// --- Guest hooks -------------------------------------------------------------

// Public wrap (803C6920) → Create (803C6D4C). Observe out-param after wrap returns.
// Do not also override Create: wrap's internal bl already hits the same object.
PPC_OVERRIDE(CModelImplCreateWrap)
{
    const uint32_t outPtr = ctx.r[3];
    const uint32_t lr = ctx.lr;
    PPC_ORIGINAL(CModelImplCreateWrap);
    if (!OwnOn()) return;
    const uint32_t out = GuestCanon(outPtr);
    if (!out || !GuestOk(out)) return;
    const uint32_t impl = GuestCanon(Read32(out));
    if (!impl || !GuestOk(impl + kA3kModelImpl_ScnMdl)) return;
    const uint32_t scn = GuestCanon(Read32(impl + kA3kModelImpl_ScnMdl));
    if (scn)
        RegisterCreate(impl, scn, lr);
}

// Keep Create address registered for documentation / future use; no body override.
PPC_OVERRIDE_TODO(CModelImplCreate);
