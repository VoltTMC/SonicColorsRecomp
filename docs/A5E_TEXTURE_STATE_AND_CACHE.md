# A5e — Texture state and cache coherency

## Layers (not interchangeable)

1. **Guest `GXTexObj` (0x20)** — mode0/1, image0/3, fmt `@+0x14`, tlut name `@+0x18`, flags `@+0x1F`.
2. **Guest `__gx` caches** — region/tlut callbacks `@+0x518/+0x51C`; per-map image0 `@+0x564`, mode0 `@+0x584`; dirty `@+0x5FC`.
3. **Host BP shadow `g_gx.bp[]`** — filled by FIFO or typed `GX_ApplyBpWord`.
4. **Decoded RGBA + GPU image** — `GetTexture` / Vulkan tex cache (addr×fmt×dims×mips×CI/TLUT).
5. **Sampler** — from `TX_SETMODE0/1` at bind time.
6. **Descriptors** — backend-owned; A4n descriptor cache remains **disabled**.

## Validity

- Direct LoadTexObj and legacy BP share `GX_ApplyBpWord`.
- `GX_Init` / shadow invalidate still force legacy until re-established.
- `0x66` → `Render_InvalidateTextureCache` (A5d); never identical-skipped.
- EFB-copy textures keep generation / fingerprint rules (A4f/A4i).
- Guest RAM / TLUT edits are detected by existing hash / fingerprint paths — adapters do not skip decode.

## CI / TLUT

- Colour-index: TexObj flags bit1 clear → PreLoaded calls tlut-region callback and emits `TX_SETTLUT`.
- `GXLoadTlut@8036C7C0` updates TMEM mapping; flush may skip when IND_IMASK shadow valid (same A5d rule).
