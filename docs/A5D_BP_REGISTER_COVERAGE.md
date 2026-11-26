# A5d — BP register coverage

## Hot register → meaning → writer map (SNCP8P)

| BP | Verified meaning | Main immediate writer(s) | Origin | Direct? |
|---|---|---|---|---|
| `0x0F` | IND_IMASK | `__GXSetIndirectMask@8036D400`, `__GXFlushTextureState@8036D430` | SDK + every tex flush | **A5d HOT** (flush skip when shadow valid) |
| `0x30`/`0x31` | SU_SSIZE0 / SU_TSIZE0 | `GXSetTexCoordScale@8036C9C0`, `GXSetTexCoordCylWrap@8036CA40`, `GXSetTexCoordBias@8036A8B0`, texobj path `@8036CAB0` | SDK (+ DL) | **A5d HOT** (scale/cyl/bias); CAB0 remains legacy |
| `0x66` | TX_INVALTAGS | `GXInvalidateTexAll@8036C930` | SDK | **A5d HOT** (Trigger; never nop) |
| `0x80`/`0x84`/`0x88`/`0x8C`/`0x90`/`0x94` | TX_SETMODE0/1 + IMAGE* (map0 group) | `GXLoadTexObj` family / init | Mostly **DL + LoadTexObj** | Optimised legacy (`bpNop`); no full LoadTexObj adapter |
| `0x43` | PE_CONTROL | PE setters | SDK/DL | Legacy |
| `0x4A`/`0x4B` | PE / fog-range family | PE/fog SDK | SDK/DL | Legacy |
| `0xF6`–`0xFD` | TEV_KSEL | `GXSetTevSwapMode@8036D2B0` (swap fields in `0xF6`); KColor/KAlpha often dirty-deferred | SDK | **A5d KSEL** (swap); full KColorSel deferred with dirty flush |
| `0x45`/`0x47`/`0x48`/`0x52`/`0x65`/`0xE0–E7` | finish/token/EFB/TLUT/konst | various | special | **Trigger/Special — never identical-skip** |

## Classification updates (A5d)

- `0x66` promoted to **Trigger** (was Ordinary). `Render_InvalidateTextureCache()` clears host tex fingerprints / bind cache.
- A5c docs incorrectly labelled `0x80–0x94` as “KSEL”; they are **texture unit** registers. True KSEL is `0xF6–0xFD` (`PSKey.ksel[]`).

## Final bucket per top hot reg

1. Direct native adapter — `0x0F`, `0x30`/`0x31` (selected setters), `0x66`, KSEL swap via `0xF6`
2. Optimised legacy BP decoding — TX `0x80–0x94`, PE `0x43`/`0x4A`/`0x4B` (`bpNop` + shared `GX_ApplyBpWord`)
3. Required display-list operation — most TX image/mode traffic inside DLs
4. Special side-effecting — `0x45`/`47`/`48`/`52`/`65`/`66`/`E0–E7`
5. Not yet fully adapted — full `GXLoadTexObj`, dirty-state KColorSel flush, indexed XF
