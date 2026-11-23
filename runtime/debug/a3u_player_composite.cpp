// Approach A3u — pre-player EFB background + player-only mid segment replay.
#include "a3u_player_composite.h"
#include "a2b_fifo_archive.h"
#include "pose_capture.h"
#include "../gx/gx_state.h"
#include "../kernel/memory.h"
#include "../log.h"
#include "../perf.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mutex>

namespace
{

constexpr int kBgSlots = 4;

std::atomic<int> s_on{ -1 };
std::atomic<int> s_audit{ -1 };
std::mutex s_mu;
A3uStats s_st{};

struct BgSlot
{
    uint64_t xfbIndex = 0;
    uint64_t simFrame = 0;
    uint32_t fifoOff = 0;
    uint32_t scnMdlEa = 0;
    uint32_t viewBaseEa = 0;
    GXState gx{};
    bool valid = false;
    bool hasGx = false;
};

BgSlot s_slots[kBgSlots]{};
int s_write = 0;

// Per-XFB live classification.
uint64_t s_xfbTag = ~0ull;
uint32_t s_drawsBefore = 0, s_drawsPlayer = 0, s_drawsAfter = 0;
uint32_t s_firstPlayerSeq = 0, s_lastPlayerSeq = 0;
uint32_t s_drawSeq = 0;
bool s_seenNonPlayerAfter = false;
bool s_bgCaptured = false;
bool s_inPlayerSpan = false;
uint64_t s_playerRowMask = 0;  // XF pos-matrix row indices (0..63) from ViewPos loads
uint32_t s_viewBase0 = 0, s_viewBase1 = 0, s_viewActive = 0;
uint32_t s_scnEa = 0;
uint32_t s_fifoOffAtCapture = 0;
GXState s_gxAtCapture{};
bool s_haveGxCapture = false;

bool s_playerOnlyMid = false;
int s_midSlot = -1;

bool EnvOn()
{
    int v = s_on.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_A3U_PLAYER_COMPOSITE");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_on.store(v, std::memory_order_relaxed);
        if (v) LOG_INFO("A3u: player-only mid composite enabled (SCR_A3U_PLAYER_COMPOSITE)");
    }
    return v != 0;
}

bool AuditOn()
{
    int v = s_audit.load(std::memory_order_relaxed);
    if (v < 0)
    {
        const char* e = std::getenv("SCR_A3U_AUDIT");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        s_audit.store(v, std::memory_order_relaxed);
    }
    return v != 0 || EnvOn();
}

void RefreshViewBases()
{
    PoseSnap snap{};
    if (!PoseCapture_GetLatest(snap) || !snap.scnMdlEa) return;
    s_scnEa = snap.scnMdlEa;
    // ViewPos pointer at ScnMdl+0xF4 (kPoseScn_ViewPos); dual-buffered by slotBytes.
    const uint32_t view = CanonAddr(Read32(snap.scnMdlEa + kPoseScn_ViewPos));
    if (!view) return;
    s_viewBase0 = view;
    const uint32_t slotBytes = snap.slotBytes ? snap.slotBytes : 0x30u * 193u;
    s_viewBase1 = view + slotBytes;
    s_viewActive = view + snap.viewSlot * slotBytes;
}

bool EaInViewPos(uint32_t guestEa)
{
    if (!s_viewBase0) RefreshViewBases();
    if (!s_viewBase0) return false;
    const uint32_t ea = CanonAddr(guestEa);
    auto in = [&](uint32_t base) {
        if (!base) return false;
        const uint32_t bytes =
            (s_viewBase1 > base) ? (s_viewBase1 - base) : (193u * 0x30u);
        return ea >= base && ea < base + bytes + 0x30u;  // allow last matrix
    };
    return in(s_viewBase0) || in(s_viewBase1) || in(s_viewActive);
}

bool DrawUsesPlayerRows(bool posMtxDirect, uint8_t defRow, const uint8_t* hostVtxMtx0,
                        uint32_t vtxCount, uint32_t vtxStride)
{
    if (s_playerRowMask == 0) return false;
    if (!posMtxDirect)
        return defRow < 64 && (s_playerRowMask & (1ull << defRow)) != 0;
    if (!hostVtxMtx0 || vtxCount == 0 || vtxStride < 1) return false;
    const uint32_t n = vtxCount < 32 ? vtxCount : 32;
    for (uint32_t i = 0; i < n; i++)
    {
        // HostVertex::mtx[0] is first byte after pos[3]+pad — caller passes mtx field ptr
        // when available; otherwise we only have defRow path.
        const uint8_t row = hostVtxMtx0[i * vtxStride] & 63;
        if (s_playerRowMask & (1ull << row)) return true;
    }
    return false;
}

void ResetXfbLocal(uint64_t xfb)
{
    s_xfbTag = xfb;
    s_drawsBefore = s_drawsPlayer = s_drawsAfter = 0;
    s_firstPlayerSeq = s_lastPlayerSeq = 0;
    s_drawSeq = 0;
    s_seenNonPlayerAfter = false;
    s_bgCaptured = false;
    s_inPlayerSpan = false;
    s_playerRowMask = 0;
    s_fifoOffAtCapture = 0;
    s_haveGxCapture = false;
    RefreshViewBases();
}

}  // namespace

bool A3u_Enabled() { return EnvOn(); }
bool A3u_AuditOn() { return AuditOn(); }

static void TryCapturePrePlayer(uint64_t xfb)
{
    if (s_bgCaptured || !EnvOn()) return;
    Render_Flush();  // finish any pending env draws before snapshot
    const int slot = s_write % kBgSlots;
    if (!Render_A3uCapturePrePlayer(slot))
    {
        s_st.bgCaptureFail++;
        return;
    }
    s_fifoOffAtCapture = A2b_CurrentCmdBytes();
    s_gxAtCapture = g_gx;
    s_haveGxCapture = true;
    {
        std::lock_guard lk(s_mu);
        BgSlot& b = s_slots[slot];
        b.xfbIndex = xfb;
        b.simFrame = Perf_GuestFrameIndex();
        b.fifoOff = s_fifoOffAtCapture;
        b.scnMdlEa = s_scnEa;
        b.viewBaseEa = s_viewBase0;
        b.gx = s_gxAtCapture;
        b.hasGx = true;
        b.valid = true;
        s_write++;
        s_st.bgCaptureOk++;
        s_st.scnMdlEa = s_scnEa;
        s_st.viewBaseEa = s_viewBase0;
        s_st.playerFifoOff = s_fifoOffAtCapture;
    }
    s_bgCaptured = true;
    if (AuditOn() && (s_st.bgCaptureOk <= 4 || (s_st.bgCaptureOk % 120) == 0))
        LOG_INFO("A3u bgCapture xfb={} fifoOff={} scn={:08x} view={:08x} rows={:016x}",
                 (unsigned long long)xfb, s_fifoOffAtCapture, s_scnEa, s_viewBase0,
                 (unsigned long long)s_playerRowMask);
}

static uint32_t XfMatrixIndex(uint32_t xfAddr)
{
    // GX pos matrix N occupies XF words [N*12, N*12+12).
    return (xfAddr / 12u) % 64u;
}

void A3u_OnIndexedXf(uint32_t guestEa, uint32_t xfAddr, uint32_t floatCount)
{
    if (!EnvOn() && !AuditOn()) return;
    if (floatCount < 12 || xfAddr >= 0x1000) return;
    const uint32_t row = XfMatrixIndex(xfAddr);
    // Mid player-only: maintain row ownership as archived XF loads replay.
    if (s_playerOnlyMid)
    {
        if (!EaInViewPos(guestEa))
            s_playerRowMask &= ~(1ull << row);
        // PoseInterp path uses NotePlayerXfRow; guest ViewPos still marks rows.
        else
            s_playerRowMask |= (1ull << row);
        return;
    }
    if (Render_ReplayIsolationActive()) return;
    const uint64_t xfb = Perf_XfbFrameIndex();
    if (xfb != s_xfbTag) ResetXfbLocal(xfb);
    if (!EaInViewPos(guestEa))
    {
        // Non-ViewPos load into this matrix slot — no longer player-owned.
        s_playerRowMask &= ~(1ull << row);
        return;
    }
    const bool first = (s_playerRowMask == 0);
    s_playerRowMask |= (1ull << row);
    // First ViewPos→XF load: environment draws before this point are flushed+captured.
    if (first) TryCapturePrePlayer(xfb);
}

bool A3u_OnDrawClassify(bool posMtxDirect, uint8_t defRow, const uint8_t* hostVtxMtx0,
                        uint32_t vtxCount, uint32_t vtxStride)
{
    if (!EnvOn() && !AuditOn()) return false;
    if (Render_ReplayIsolationActive()) return false;
    const uint64_t xfb = Perf_XfbFrameIndex();
    if (xfb != s_xfbTag) ResetXfbLocal(xfb);

    const uint32_t seq = ++s_drawSeq;
    const bool player = DrawUsesPlayerRows(posMtxDirect, defRow, hostVtxMtx0, vtxCount, vtxStride);

    if (player)
    {
        if (!s_bgCaptured && EnvOn()) TryCapturePrePlayer(xfb);
        if (!s_inPlayerSpan)
        {
            s_inPlayerSpan = true;
            s_firstPlayerSeq = seq;
        }
        s_lastPlayerSeq = seq;
        s_drawsPlayer++;
        return true;
    }

    if (s_inPlayerSpan)
    {
        s_seenNonPlayerAfter = true;
        s_drawsAfter++;
    }
    else
        s_drawsBefore++;
    return false;
}

void A3u_OnXfb()
{
    if (!EnvOn() && !AuditOn()) return;
    std::lock_guard lk(s_mu);
    s_st.xfbSeen++;
    s_st.drawsBefore = s_drawsBefore;
    s_st.drawsPlayer = s_drawsPlayer;
    s_st.drawsAfter = s_drawsAfter;
    s_st.playerContiguous = (s_drawsPlayer > 0 && !s_seenNonPlayerAfter) ? 1 : 0;
    s_st.bgValid = s_bgCaptured ? 1 : 0;
    s_st.playerFifoOff = s_fifoOffAtCapture;
    if (AuditOn() && s_drawsPlayer > 0 &&
        (s_st.xfbSeen <= 6 || (s_st.xfbSeen % 120) == 0))
        LOG_INFO("A3u audit xfb={} before={} player={} after={} contig={} bg={} fifoOff={}",
                 (unsigned long long)s_xfbTag, s_drawsBefore, s_drawsPlayer, s_drawsAfter,
                 s_st.playerContiguous, s_st.bgValid, s_fifoOffAtCapture);
    // Next XFB resets via tag mismatch.
    s_xfbTag = ~0ull;
}

void A3u_BeginPlayerOnlyMid() { s_playerOnlyMid = EnvOn(); }
void A3u_EndPlayerOnlyMid()
{
    s_playerOnlyMid = false;
    s_midSlot = -1;
}
bool A3u_PlayerOnlyMidActive() { return s_playerOnlyMid; }

bool A3u_ShouldSkipDraw(bool posMtxDirect, uint8_t defRow, const uint8_t* hostVtxMtx0,
                        uint32_t vtxCount, uint32_t vtxStride)
{
    if (!s_playerOnlyMid) return false;
    // During mid, player rows are those overridden by PoseInterp (same mask refreshed
    // when OverrideIndexed succeeds — see A3u_NotePlayerXfRow).
    return !DrawUsesPlayerRows(posMtxDirect, defRow, hostVtxMtx0, vtxCount, vtxStride);
}

void A3u_NotePlayerXfRow(uint32_t xfAddr, uint32_t floatCount)
{
    if (!s_playerOnlyMid && !EnvOn()) return;
    if (floatCount < 12 || xfAddr >= 0x1000) return;
    const uint32_t row = XfMatrixIndex(xfAddr);
    s_playerRowMask |= (1ull << row);
}

bool A3u_PrepareMid(uint64_t xfbIndex, uint32_t* outFifoOff, uint32_t* outCmdBytes)
{
    if (!EnvOn()) return false;
    std::lock_guard lk(s_mu);
    s_st.midAttempt++;
    int found = -1;
    for (int i = 0; i < kBgSlots; i++)
    {
        const int idx = (s_write - 1 - i + kBgSlots * 8) % kBgSlots;
        if (s_slots[idx].valid && s_slots[idx].xfbIndex == xfbIndex)
        {
            found = idx;
            break;
        }
    }
    // Prefer exact xfb; else newest valid slot (sample-held bg for nearby mid).
    if (found < 0)
    {
        for (int i = 0; i < kBgSlots; i++)
        {
            const int idx = (s_write - 1 - i + kBgSlots * 8) % kBgSlots;
            if (s_slots[idx].valid)
            {
                found = idx;
                break;
            }
        }
    }
    if (found < 0)
    {
        s_st.midFailNoBg++;
        return false;
    }
    const BgSlot& b = s_slots[found];
    if (!b.hasGx || b.fifoOff == 0)
    {
        // fifoOff==0 can be valid if player is first draw — still allow with gx.
        if (!b.hasGx)
        {
            s_st.midFailNoSeg++;
            return false;
        }
    }
    s_midSlot = found;
    if (outFifoOff) *outFifoOff = b.fifoOff;
    if (outCmdBytes) *outCmdBytes = 0;  // filled by caller from packet size
    s_playerRowMask = 0;  // rebuilt by PoseInterp overrides during replay
    return true;
}

bool A3u_ScratchBeginFromBg(uint32_t* outW, uint32_t* outH)
{
    if (s_midSlot < 0 || s_midSlot >= kBgSlots) return false;
    return Render_A3uBeginScratchFromBg(s_midSlot, outW, outH);
}

const void* A3u_GxAtPlayer()
{
    if (s_midSlot < 0 || s_midSlot >= kBgSlots) return nullptr;
    std::lock_guard lk(s_mu);
    if (!s_slots[s_midSlot].hasGx) return nullptr;
    return &s_slots[s_midSlot].gx;
}

void A3u_NoteMidResult(bool ok, uint32_t dix, float cpuMs, float gpuMs, const char* reason)
{
    std::lock_guard lk(s_mu);
    if (ok) s_st.midOk++;
    else if (reason && std::strstr(reason, "bg")) s_st.midFailNoBg++;
    else if (reason && std::strstr(reason, "seg")) s_st.midFailNoSeg++;
    else if (!ok) s_st.midFailOther++;
    s_st.playerDix = dix;
    s_st.lastMidCpuMs = cpuMs;
    s_st.lastMidGpuMs = gpuMs;
    if (AuditOn() && (s_st.midAttempt <= 8 || (s_st.midAttempt % 60) == 0))
        LOG_INFO("A3u mid {} dix={} cpu={:.2f}ms gpu={:.2f}ms | {}",
                 ok ? "OK" : "FAIL", dix, cpuMs, gpuMs, reason ? reason : "");
}

void A3u_GetStats(A3uStats& out)
{
    std::lock_guard lk(s_mu);
    out = s_st;
}

void A3u_OverlayLine(char* buf, size_t bufSize)
{
    if (!buf || !bufSize) return;
    buf[0] = 0;
    if (!EnvOn() && !AuditOn()) return;
    A3uStats s;
    A3u_GetStats(s);
    std::snprintf(buf, bufSize,
                  "A3u bg=%llu mid=%llu/%llu plyDraw=%u after=%u contig=%u gpu=%.1f",
                  (unsigned long long)s.bgCaptureOk, (unsigned long long)s.midOk,
                  (unsigned long long)s.midAttempt, s.drawsPlayer, s.drawsAfter,
                  s.playerContiguous, s.lastMidGpuMs);
}
