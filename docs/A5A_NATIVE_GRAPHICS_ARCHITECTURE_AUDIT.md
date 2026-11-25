# A5a — Native Graphics Architecture Audit

**Date:** 2026-11-25  
**Baseline commit:** `cd28bd0` (A4n)  
**Scope:** Document the real GX→GPU path before any direct-path prototype. Decomp tree is read-only.

## 1. Executive summary

| Layer | Nature | Role |
|---|---|---|
| ~38,506 DOL functions | **Static recomp → native x86-64** | Game + GX SDK library code |
| GX SDK (`gx.a`) | **Recompiled PPC**, not HLE | Writes CP/BP/XF/draw opcodes to the write-gather pipe |
| MMIO `0xCC008000` (GXFIFO) | **Host HLE** | `mmio.cpp` → `GPFifo_Write*` |
| Gather pipe + 32 MiB ring | **Host compatibility** | Dual-core-style CPU→GX thread transport |
| `RunCommands` / CP·BP·XF | **Host command processor** | Decodes Wii GX wire format; updates `g_gx` |
| `Render_Draw` / `Flush` | **Host** | Vertex decode → `GfxDrawBatch` |
| D3D11 / Vulkan | **Native GPU backends** | Consume `GfxDrawBatch` |

Pre-A5a there were **no** GX `PPC_OVERRIDE`s. The FIFO is how recompiled GX *talks to hardware*. It is not optional for correctness of unmodified SDK code, but individual SDK functions can be replaced with typed host commands if ordering and PI FIFO watermarks stay consistent. A5a adds opt-in overrides for SNCP8P `GXLoadPosMtxImm` / `GXLoadNrmMtxImm` only.

## 2. Exact call path (game draw → Vulkan)

```
Recompiled game / GX SDK (CPU fiber)
  → stw/sth/stb to EA in GXFIFO MMIO window (wgPipe)
      runtime/hw/mmio.cpp  Block::GXFIFO
  → GPFifo_Write8/16/32
      runtime/gx/gx_fifo.cpp
  → Gather(): copy into SPSC ring (or RAM if recording a DL)
  → [GX thread] GpuThread: ring → s_fifo → ProcessFifo → RunCommands
        0x08 LoadCPReg → g_gx.cp + Render_OnCP
        0x10 LoadXF    → Render_Flush; g_gx.xf*; Render_OnXF
        0x20.. Indexed XF from guest RAM
        0x40 CallDisplayList → nested RunCommands on PhysPtr
        0x61 LoadBP    → Render_Flush if needed; g_gx.bp; Render_OnBP / EFB
        0x80|prim|vat  → Render_Draw(prim,vat,count,bytes,layout)
  → Render_Draw (renderer.cpp): VertexLoader_Decode → s_verts/s_indices
  → Flush() / FlushVulkan(): build GfxDrawBatch (gfx_draw.h)
  → VkGx_Draw (vk_gx.cpp): XF SSBO, dyn UBO, descriptors, vkCmdDrawIndexed
  → later VkGx_PresentXfb / VkGx_CopyEfbToTexture
```

**Threads:** guest CPU fibers write the pipe; dedicated **GX** thread decodes and draws (when `g_gpuThread`); present may SyncGPU.

**D3D11 path:** same through `Flush()`; then D3D11 binds/DrawIndexed instead of `VkGx_Draw`.

## 3. Answers to critical questions

1. **Which GX ops run as recompiled game functions?**  
   GX SDK in the SNCP8P DOL under `8036****` (see decomp `config/SNCP8P/symbols.txt`). **`tools/sdk_names.txt` is RENE8P-oriented and must not be trusted for Colors addresses** (e.g. it labels `8056DE14` as `GXLoadPosMtxImm`; real SNCP8P entry is `8036EE10`).

2. **Which GX SDK functions are ordinary translated PPC?**  
   All of `gx.a` linked into the DOL — not replaced by HLE except the A5a opt-in matrix overrides.

3. **Which GX functions are `PPC_OVERRIDE`?**  
   A5a opt-in: `GXLoadPosMtxImm` @ `8036EE10`, `GXLoadNrmMtxImm` @ `8036EE90`. Otherwise OS/PAD/WPAD/music/perf/debug only.

4. **Where is the FIFO constructed?**  
   Inside recompiled GX (stores to wgPipe) → `Gather` builds the host ring / DL RAM image.

5. **Is the FIFO essential behaviour or transport?**  
   **Both.** Wire opcodes + DL bytes in RAM are part of GX semantics (especially display lists). Immediate-mode SDK→pipe encoding is largely a **transport** that a typed host API can replace per-function.

6. **Can game logic call a host-native interface without changing semantics?**  
   **Yes**, via targeted `PPC_OVERRIDE` of SDK entry points that update the same canonical `g_gx` / draw path on the GX thread **in FIFO order**, while advancing PI wptr for skipped pipe bytes. Unsupported ops keep writing the pipe.

7. **Ordered register updates?**  
   Yes — BP/XF/CP writes flush pending geometry (`Render_Flush`) when state that affects draws changes. Order vs draws and EFB copies must be preserved.

8. **Display lists?**  
   Opcode `0x40` → `CallDisplayList`; recording uses PI vs CP base divergence (`RecordingToRAM`). DLs remain FIFO byte streams in guest RAM — retain compatibility decode.

9. **Hardware-specific FIFO behaviour?**  
   32-byte gather bursts, PI wptr, CP hi/lo watermark, PE token/finish IRQs, dual-core ring backpressure — required for SDK idle/sync.

10. **Safe typed native commands?**  
    High-frequency **immediate XF matrix loads** (`GXLoadPosMtxImm` / `GXLoadNrmMtxImm`) are the best first subset: fixed layout, no `__gx` CPU shadow dependency, already validated through existing `LoadXF` → renderer.

## 4. Compatibility vs implementation choice

| Component | Verdict |
|---|---|
| CP/BP/XF register meanings | **Compatibility requirement** |
| Draw opcode vertex wire format | **Requirement** for DLs + non-overridden draws |
| Write-gather → ring → `s_fifo` copy | **Implementation** — second copy is removable |
| `GfxDrawBatch` | **Keep / extend** — already backend-independent |
| Dual GX thread | **Keep** — performance + IRQ timing |
| Encoding XF as `0x10` packets from SDK | **Replaceable** with typed enqueue |

## 5. Cost centres (what to measure)

See `SCR_A5A_PROF` / `tools/a5a_tr1_bench.sh`. Expected hotspots from A4 work:

- Vertex decode + Flush prep (mid-prof already)
- XF SSBO upload (A4n cut payload to `0x680`)
- Descriptor updates (cache still off)
- Present draw-fence + `WaitLastSubmit`
- FIFO ring copy + `RunCommands` walk (A5a target)

Do not assume FIFO decode dominates outdoor unique_xfb without `SCR_A5A_PROF` numbers.

## 6. Prototype direction (locked by this audit)

- Extend `GfxDrawBatch` / add `gfx_native.h` (no `Vk*` / D3D types in game-facing API).
- Opt-in `SCR_A5A_DIRECT=1`: `PPC_OVERRIDE` matrix loads → typed cmds ordered vs FIFO (`afterFifoW`).
- Display lists, BP/CP, draws: legacy path until proven.
- Legacy remains default reference.
