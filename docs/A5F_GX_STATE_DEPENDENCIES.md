# A5f — GX State Dependency Model

Generated from `GxDefer_FamiliesForBp` / `BuildPSKey` / `BuildVSKey` / `Render_OnBP` in this tree.

| State family | Raw BP (and XF) inputs | Derived outputs | Flush / consume |
|---|---|---|---|
| GenMode | `0x00` | `PSKey` tev/texgen counts; `VSKey` texgens/chans; cull | Draw Flush |
| TEV | `0x10–0x2F`, `0xC0–0xDF`, `0x28+` order | `PSKey` color/alpha/ind/order | Draw |
| KSEL | `0xF6–0xFD` | `PSKey.ksel[]` | Draw |
| Fog | `0xE8`, `0xF0–0xF2` | `PSKey.fogType` + fog constants at Flush | Draw |
| Alpha / ZTex | `0xF3–0xF5` | `PSKey.alphaFunc`, `zTexType` | Draw |
| TexUnit | `0x80–0xBB`, SU `0x30–0x3F` | Sampler/image at Flush; PSKey via tev order | Draw |
| Blend | `0x41–0x43` | Pipeline blend / dst-alpha / pixel format | Draw |
| Depth | `0x40`, `0x44` | Pipeline Z; `perPixelDepth` | Draw |
| Ras | `0x20`/`0x21` scissor, `0x59`/`0x5A` | Viewport/scissor at Flush; VSKey GenMode overlap | Draw |
| Light | XF `COLOR*CNTRL`, dualtex, texmtx | `VSKey.chan[]` / mtx info | Draw |
| Immediate triggers | `0x52` EFB, `0x65` TLUT, `0x66` inval → dirty All; `0x45`/`47`/`48` finish/token → no key dirty; `0xE0–E7` konst → OnBP latch, no PSKey dirty (constants live at Flush) | Side effects **now** | At write |

### Rules

1. Canonical `g_gx.bp[]` updates always happen in FIFO/SDK order.
2. A family dirty bit is set on every non-nop write that maps to it.
3. `BuildPSKey` / `BuildVSKey` clear their dirty masks after a successful rebuild.
4. A draw must never sample a cached key while its dirty mask is non-zero (enforced by `GxDefer_*NeedsRebuild`).
5. If a register affects multiple families, every dependent family is marked.
6. Shadow invalidation (`GxShadow_InvalidateAll`) forces full key rebuild.

### Not deferred

EFB clear/copy, texture-cache invalidate, finish/token, konst latch side effects, unknown/unclassified trigger paths.
