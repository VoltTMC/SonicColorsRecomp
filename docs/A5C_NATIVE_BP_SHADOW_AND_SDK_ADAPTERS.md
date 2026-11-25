# A5c — Native BP shadow & SDK adapters

**Start:** `552f4ceeb3a200829b271c3789be84f34efbfd90` (A5b)  
**Branch:** `main`

## 1. A5b baseline

Corrected decode profile (no nested double-count):

| Metric | A5b |
|---|---:|
| Top-level decode | ~42 s |
| Nested decode | ~8 s |
| BP self-time share | ~76% |
| Indexed XF | ~17% |
| Imm XF | ~4% |
| Draw | ~1% |
| `bpNop` | ~0.5M / run |
| Early DIRECT XF sample | ~75 KiB / ~1407 ops |

## 2. BP register / special classification

See [`A5C_BP_SHADOW_DESIGN.md`](A5C_BP_SHADOW_DESIGN.md).

Special/trigger (never skip identical): `0x45`, `0x47`, `0x48`, `0x52`, `0x65`, `0xE0–0xE7`.

Ordinary PE/TEV targets for adapters: `0x40` (ZMODE), `0x41` (CMODE0), `0xC0+` / `0xC1+` (TEV combine).

Profile: `SCR_A5C_BP_PROFILE=1` → `A5c-bpprof-*` top registers + class counters. `bpNop` still means identical ordinary skipped with valid shadow.

### Measured top write-frequency registers (title/menu TR1 explore, RTX 3080)

From `out/a5c_bp_profile/` / legacy samples (ordered by writes; class Ordinary unless noted):

| Reg | Typical role | Notes |
|---|---|---|
| `0x0F` | IND_IMASK / related | almost all nops |
| `0x66` | copy-related | **always changes** (no nop) |
| `0x8C`/`0x84` | TEV/KSEL family | near-100% nops |
| `0x90`/`0x88`/`0x80`/`0x94` | TEV/KSEL family | mixed |
| `0x30`/`0x31` | SU_SSIZE/TSIZE | high change rate |
| `0x43` | PE control / ZCompLoc | mostly nops |
| `0x4A`/`0x4B` | PE/blend-adjacent | mixed |

TEV combine (`0xC0+`) and CMODE0/ZMODE appear lower in the frequency list but are the verified SDK surface for A5c adapters. Future milestones should target `0x30`/`0x31` and KSEL/`0x66` paths once SNCP8P setters are mapped.

## 3. `__gx` layout (SNCP8P)

- Object: `lbl_807FA960`, size `0x600`
- Pointer: `__GXData` @ `808FC248` → `807FA960`
- Offsets: dirty `+0x2`, tev colour `+0x180`, tev alpha `+0x1C0`, CMODE0 `+0x220`, ZMODE `+0x228`

## 4. Shadow architecture

`gx_shadow.h` / `gx_shadow.cpp`: validity bits, generation, classification, shared `GX_ApplyBpWord` used by FIFO and typed direct drain. Invalidation on `GX_Init`.

## 5. Direct BP SDK adapters (verified SNCP8P)

| Function | Address | Guest field | BP |
|---|---|---|---|
| `GXSetTevColorIn` | `8036D500` | `+0x180+4*st` | colour env |
| `GXSetTevAlphaIn` | `8036D540` | `+0x1C0+4*st` | alpha env |
| `GXSetTevColorOp` | `8036D580` | `+0x180+4*st` | colour env |
| `GXSetTevAlphaOp` | `8036D5E0` | `+0x1C0+4*st` | alpha env |
| `GXSetBlendMode` | `8036DFD0` | `+0x220` | `0x41` |
| `GXSetColorUpdate` | `8036E020` | `+0x220` | `0x41` |
| `GXSetAlphaUpdate` | `8036E050` | `+0x220` | `0x41` |
| `GXSetZMode` | `8036E080` | `+0x228` | `0x40` |

Opt-in: **`SCR_A5C_DIRECT_BP=1`** (default off). Independent of `SCR_A5A_DIRECT`.

Behaviour: update guest `__gx` on success; enqueue typed BP (`0x61`+word = 5 bytes avoided); `afterFifoW` ordering; `PPC_ORIGINAL` while `RecordingToRAM`.

## 6. Display-list recording / playback

Recording → original PPC (FIFO encode into list). Playback → legacy BP decode / shadow apply. Direct path never applies at record time.

## 7. Indexed XF investigation

Indexed XF ≈17% handler self-time. Path: opcode `0x20/0x28/0x30/0x38` → guest index arrays → XF mem + versioned SSBO ring.

**Decision: leave on FIFO.** Not equivalent to immediate matrix loads (index source, range, matrix type, palette interactions with A3h). Unsafe to bypass without a dedicated audit+test milestone. No `SCR_A5C_DIRECT_INDEXED_XF` shipped.

## 8. Display-list cache

See [`A5C_DISPLAY_LIST_CACHE_AUDIT.md`](A5C_DISPLAY_LIST_CACHE_AUDIT.md). **Unimplemented** in A5c.

## 9. Tests / benches

- `scr_a5c_bp_shadow_test` — class, skip rules, Z/blend/TEV pack parity
- Existing A5b/A4 regressions remain
- `tools/a5c_tr1_bench.sh` — `legacy|a5a|bp|combined|bp_profile|compare`
- Logs: `out/a5c_{legacy,direct_xf,direct_bp,direct,bp_profile,compare}/`

### 90s TR1 explore samples (RTX 3080, validation off)

| Config | directBp | directXf | avoided | bpNop | title unique_xfb | notes |
|---|---:|---:|---:|---:|---:|---|
| legacy | 0 | 0 | 0 | ~139k | ~60 | BP cat ~4.4s in last present |
| `DIRECT_BP` only | ~4.1k | 0 | ~20 KiB | — | (load flake) | `fallback=0`; shorter fifo sample |
| combined XF+BP | ~26k | ~2.4k | ~253 KiB | ~150k | ~58 | `fallback=0` `dlRec=0`; tev/blend/z counted |

Outdoor mid-band unique_xfb not obtained in these 90s windows (load-watchdog class, also seen in A5b). Do not claim FPS win from BP command counts alone.

## 10. Env vars

| Var | Default | Meaning |
|---|---|---|
| `SCR_A5A_DIRECT` | off | Pos/Nrm/TexMtxImm typed XF |
| `SCR_A5C_DIRECT_BP` | off | TEV/blend/Z/PE typed BP |
| `SCR_A5B_PROF` | off | decode category self-times |
| `SCR_A5C_BP_PROFILE` | off | per-BP-register counters |

## 11. Backend independence

Same `g_gx` + `Render_OnBP` → `GfxDrawBatch` → D3D11 / Vulkan. No `Vk*` / D3D types in adapters.

## 12. Explicit non-goals / deferred

- FIFO not removed
- Indexed XF direct: deferred
- DL cache: deferred
- A4n desc cache / async present / A4h full-EFB void: remain off
- AMD/RADV: **deferred / unverified**
- Native Windows Vulkan: **unverified**
- D3D11 remains default

## 13. Related docs

- `A5C_BP_SHADOW_DESIGN.md`
- `A5C_DISPLAY_LIST_CACHE_AUDIT.md`
- `A5B_GX_DECODE_PROFILE.md`
- `A5B_NATIVE_GX_SDK_ADAPTERS_AND_DECODE.md`
- `A5A_NATIVE_GRAPHICS_FRONTEND.md`
