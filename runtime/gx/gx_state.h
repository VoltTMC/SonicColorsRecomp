#pragma once
#include <cstdint>
#include <mutex>

// Raw GX register state as written by the command stream.
struct GXState
{
    uint32_t bp[0x100];
    uint32_t cp[0x100];
    uint32_t xfmem[0x1000];     // matrices, lights (0x0000-0x0FFF)
    uint32_t xfregs[0x100];     // 0x1000-0x10FF
    uint32_t bpMaskValue = 0xFFFFFF;   // BP mask (reg 0xFE) for next write
};

extern GXState g_gx;

// Vertex attribute component layout of one vertex as decoded from VCD/VAT.
struct VertexLayout
{
    uint32_t size;
    // per-attribute descriptors
    uint8_t posMtxIdx;      // 0 none, 1 direct
    uint8_t texMtxIdx[8];
    uint8_t pos, nrm, col[2], tex[8];          // 0 none,1 direct,2 idx8,3 idx16
    uint8_t posCnt, posFmt, posShift;
    uint8_t nrmCnt, nrmFmt, nrmIndex3;
    uint8_t colCnt[2], colFmt[2];
    uint8_t texCnt[8], texFmt[8], texShift[8];
    bool byteDequant;
};

void GX_ComputeLayout(int vat, VertexLayout& l);

inline float XFFloat(uint32_t idx)
{
    union { uint32_t u; float f; } c;
    c.u = idx < 0x1000 ? g_gx.xfmem[idx] : g_gx.xfregs[idx - 0x1000];
    return c.f;
}

// CP registers
enum
{
    CP_MATINDEX_A = 0x30,
    CP_MATINDEX_B = 0x40,
    CP_VCD_LO = 0x50,
    CP_VCD_HI = 0x60,
    CP_VAT_A = 0x70,
    CP_VAT_B = 0x80,
    CP_VAT_C = 0x90,
    CP_ARRAY_BASE = 0xA0,
    CP_ARRAY_STRIDE = 0xB0,
};

// XF registers (index into xfregs)
enum
{
    XF_ERROR = 0x00,
    XF_DIAG = 0x01,
    XF_STATE0 = 0x02,
    XF_STATE1 = 0x03,
    XF_CLOCK = 0x04,
    XF_CLIPDISABLE = 0x05,
    XF_SETGPMETRIC = 0x06,
    XF_VTXSPECS = 0x08,
    XF_NUMCOLORS = 0x09,
    XF_AMBIENT0 = 0x0A,
    XF_AMBIENT1 = 0x0B,
    XF_MATERIAL0 = 0x0C,
    XF_MATERIAL1 = 0x0D,
    XF_COLOR0CNTRL = 0x0E,
    XF_COLOR1CNTRL = 0x0F,
    XF_ALPHA0CNTRL = 0x10,
    XF_ALPHA1CNTRL = 0x11,
    XF_DUALTEX = 0x12,
    XF_MATINDEX_A = 0x18,
    XF_MATINDEX_B = 0x19,
    XF_VIEWPORT = 0x1A,     // 6 floats
    XF_PROJECTION = 0x20,   // 6 floats + type
    XF_NUMTEXGENS = 0x3F,
    XF_TEXMTXINFO = 0x40,   // 8
    XF_POSTMTXINFO = 0x50,  // 8
};

// Renderer callbacks (implemented by the D3D backend)
void Render_Draw(int primitive, int vat, uint32_t count, const uint8_t* data, const VertexLayout& layout);
void Render_OnBP(uint32_t reg, uint32_t oldValue, uint32_t newValue);
void Render_OnXF(uint32_t addr, uint32_t count, bool indexed = false);
void Render_OnCP(uint32_t reg);
// True while RunCommands is nested inside CallDisplayList (GPU thread).
bool GX_InDisplayList();
void Render_EFBCopy();
void Render_LoadTLUT();
void Render_PreloadTMEM();
void Render_InvalidateTextureCache();

// A2c/A2d scratch replay isolation (default off / inactive).
void GX_SetPeSuppress(bool on);
bool GX_PeSuppressActive();
void GX_ReplayCommands(const uint8_t* data, size_t n);
// Blocks until GPU thread drains; returned lock keeps GPU idle for renderer work.
std::unique_lock<std::mutex> GX_SyncGPU();

void Render_SetReplayIsolation(bool on);
bool Render_ReplayIsolationActive();
void Render_ForceXfDirty();
void Render_Flush();  // submit pending host draw batch
void Render_ResetHostDrawState();  // flush batches; force XF upload next draw
// A3x: drop A3s/A3t Flush bind/CB cache after external D3D11 state changes
// (EFB copy unbind, XFB present blit, util draws).
void Render_InvalidateHostBindCache();

// Incoming EFB snapshot slots (GPU CopyResource; independent of live EFB).
// slot < 0 means invalid. Capture at pass entry = after post-XFB ClearEFB
// (or immediately after toXFB when the clear bit is off).
// hashContents=false: GPU CopyResource only (A3p prod). true: also every-pixel
// CPU hash for A2e/A2f MATCH diagnostics.
int Render_EfbIncomingCapture(uint32_t* outColorHash, uint32_t* outDepthHash,
                              uint32_t* outW, uint32_t* outH, bool hashContents = true);
void Render_EfbIncomingAddRef(int slot);
void Render_EfbIncomingRelease(int slot);
bool Render_EfbIncomingValid(int slot);
uint32_t Render_EfbIncomingColorHash(int slot);
uint32_t Render_EfbIncomingDepthHash(int slot);

// Scratch: seed from incoming (no clear) or clear-to-black (legacy probe).
bool Render_ScratchBeginFromIncoming(int incomingSlot, uint32_t* outW, uint32_t* outH);
// A2g: retainScratchEfbCopies keeps isolation EFB→tex map across sequential frames.
bool Render_ScratchBeginFromIncomingEx(int incomingSlot, bool retainScratchEfbCopies,
                                       uint32_t* outW, uint32_t* outH);
void Render_ScratchDropEfbCopies();
uint32_t Render_ScratchEfbCopyCount();
bool Render_ScratchHasEfbCopy(uint32_t destAddr);
uint64_t Render_ScratchEfbCopyGen(uint32_t destAddr);
uint32_t Render_HashLiveEfbCopy(uint32_t destAddr);
uint32_t Render_HashScratchEfbCopy(uint32_t destAddr);
// A2g selftest: when true under isolation, non-toXFB EFB→tex copies are no-ops
// so retained producer generations are sampled by the consumer frame.
void Render_SetScratchSkipEfbTexCopy(bool skip);
bool Render_ScratchSkipEfbTexCopy();
bool Render_ScratchBeginCleared(uint32_t* outW, uint32_t* outH);
uint32_t Render_ScratchEndHash();          // color fingerprint; ends isolation
uint32_t Render_ScratchEndDepthHash();    // depth fingerprint; call before EndHash
uint32_t Render_HashScratchColor();       // color fingerprint; keeps isolation
uint32_t Render_HashScratchDepth();       // depth fingerprint; keeps isolation
uint32_t Render_HashLiveEfb();
uint32_t Render_HashLiveDepth();
uint32_t Render_HashScratchBlack();

// A3o: copy scratch colour → mid-XFB texture, Present, count as unique mid-frame.
// Call while scratch isolation still active (scratch colour valid). Returns false if
// scratch/device unavailable. outColorHash optional fingerprint of presented image.
bool Render_PresentScratchUniqueMid(uint32_t* outColorHash);
uint64_t Render_UniqueMidPresentTotal();

// A3u: retained pre-player EFB colour+depth ring (slot 0..3).
bool Render_A3uCapturePrePlayer(int slot);
bool Render_A3uBeginScratchFromBg(int slot, uint32_t* outW, uint32_t* outH);

// A3s: mid-stage CPU timers (active when SCR_MID_PROFILE=1).
void Render_MidProfReset();
void Render_MidProfGet(uint64_t* flushNs, uint64_t* xfUploadNs, uint64_t* drawDecodeNs,
                       uint64_t* flushCount, uint64_t* drawCount, uint64_t* drawIndexed);
// Optional: CPU ns spent in FillVS/PS + GetTexture inside Flush (nullptr ignored).
void Render_MidProfGetPrep(uint64_t* flushPrepNs);

// A3t: D3D11 submit counters (SCR_D3D11_SUBMIT_PROFILE=1) + async mid GPU timestamps.
// A4a: also enabled by SCR_FRAME_PROFILE=1 for the normal (non-mid) Flush path.
struct RenderSubmitProf
{
    uint64_t mapNs = 0, mapN = 0;
    uint64_t unmapN = 0;
    uint64_t updateSubNs = 0, updateSubN = 0;
    uint64_t drawIndexedNs = 0, drawIndexedN = 0;
    uint64_t bindCalls = 0;       // IA/VS/PS/OM/RS sets that reached the context
    uint64_t bindSkipped = 0;     // suppressed by bind/CB cache
    uint64_t cbUploadN = 0;       // VS/PS constant Map uploads
    uint64_t cbSkipN = 0;         // identical CB content skipped
    uint64_t texLookupN = 0;
    uint64_t xfUploadN = 0;       // full XF UpdateSubresource count
};
void Render_SubmitProfReset();
void Render_SubmitProfGet(RenderSubmitProf* out);
// A4a: normal-path Flush aggregates (SCR_FRAME_PROFILE=1). Reset by Get-and-clear.
void Render_FrameProfGet(uint64_t* flushNs, uint64_t* xfUploadNs, uint64_t* flushPrepNs,
                         uint64_t* flushCount, uint64_t* drawIndexed, bool clear);
// Bracket mid scratch GPU work; resolve previous bracket asynchronously (DONOTFLUSH).
void Render_MidGpuTimingBegin();
void Render_MidGpuTimingEnd();
// Last successfully resolved mid GPU duration (ns); 0 if none/disjoint.
uint64_t Render_MidGpuNsLast();

struct RenderEfbCompare
{
    uint32_t width = 0, height = 0;
    uint32_t liveColorHash = 0, scratchColorHash = 0;
    uint32_t liveDepthHash = 0, scratchDepthHash = 0;
    uint32_t colorMismatch = 0;
    uint32_t depthMismatch = 0;   // ~0u => depth compare unavailable
    uint32_t maxChannelDelta = 0;
    uint64_t sumChannelDelta = 0;
    uint32_t comparedPixels = 0;
    bool colorExact = false;
    bool depthExact = false;
    bool depthCompared = false;   // true only if depth pixels were actually compared
};
// Exact CPU compare of live vs scratch color (and optionally depth). Call while both valid,
// before ScratchEndHash tears down isolation (or after with textures still allocated).
// Live sample point: pass-exit EFB (after archived cmds, before post-XFB clear).
bool Render_CompareLiveVsScratch(RenderEfbCompare& out, bool compareDepth);

// Async compare: GPU CopyResource into staging + event query (no Map on submit).
// Ticket 0 = failure. Poll until ready; skipped tickets are never reported as MATCH.
int Render_CompareBeginAsync(bool compareDepth);
bool Render_ComparePoll(int ticket, RenderEfbCompare& out, bool* stillPending);
void Render_CompareCancel(int ticket);
uint32_t Render_CompareQueueDepth();

uint8_t* Render_TmemPtr();
size_t Render_TmemSize();
