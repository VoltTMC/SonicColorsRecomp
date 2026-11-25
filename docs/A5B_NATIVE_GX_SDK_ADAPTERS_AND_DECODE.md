# A5b — Native GX SDK Adapters & FIFO Decode Optimisation

**Status (2026-11-25):** Decode profiled internally; `GXLoadTexMtxImm@8036EF50` added to the A5a direct set; FIFO decoder BP-nop + NOP coalesce + honest top/nested timing. Legacy FIFO remains default. AMD/RADV **deferred**. Native Windows Vulkan **unverified**.

**Baseline:** A5a `b87768c`.

## 1. A5a starting architecture

Recompiled GX SDK → GXFIFO MMIO → Gather/ring → `RunCommands` → `GfxDrawBatch` → D3D11/Vulkan.  
Direct (opt-in): `GXLoadPosMtxImm@8036EE10`, `GXLoadNrmMtxImm@8036EE90` via `gfx_native` typed XF + `afterFifoW`.

## 2–3. Decode profile (self vs inclusive)

See [`A5B_GX_DECODE_PROFILE.md`](A5B_GX_DECODE_PROFILE.md).

Summary: **BP ≈ 76%** of handler self-time; indexed XF ≈ 17%; imm XF ≈ 4%; `Render_Draw` ≈ 1%. Top-level decode ~42s vs nested ~8s on a TR1 sample — A5a’s ~47s was inclusive and double-counted nested DLs.

## 4. SNCP8P SDK inventory (exact addresses)

| Symbol | Address | Role | Direct? |
|---|---|---|---|
| `GXLoadPosMtxImm` | `8036EE10` | XF pos 12f | A5a |
| `GXLoadNrmMtxImm` | `8036EE90` | XF nrm 9f | A5a |
| `GXLoadTexMtxImm` | `8036EF50` | XF tex 8/12f | **A5b** |
| BP writers (`8036D500`–`8036E080`, …) | various | TEV/blend/Z | **A5c** `SCR_A5C_DIRECT_BP` |
| Indexed XF | opcodes `0x20–0x38` | array loads | Keep FIFO (A5c deferred) |

Do **not** use RENE8P `tools/sdk_names.txt` `8056****` labels for Colors.

## 5–6. Direct adapters

`SCR_A5A_DIRECT=1` (unchanged env name):

1. Publish gather; enqueue typed XF (`afterFifoW`).
2. `GX_FifoSkipPipeBytes(5+4*count)`.
3. GX thread `GfxNative_DrainReady` applies into `g_gx` + `Render_OnXF`.
4. `RecordingToRAM` → `PPC_ORIGINAL` (`dlRec` counter).

Tex mapping (DOL-verified): `id<0x40 → id*4`, else `(id-0x40)*4+0x500`; `type==1 → 8` floats else `12`.

## 7. Mixed ordering

Single `g_gx`; typed cmds ordered by `afterFifoW` vs FIFO watermark. No second state machine.

## 8–9. Decoder optimisations

- **Top-level vs nested** decode accounting (fixes double-count).
- **NOP coalesce** in `RunCommands`.
- **BP nop:** identical non-special BP (`nv==old`, not PE/EFB/TLUT/TEV-colour) skips flush + `Render_OnBP`.
- No DL predecode cache (deferred — invalidation risk).

## 10. Backends

D3D11 and Vulkan still consume `GfxDrawBatch`. Direct path only changes XF fill on the GX thread.

## 11–15. Results (RTX 3080 TR1)

| Run | topDecodeMs | bp / bpNop | directXf (pos/nrm/tex) | avoided | mid unique_xfb notes |
|---|---:|---:|---|---:|---|
| Decode/legacy-class | ~42–44s | ~1.45–1.49M / ~0.51M | 0 | 0 | mid ~4 after outdoor sample; late watchdog stalls observed |
| Direct (short) | (early exit) | — | 1407 (1404/1/2), fallback=0 | ~75 KiB | Title/early; one 90s run watchdog-hung in load — intermittent |

Honest claims:

- Measurable **BP nop** volume and corrected decode attribution.
- TexMtxImm adapter works (`tex>0`, `fallback=0`) when DIRECT=1.
- **No** claim that outdoor unique_xfb improved vs A5a mid-band 12.4 from A5b alone on these runs.
- FIFO remains required for DLs, BP/CP, draws, indexed XF.

## 16. Validation / regressions

- Unit: `scr_a5b_texmtx_addr_test`, topo/cull/fog/efb/shader_cache OK.
- Vulkan validation **not** forced for PROF benches.
- Linux clang + MinGW builds OK.

## 17–19. Remaining work / deferred

- Direct BP/TEV/blend adapters with `__gx` parity → **A5c** (`SCR_A5C_DIRECT_BP`).
- Indexed XF typing → still deferred (A5c audit).
- Safe DL metadata cache → feasibility only in A5c (unimplemented).
- AMD/RADV **deferred, unverified**.
- Native Windows Vulkan **unverified**.

See [`A5C_NATIVE_BP_SHADOW_AND_SDK_ADAPTERS.md`](A5C_NATIVE_BP_SHADOW_AND_SDK_ADAPTERS.md).
