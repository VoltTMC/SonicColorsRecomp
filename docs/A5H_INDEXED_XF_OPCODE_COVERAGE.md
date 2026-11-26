# A5h — Indexed XF Opcode Coverage

## Families

| Opcode | CP array | Typical payload | SNCP8P SDK writer | A5 adapter |
|---|---:|---|---|---|
| `0x20` | 12 | 12 floats (pos mtx) | `GXLoadPosMtxIndx@8036EE60` (`fn_8036EE60`) | A5g `SCR_A5G_DIRECT_INDEXED_XF` |
| `0x28` | 13 | 9 floats (nrm 3×3) | `GXLoadNrmMtxIndx3x3@8036EEF0` (`fn_8036EEF0`) | A5g `SCR_A5G_DIRECT_INDEXED_XF` |
| `0x30` | 14 | tex mtx (indexed) | **none in SNCP8P DOL/SDK** | **rejected** — FIFO only |
| `0x38` | 15 | light object (indexed) | **none in SNCP8P DOL/SDK** | **rejected** — FIFO only |

Array slot mapping from FIFO decode: `12 + ((op >> 3) & 3)`.

## Audit method

- Read-only decomp `/home/volt/Decomp/sonic2010-sncp8p`.
- Confirmed `fn_8036EE60` / `fn_8036EEF0` write FIFO opcodes `0x20` / `0x28` to `0xCC008000`.
- No `GXLoadTexMtxIndx` / `GXLoadLightObjIndx` (or equivalent) writers that emit `0x30` / `0x38` in the GX SDK region.
- Recomp `game_addrs.inc` registers only Pos/Nrm Indx for indexed XF.
- Runtime profiling (TR1 explore, A5g on): `fifoIdxOp38=0`; **`fifoIdxOp30` is non-zero and DL-resident** (≈ equal to `0x20`/`0x28` in sampled DLs). So texture-matrix indexed loads exist inside display lists even though there is no SNCP8P SDK `GXLoadTexMtxIndx` writer to override. They remain FIFO-decoded; A5h playback opts apply to them.

## `SCR_A5H_DIRECT_INDEXED_XF_EXT`

- Default **off**.
- Reserved for additional SDK adapters.
- **No adapters implemented** — unused/rare opcodes do not justify migration.
- Enabling the switch only activates A5h profiling labels (same as other A5* PROF paths).

## Shared apply

All paths (FIFO decode, A5g typed `XfIdx`, future EXT if ever justified) call `GX_ApplyIndexedXf` → canonical `g_gx.xfmem` + `Render_OnXF(..., indexed=true)`.

## Rejected candidates

| Candidate | Reason |
|---|---|
| Direct `0x30` adapter | No SDK entry point; no verified calling convention / usage in SNCP8P |
| Direct `0x38` adapter | Same |
| Speculative light/tex Indx HLE | Would invent semantics; prefer documented FIFO retention |

A5g Pos/Nrm behaviour and default remain unchanged.
