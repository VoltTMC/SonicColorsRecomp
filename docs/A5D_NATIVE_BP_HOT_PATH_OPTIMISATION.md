# A5d — Native BP hot-path migration & TEV optimisation

**Start:** `e0d36ed823d41cd4e2c48ced1e5b1f1cce9b066e` (A5c)  
**Branch:** `main`

## 1. A5c baseline

- BP ≈97% of legacy decode category time; `bpNop` ≈47% of BP writes.
- Combined sample: `directBp≈26k`, avoided ≈253 KiB, title unique XFB ≈58–60.
- Hottest regs: `0x0F`, `0x66`, TX `0x80–0x94`, SU `0x30`/`0x31`, PE `0x43`/`0x4A`/`0x4B`.

## 2–3. Register map & KSEL

See [`A5D_BP_REGISTER_COVERAGE.md`](A5D_BP_REGISTER_COVERAGE.md) and [`A5D_KSEL_AND_TEV_SHADOW.md`](A5D_KSEL_AND_TEV_SHADOW.md).

Correction: `0x80–0x94` are **texture** registers, not KSEL. KSEL is `0xF6–0xFD`.

## 4. Shadow / special

- `0x66` → Trigger; `Render_InvalidateTextureCache()` implemented.
- Shared `GX_ApplyBpWord` unchanged ownership model.
- New `__gx` offsets documented: `+0x108`/`+0x128` SU, `+0x170` KSEL0, `+0x174` IND_IMASK.

## 5. Direct adapters (SNCP8P)

| Function | Addr | Switch | Notes |
|---|---|---|---|
| `GXFlushTextureState` | `8036D430` | `SCR_A5D_DIRECT_HOT` | skip FIFO when `0x0F` shadow valid |
| `GXSetIndirectMask` | `8036D400` | HOT | IND_IMASK |
| `GXInvalidateTexAll` | `8036C930` | HOT | flush + `0x661000`/`0x661100` + flush |
| `GXSetTexCoordScale` | `8036C9C0` | HOT | SU S/T |
| `GXSetTexCoordCylWrap` | `8036CA40` | HOT | SU cyl bits |
| `GXSetTexCoordBias` | `8036A8B0` | HOT | SU bias bits |
| `GXSetTevSwapMode` | `8036D2B0` | `SCR_A5D_DIRECT_KSEL` | KSEL0 swap fields |

A5c TEV/blend/Z (`SCR_A5C_DIRECT_BP`) retained. DL recording → `PPC_ORIGINAL`.

## 6. Decoder optimisation

- Flush skip is the largest A5d win for `0x0F` traffic (A5c `bpNop` still handles legacy identical TX/PE).
- No unsafe identical-skip of `0x66`.

## 7. Indexed XF / DL cache

Unchanged: **FIFO** / **unimplemented**.

## 8. Tests / benches

- `scr_a5d_bp_hot_test`
- `tools/a5d_tr1_bench.sh` — `legacy|a5c|hot|ksel|combined|bp_profile|compare`
- Logs: `out/a5d_*`

### 90s TR1 explore samples (RTX 3080, validation off)

| Config | flushSkip | inval | directBp | avoided | fallback | title unique_xfb | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| legacy | 0 | 0 | 0 | 0 | 0 | ~56 | deepest FIFO sample |
| HOT only | ~9.2k | ~4.6k | ~9.2k | ~90 KiB | 0 | ~59 | shorter fifo (load flake) |
| combined A5a+A5c+A5d | ~12.3k | ~6.1k | ~29.3k | ~287 KiB | 0 | ~52 | + XF/TEV/blend; `ksel=0` in title |

Outdoor mid-band not obtained in these windows. Do not claim FPS from unequal FIFO depths alone. Flush-skip is the clearest A5d CPU-path win for `0x0F`.

## 9. Env

| Var | Default | Meaning |
|---|---|---|
| `SCR_A5D_DIRECT_HOT` | off | IND/SU/TX_INVAL adapters |
| `SCR_A5D_DIRECT_KSEL` | off | TevSwapMode |
| `SCR_A5C_DIRECT_BP` | off | A5c TEV/blend/Z |
| `SCR_A5A_DIRECT` | off | XF imm |
| `SCR_A5C_BP_PROFILE` | off | per-reg profile |

## 10. Deferred / status

- ~~Full `GXLoadTexObj` direct path~~ → **A5e** (`SCR_A5E_DIRECT_TEXOBJ`)
- Dirty-state KColorSel flush
- Indexed XF, DL cache
- AMD/RADV **deferred / unverified**
- Native Windows Vulkan **unverified**
- D3D11 default
