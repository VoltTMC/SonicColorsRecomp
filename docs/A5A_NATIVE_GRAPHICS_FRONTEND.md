# A5a — Native GX Front-End & Direct GPU Submission

**Status (2026-11-25):** Architecture audited; backend-independent `gfx_native` surface; opt-in direct path for SNCP8P `GXLoadPosMtxImm` @ `8036EE10` / `GXLoadNrmMtxImm` @ `8036EE90` bypasses write-gather XF packet construction while preserving FIFO ordering on the GX thread. Legacy FIFO remains the default reference. AMD/RADV **deferred**. Native Windows Vulkan **unverified**.

**Baseline:** A4n `cd28bd0`.

## 1. Existing architecture

See [`A5A_NATIVE_GRAPHICS_ARCHITECTURE_AUDIT.md`](A5A_NATIVE_GRAPHICS_ARCHITECTURE_AUDIT.md).

Layers:

1. **Native CPU recompilation** — game + GX SDK as x86-64.
2. **GX compatibility** — write-gather → ring → `RunCommands` → `g_gx` / `Render_Draw`.
3. **Native GPU** — `GfxDrawBatch` → D3D11 or Vulkan (`vkCmdDrawIndexed`).

Vulkan draws were already native GPU execution. A5a removes a slice of **compatibility transport** (immediate XF encode/decode), not the whole GX model.

## 2. Exact GX/FIFO call path

Documented in the audit (§2). Before A5a there were no GX `PPC_OVERRIDE`s.

**Address note:** `tools/sdk_names.txt` is RENE8P/signature noise for Colors. SNCP8P GX lives under `8036****` (decomp `symbols.txt` + DOL evidence). Wrong `8056****` hooks never fired (`directXf=0`).

## 3. Cost breakdown (RTX 3080 TR1, ~60s, `SCR_A5A_PROF=1`)

| Counter | Meaning |
|---|---|
| `fifoIn` | Bytes entering `Gather` |
| `decoded` / `cmds` | Bytes/opcodes consumed by `RunCommands` |
| `draws` / `dl` / `xf` / `bp` / `cp` | Opcode class counts (FIFO path) |
| `directXf` | Typed XF applies from overrides |
| `avoided` | Pipe bytes not written for those XF loads |
| `gatherMs` / `decodeMs` / `typedMs` | Host time (cumulative) |

End-of-run sample (`tools/a5a_tr1_bench.sh`):

| Path | fifoIn | decodeMs | directXf | avoided | mid unique_xfb mean |
|---|---|---|---|---|---|
| Legacy | ~95.9M | ~47.8s | 0 | 0 | ~10.8 |
| Direct | ~96.5M | ~46.7s | ~14.9k | ~787 KiB | ~12.4 |

`fallback=0` for the whole direct run. Most XF traffic remains FIFO (display lists + other loaders) — immediate `GXLoad*MtxImm` is a minority of `xf` counts. Do not treat `avoided` alone as a throughput win; this sample shows **no mid unique_xfb regression** and a small decode-time dip under noisy scene timing.

## 4. Operations suitable for direct conversion

| Op | Status |
|---|---|
| `GXLoadPosMtxImm` @ `8036EE10` | **Direct** (opt-in) |
| `GXLoadNrmMtxImm` @ `8036EE90` | **Direct** (opt-in) |
| `GXLoadTexMtxImm` @ `8036EF50` | **Direct** (opt-in, A5b) |
| Immediate BP setters | Candidate (`__gx` shadow care) |
| `GXBegin` / draws | Later — wire + DL coupling |
| `GXCallDisplayList` | Retain FIFO decode of RAM image |

## 5. Must retain compatibility

Display-list recording/playback, CP VCD/VAT, BP EFB copies, PE IRQ, gather/PI wptr, indexed XF, all non-overridden SDK paths.

## 6. Native graphics interface

- `runtime/gx/gfx_draw.h` — `GfxDrawBatch` (unchanged backend contract).
- `runtime/gx/gfx_native.h` — profiling + typed XF enqueue (no `Vk*` / D3D types).

## 7. Direct-path prototype

`SCR_A5A_DIRECT=1`:

1. `PPC_OVERRIDE(GXLoadPosMtxImm|GXLoadNrmMtxImm|GXLoadTexMtxImm)` at proven SNCP8P starts.
2. Publish pending gather; enqueue typed XF with `afterFifoW = s_localW`.
3. `GX_FifoSkipPipeBytes` for PI gather accounting (`1+4+4*count`).
4. GX thread `GfxNative_DrainReady` applies XF in order with FIFO decode.
5. DL recording → `PPC_ORIGINAL` (`directFallback++`).

Default: unset/`0` → originals (legacy reference).

## 8. Mixed-path ordering

Typed cmds carry `afterFifoW`. Drain when ring watermark reached, alongside `ProcessFifo`. One canonical `g_gx`.

## 9. D3D11 & Vulkan

Both still consume `GfxDrawBatch` from `Flush` / `FlushVulkan`. Direct path only changes how XF memory is filled on the GX thread.

## 10–13. Parity / performance

- Same Vulkan TR1 explore script; no watchdog; `fallback=0`.
- Immediate matrix loads no longer encode `0x10` packets when DIRECT=1.
- Remaining decode cost dominated by DL replay + BP/CP/draws — expected.
- A4j/A4k metrics unchanged in definition; full four-way BMP matrix not re-run this milestone (opt-in path; legacy default).

## 14. Remaining migration

- More SDK adapters (blend/Z/TEV) with shadow-state discipline.
- Optional draw-path typing; reduce ring→`s_fifo` copy.
- Keep A4n desc cache / async present **off** by default.

## 15–16. Deferred

AMD/RADV **deferred, unverified**. Native Windows Vulkan **unverified**.
