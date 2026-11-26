#pragma once
// A5a–A5c: backend-independent native graphics front-end surface.
// Game-facing code and PPC_OVERRIDE adapters must not see Vk* or D3D11 types.
// Draw submission still flows through GfxDrawBatch → gfx_backend (D3D11/Vulkan).
#include "gfx_draw.h"

#include <cstdint>

// Opt-in: SCR_A5A_DIRECT=1 routes selected GX SDK XF calls through typed GX-thread
// commands instead of write-gather FIFO bytes. Default off (legacy reference).
bool GfxNative_DirectEnabled();

// Opt-in: SCR_A5C_DIRECT_BP=1 routes selected BP-backed GX SDK calls (TEV/blend/Z)
// through typed BP commands. Default off. Independent of SCR_A5A_DIRECT.
bool GfxNative_DirectBpEnabled();

// A5d opt-in: SCR_A5D_DIRECT_HOT=1 — IND_IMASK flush/mask, SU 0x30/0x31 scale, TX_INVAL 0x66.
// SCR_A5D_DIRECT_KSEL=1 — GXSetTevSwapMode (KSEL 0xF6 swap fields). Default off.
bool GfxNative_DirectHotEnabled();
bool GfxNative_DirectKselEnabled();

// A5e opt-in: SCR_A5E_DIRECT_TEXOBJ=1 — GXLoadTexObj / PreLoaded / GXLoadTlut → typed BP.
// Does not decode/upload textures; only replaces FIFO TX unit BP writes. Default off.
bool GfxNative_DirectTexObjEnabled();

// A5g opt-in: SCR_A5G_DIRECT_INDEXED_XF=1 — GXLoadPosMtxIndx / GXLoadNrmMtxIndx3x3 → typed
// indexed XF (same GX_ApplyIndexedXf as FIFO 0x20/0x28). Default off. DL recording → FIFO.
bool GfxNative_DirectIndexedXfEnabled();

// A5h: SCR_A5H_DIRECT_INDEXED_XF_EXT=1 — reserved for additional SDK idx adapters (0x30/0x38).
// Currently no justified adapters (opcodes unused by SNCP8P SDK); switch still enables
// A5h profiling labels. Default off. Does not change A5g behaviour.
bool GfxNative_DirectIndexedXfExtEnabled();

// A5h: SCR_A5H_INDEXED_XF_PLAYBACK=1 — skip empty Render_Flush inside GX_ApplyIndexedXf
// and use a specialised consecutive indexed-XF decode path (FIFO/DL). Default off.
bool GfxNative_IndexedXfPlaybackOptEnabled();

// A5i: SCR_A5I_INDEXED_XF_APPLY=1 — bulk BE→host endian convert + skip Render_OnXF /
// MarkXfDirty when the indexed payload is byte-identical to current xfmem. Default off.
// Does not change A5g/A5h switch behaviour. A3u/A3h hooks still run.
bool GfxNative_IndexedXfApplyOptEnabled();

// A5j: SCR_A5J_INDEXED_XF_DISPATCH=1 — specialised consecutive indexed-XF FIFO/DL decode
// with batched opcode notes (atomics). Does not change A5g/A5h/A5i semantics. Default off.
bool GfxNative_IndexedXfDispatchOptEnabled();

// A5k: SCR_A5K_INDEXED_XF_FLUSH_OPT=1 — optimise non-empty Render_Flush used by indexed XF
// (Vulkan: skip redundant GXTex_Decode when the tex-cache key already hits). Does not skip
// required geometry drains. Default off; A5j Flush path remains the fallback.
bool GfxNative_IndexedXfFlushOptEnabled();

// Profiling: SCR_A5A_PROF=1 (or SCR_A5A_DIRECT / SCR_A5C_DIRECT_BP / A5d) accumulates host counters.
// SCR_A5B_PROF=1 enables fine-grained RunCommands category self-times.
// SCR_A5C_BP_PROFILE=1 enables per-BP-register write/nop/special counters.
bool GfxNative_ProfEnabled();
bool GfxNative_DecodeProfEnabled();

enum class GfxDecodeCat : uint8_t
{
    Cp = 0,
    XfImm,
    XfIdx,
    Bp,
    DlCall,   // CallDisplayList wall (includes nested RunCommands)
    Draw,     // Render_Draw wall
    Dispatch, // opcode decode / layout / bookkeeping self (approx)
    Count
};

struct GfxNativeStats
{
    uint64_t fifoBytesIn = 0;         // bytes entering Gather (non-DL)
    uint64_t fifoBytesDecoded = 0;    // top-level FIFO bytes only (no nested DL)
    uint64_t dlBytesDecoded = 0;      // bytes consumed inside display lists
    uint64_t cmdsDecoded = 0;         // top-level + nested complete opcodes
    uint64_t drawOps = 0;
    uint64_t dlCalls = 0;
    uint64_t xfLoads = 0;             // FIFO-path XF (0x10 / indexed)
    uint64_t bpLoads = 0;
    uint64_t bpNop = 0;               // identical ordinary BP skipped (A5b/A5c; requires valid shadow)
    uint64_t cpLoads = 0;
    uint64_t directXf = 0;            // typed XF via overrides (pos/nrm/tex)
    uint64_t directPos = 0;
    uint64_t directNrm = 0;
    uint64_t directTex = 0;
    uint64_t directBp = 0;            // typed BP via A5c overrides
    uint64_t directBpBlend = 0;
    uint64_t directBpZ = 0;
    uint64_t directBpTev = 0;
    uint64_t directBpPeBit = 0;       // color/alpha update
    uint64_t directBpFlush = 0;       // A5d: FlushTextureState / SetIndirectMask
    uint64_t directBpFlushSkip = 0;   // A5d: flush skipped (shadow valid, identical)
    uint64_t directBpScale = 0;       // A5d: SU_SSIZE/TSIZE (0x30/0x31)
    uint64_t directBpInval = 0;       // A5d: TX_INVALTAGS 0x66
    uint64_t directBpKsel = 0;        // A5d: KSEL / swap mode
    uint64_t directTexObj = 0;        // A5e: LoadTexObj / PreLoaded calls
    uint64_t directTexObjBp = 0;      // A5e: BP words from texobj path
    uint64_t directTlut = 0;          // A5e: GXLoadTlut calls
    uint64_t directIdxXf = 0;         // A5g: typed indexed XF ops
    uint64_t directIdxPos = 0;        // A5g: PosMtxIndx
    uint64_t directIdxNrm = 0;        // A5g: NrmMtxIndx3x3
    uint64_t fifoIdxOp20 = 0;         // A5h: FIFO/DL decode counts by opcode
    uint64_t fifoIdxOp28 = 0;
    uint64_t fifoIdxOp30 = 0;
    uint64_t fifoIdxOp38 = 0;
    uint64_t fifoIdxTop = 0;          // A5h: indexed XF outside display lists
    uint64_t fifoIdxDl = 0;           // A5h: indexed XF during DL playback
    uint64_t idxXfFlushSkip = 0;      // A5h: empty Flush skipped in ApplyIndexedXf
    uint64_t idxXfFlushCall = 0;      // A5h: Flush actually called from ApplyIndexedXf
    uint64_t idxXfApply = 0;          // A5i: ApplyIndexedXf invocations (all paths)
    uint64_t idxXfGuestBytes = 0;     // A5i: guest payload bytes read
    uint64_t idxXfIdentical = 0;      // A5i: payload matched xfmem (dirty skipped when APPLY=1)
    uint64_t idxXfChanged = 0;        // A5i: payload differed → dirty marked
    uint64_t idxXfApplyNs = 0;        // A5i: self-time inside ApplyIndexedXf (excl. Flush)
    uint64_t idxXfGuestNs = 0;        // A5i: guest read + endian convert + xfmem write
    uint64_t idxXfOnXfNs = 0;         // A5i: Render_OnXF wall from Apply
    uint64_t idxXfFlushNs = 0;        // A5j: Render_Flush wall inside Apply (excl. empty skip)
    uint64_t idxXfDispatchNs = 0;     // A5j: exclusive idx case wall minus Apply+Flush (FIFO/DL)
    uint64_t idxXfDispatchRuns = 0;   // A5j: consecutive idx decode runs
    uint64_t idxXfDispatchCmds = 0;   // A5j: cmds consumed by specialised dispatch
    uint64_t idxXfNoteNs = 0;         // A5j: time spent in idx Note* helpers (overhead)
    // A5k: Render_Flush exclusive breakdown (CPU; no GPU wait attribution).
    uint64_t flushIdxOrigin = 0;      // Flush entered from ApplyIndexedXf
    uint64_t flushOtherOrigin = 0;    // Flush from other GX / draw / frame paths
    uint64_t flushEmpty = 0;          // early-out (no pending indices / not ready)
    uint64_t flushNonEmpty = 0;       // drained geometry
    uint64_t flushBatches = 0;        // non-empty flush count (= drained host batches)
    uint64_t flushVerts = 0;          // pending verts at non-empty flush
    uint64_t flushIndices = 0;        // pending indices at non-empty flush
    uint64_t flushMaxVerts = 0;       // max pending verts observed
    uint64_t flushMaxIndices = 0;     // max pending indices observed
    uint64_t flushTexDecodeNs = 0;    // CPU GXTex_Decode inside FlushVulkan
    uint64_t flushTexDecodeSkip = 0;  // A5k: RAM tex maps that skipped decode (cache hit)
    uint64_t flushTexDecodeMaps = 0;  // RAM tex maps that decoded
    uint64_t flushPrepNs = 0;         // D3D FillVS/PS + GetTexture / VK batch prep excl decode
    uint64_t flushSubmitNs = 0;       // D3D DrawIndexed / VkGx_Draw wall
    uint64_t flushBodyNs = 0;         // exclusive non-empty Flush body (both backends)
    uint64_t flushVk = 0;             // non-empty Vulkan flushes
    uint64_t flushD3d = 0;            // non-empty D3D11 flushes
    uint64_t directFallback = 0;
    uint64_t directDlRecord = 0;      // fell back because RecordingToRAM
    uint64_t fifoBytesAvoided = 0;
    uint64_t gatherNs = 0;
    // decodeNs: TOP-LEVEL RunCommands wall only (excludes nested double-count).
    uint64_t decodeNs = 0;
    // decodeNestedNs: sum of in-DL RunCommands walls (subset of DlCall inclusive).
    uint64_t decodeNestedNs = 0;
    uint64_t typedApplyNs = 0;
    uint64_t catNs[(size_t)GfxDecodeCat::Count]{};
    uint64_t catCount[(size_t)GfxDecodeCat::Count]{};
};

const GfxNativeStats& GfxNative_Stats();
void GfxNative_StatsReset();
void GfxNative_LogStats(const char* tag);

// Ordered typed XF load (GX thread). afterFifoW is captured inside.
bool GfxNative_EnqueueXfImm(uint32_t addr, uint32_t count, const uint32_t* words, bool wordsAreBE);

// Ordered typed BP write (full word = reg<<24 | val24). afterFifoW captured inside.
bool GfxNative_EnqueueBp(uint32_t word);

void GfxNative_DrainReady(size_t fifoBytesConsumedTotal);

void GfxNative_NoteGather(uint64_t bytes, uint64_t ns);
// topLevel: false when RunCommands is executing a display list body.
void GfxNative_NoteDecode(uint64_t bytes, uint64_t cmds, uint64_t ns, bool topLevel);
void GfxNative_NoteOpcode(uint8_t op);
void GfxNative_NoteDecodeCat(GfxDecodeCat cat, uint64_t ns);
void GfxNative_NoteBpNop();
// A5h: FIFO/DL indexed-XF opcode accounting (op in {0x20,0x28,0x30,0x38}).
void GfxNative_NoteIndexedXfFifo(uint8_t op, bool inDisplayList);
void GfxNative_NoteIndexedXfFlush(bool skipped);
// A5i: guest-read / apply accounting (bytes = size*4; identical ⇒ dirty skip when APPLY=1).
void GfxNative_NoteIndexedXfApply(uint32_t guestBytes, bool identical, bool calledOnXf, uint64_t guestNs,
                                  uint64_t onXfNs, uint64_t applyNs, uint64_t flushNs = 0);
// A5j: batch FIFO/opcode notes for a consecutive idx run (one atomic update set).
void GfxNative_NoteIndexedXfFifoBatch(uint64_t op20, uint64_t op28, uint64_t op30, uint64_t op38,
                                      bool inDisplayList, uint64_t cmdCount);
void GfxNative_NoteIndexedXfDispatch(uint64_t dispatchExclusiveNs, uint64_t cmds, uint64_t noteNs);
// A5k: Flush origin + exclusive breakdown (called from renderer; atomics).
void GfxNative_NoteFlushBegin(bool fromIndexedXf, bool empty, uint32_t vertCount, uint32_t indexCount);
void GfxNative_NoteFlushDetail(uint64_t bodyNs, uint64_t texDecodeNs, uint64_t prepNs, uint64_t submitNs,
                               uint64_t texDecodeSkip, uint64_t texDecodeMaps, bool vulkan);
