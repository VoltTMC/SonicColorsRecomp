# A5e — Texture BP register coverage

## TX unit map (verified SNCP8P `.sdata` @ `808F29E0`)

| Maps 0–3 | Maps 4–7 | Meaning |
|---|---|---|
| `0x80–0x83` | `0xA0–0xA3` | `TX_SETMODE0` (wrap / filter / aniso bits) |
| `0x84–0x87` | `0xA4–0xA7` | `TX_SETMODE1` (LOD / bias) |
| `0x88–0x8B` | `0xA8–0xAB` | `TX_SETIMAGE0` (w-1, h-1, format) |
| `0x8C–0x8F` | `0xAC–0xAF` | `TX_SETIMAGE1` (TMEM even / cache) |
| `0x90–0x93` | `0xB0–0xB3` | `TX_SETIMAGE2` (TMEM odd) |
| `0x94–0x97` | `0xB4–0xB7` | `TX_SETIMAGE3` (image addr >> 5) |
| `0x98–0x9B` | `0xB8–0xBB` | `TX_SETTLUT` |

## Writers

| Register family | Immediate SDK | Display list | Direct? |
|---|---|---|---|
| SETMODE / SETIMAGE | `GXLoadTexObj` / `PreLoaded` | Yes (common) | **A5e** immediate only |
| SETTLUT | PreLoaded (CI) + `GXLoadTlut` | Possible | **A5e** |
| `0x66` TX_INVALTAGS | `GXInvalidateTexAll` | rare | A5d HOT (Trigger) |

## Classification (unchanged specials)

Trigger/Special (never identical-skip): `0x45`, `0x47`, `0x48`, `0x52`, `0x65`, `0x66`, `0xE0–0xE7`.

TX `0x80–0x94` / `0xA0–0xB7`: **Ordinary** — `bpNop` when shadow valid and value unchanged.

## Final bucket

1. Direct native adapter — immediate LoadTexObj / PreLoaded / LoadTlut
2. Optimised legacy — DL TX + PE/fog (`bpNop`)
3. Required DL operation — most title/stage TX traffic inside DLs
4. Special side-effecting — inval / EFB / TLUT load ordering
5. Not adapted — full texture decode rewrite, descriptor cache
