#pragma once
// A5a: backend-independent native graphics front-end surface.
// Game-facing code and PPC_OVERRIDE adapters must not see Vk* or D3D11 types.
// Draw submission still flows through GfxDrawBatch → gfx_backend (D3D11/Vulkan).
#include "gfx_draw.h"

#include <cstdint>

// Opt-in: SCR_A5A_DIRECT=1 routes selected GX SDK calls through typed GX-thread
// commands instead of write-gather FIFO bytes. Default off (legacy reference).
bool GfxNative_DirectEnabled();

// Profiling: SCR_A5A_PROF=1 (or SCR_A5A_DIRECT) accumulates host counters.
bool GfxNative_ProfEnabled();

struct GfxNativeStats
{
    uint64_t fifoBytesIn = 0;       // bytes entering Gather (non-DL)
    uint64_t fifoBytesDecoded = 0;  // bytes consumed by RunCommands
    uint64_t cmdsDecoded = 0;       // complete GX opcodes parsed
    uint64_t drawOps = 0;           // draw packets (0x80+)
    uint64_t dlCalls = 0;
    uint64_t xfLoads = 0;           // FIFO-path XF (0x10 / indexed)
    uint64_t bpLoads = 0;
    uint64_t cpLoads = 0;
    uint64_t directXf = 0;          // typed XF via A5a overrides
    uint64_t directFallback = 0;    // override fell back to PPC_ORIGINAL
    uint64_t fifoBytesAvoided = 0;  // pipe bytes not written due to direct XF
    uint64_t gatherNs = 0;
    uint64_t decodeNs = 0;
    uint64_t typedApplyNs = 0;
};

const GfxNativeStats& GfxNative_Stats();
void GfxNative_StatsReset();
void GfxNative_LogStats(const char* tag);

// Ordered typed XF load (GX thread). afterFifoW is captured inside.
// words: count big-endian GX wire words (as in FIFO), or host-endian floats bitcast.
bool GfxNative_EnqueueXfImm(uint32_t addr, uint32_t count, const uint32_t* words, bool wordsAreBE);

// Drain typed commands whose FIFO watermark has been reached (GX thread only).
void GfxNative_DrainReady(size_t fifoBytesConsumedTotal);

// Called from Gather / RunCommands when profiling.
void GfxNative_NoteGather(uint64_t bytes, uint64_t ns);
void GfxNative_NoteDecode(uint64_t bytes, uint64_t cmds, uint64_t ns);
void GfxNative_NoteOpcode(uint8_t op);
