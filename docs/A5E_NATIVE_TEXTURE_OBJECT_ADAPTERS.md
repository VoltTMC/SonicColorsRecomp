# A5e — Native texture-object adapters

**Start:** `caf7a96e2be743a5705d04ead1cac718f23643f1` (A5d)  
**Branch:** `main`

## 1. A5d baseline

- Combined sample: `directBp≈29k`, `flushSkip≈12k`, avoided ≈287 KiB, `fallback=0`.
- TX `0x80–0x94` remained legacy + `bpNop` (mostly DL / `GXLoadTexObj`).
- True KSEL is `0xF6–0xFD` (A5d); TX unit regs are separate.

## 2. Call path (immediate SDK)

```
game → GXLoadTexObj@8036C730
         → (*__gx+0x518) TexRegionCallback(obj, map)
         → GXLoadTexObjPreLoaded@8036C5C0(obj, region, map)
              → pack TexObj/Region low24 + BP ids from .sdata@808F29E0
              → FIFO 0x61 × (6 [+1 CI TLUT])
              → guest TexObj/Region high bytes
              → __gx +0x564/+0x584 caches; dirty |= 1; clear +0x2
              → (CI) (*__gx+0x51C)(tlutName) → TX_SETTLUT
         → GX_ApplyBpWord / Render_OnBP → GetTexture / sampler (D3D11 & Vulkan)
```

`GXInitTexObj@8036BFB0` only fills the 0x20-byte guest `GXTexObj` (no FIFO) — not overridden.

## 3. Direct adapters (`SCR_A5E_DIRECT_TEXOBJ=1`)

| Function | SNCP8P | Notes |
|---|---|---|
| `GXLoadTexObjPreLoaded` | `8036C5C0` | Typed BP for mode0/1 + image0–3 [+tlut] |
| `GXLoadTexObj` | `8036C730` | Guest region callback then PreLoaded helper |
| `GXLoadTlut` | `8036C7C0` | Flush + 2 BP + flush; TMEM merge into region/obj |

DL recording → `PPC_ORIGINAL`. Shared `GX_ApplyBpWord` / `afterFifoW` / single `g_gx`. No backend descriptor mutation in the adapter.

## 4. Image vs sampler

- **Sampler:** `TX_SETMODE0/1` (`0x80/84` …) from TexObj `+0/+4`.
- **Image identity:** `TX_SETIMAGE0/3` (`0x88/94` …) width/height/fmt + addr; decode/upload remains in renderer on draw.
- Changing wrap/filter alone does not force a new decode key beyond existing fingerprint rules.

## 5. Special / deferred

- `0x66` TX_INVALTAGS still Trigger (A5d).
- DL-originated TX BP stays on FIFO (`bpNop` when identical).
- Indexed XF / DL cache unchanged (deferred).
- Descriptor cache / async present remain off.

## 6. Tests / benches

- `scr_a5e_texobj_state_test`
- `tools/a5e_tr1_bench.sh` — `legacy|a5d|texobj|combined|tex_profile|compare`
- Logs: `out/a5e_*`

### 90s TR1 explore (RTX 3080, validation off)

| Config | texObj | texObjBp | avoided | fallback | title unique_xfb | notes |
|---|---:|---:|---:|---:|---:|---|
| TEXOBJ only | ~216k | ~1.29M | ~6.2 MiB | 0 | ~55–60 | TX `0x8C/90/94` direct-dominated |
| combined A5a–A5e | ~228k | ~1.37M | ~20 MiB | 0 | ~55–60 | + XF/TEV/HOT; `tlut=0` in sample |

Do not claim outdoor FPS from mid-band alone. Decode/upload unchanged — FIFO TX encode/decode avoided for immediate loads only.

## 7. Env

| Var | Default | Meaning |
|---|---|---|
| `SCR_A5E_DIRECT_TEXOBJ` | off | LoadTexObj / PreLoaded / LoadTlut |

## 8. Status

- AMD/RADV **deferred / unverified**
- Native Windows Vulkan **unverified**
- D3D11 default
