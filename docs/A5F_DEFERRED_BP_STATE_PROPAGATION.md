# A5f — Deferred BP State Propagation

## Goal

Reduce CPU cost of display-list BP playback by distinguishing **canonical guest/GX register updates** from **derived rendering-state propagation**, without DL caching or reordering.

Opt-in: `SCR_A5F_DEFERRED_BP=1` (default off — immediate reference).

## A5e baseline

Commit `005a1f2`. Immediate SDK adapters cut FIFO traffic (~20 MiB combined avoided) but deep-sample BP category wall time remained ~35 s — dominated by DL BP volume + per-write `Render_Flush` / `Render_OnBP` overhead, not per-BP `BuildPSKey` (keys already built at Flush/draw).

## Profiling findings

| Path | Role | A5f action |
|---|---|---|
| BP decode / `GX_ApplyBpWord` | Shadow + mask + classify | Unchanged order |
| Identical ordinary + valid shadow | `bpNop` | Retained (A5b) |
| `Render_Flush` before BP change | Submit pending geometry | **Skip when no pending indices** |
| `Render_OnBP` | EFB `0x52`, TLUT `0x65`, konst `0xE0–E7` only | **Skip empty ordinary calls** |
| `BuildPSKey` / `BuildVSKey` | Draw-time derived keys | **Dirty-family cache** |
| Special / trigger | Finish/token/EFB/TLUT/inval/konst | Always immediate |

Self-time vs inclusive: nested DL decode remains attributed under A5b `nestDecodeMs`; A5f stats (`flushSkip`, `onBpSkip`, `psReuse`, `vsReuse`) are logged via `GxDefer_LogStats` on present.

## Two-level model

**Level 1 — Canonical (always immediate, ordered):**

- Raw BP shadow (`g_gx.bp[]` + `gx_shadow` validity)
- Guest-visible side effects already applied by SDK adapters / FIFO
- Special side effects: `0x45` finish, `0x47`/`0x48` token, `0x66` `Render_InvalidateTextureCache`
- Immediate `Render_OnBP` for `0x52` / `0x65` / `0xE0–E7`

**Level 2 — Derived (coalesced until consumer):**

- `PSKey` / `VSKey` rebuild
- Empty `Render_Flush` / empty `Render_OnBP`

Flush consumers: draw Flush with non-empty indices; EFB copy path still runs at `0x52` write time.

## Implementation

- `runtime/gx/gx_defer.{h,cpp}` — family map, dirty bits, stats
- `GX_ApplyBpWord` — conditional Flush/OnBP under `SCR_A5F_DEFERRED_BP`
- `BuildPSKey` / `BuildVSKey` — reuse cached keys when dirty==0
- `Render_HasPendingDraw()` — `!s_indices.empty()`
- XF channel/texmtx loads mark Light/GenMode when deferred

## Special registers (never deferred semantics)

`0x45`, `0x47`, `0x48`, `0x52`, `0x65`, `0x66`, `0xE0–0xE7` — classification unchanged from A5c/A5d. Identical-value skip does **not** apply. `0x66` always invalidates the texture cache.

## KColorSel

See `A5F_KCOLORSEL_DIRTY_FLUSH.md`. SNCP8P SwapMode already emits KSEL BP immediately; host deferral marks KSEL dirty and rebuilds `PSKey.ksel[]` at the next draw.

## Tests / benches

- `scr_a5f_bp_deferred_state_test`
- `tools/a5f_tr1_bench.sh` — `baseline|immediate|deferred|ksel|combined|compare`
- Logs: `out/a5f_baseline/`, `out/a5f_deferred/`, `out/a5f_ksel/`, `out/a5f_bp_profile/`, `out/a5f_compare/`

## Constraints preserved

No DL cache; FIFO order retained; `PPC_ORIGINAL` DL record; D3D11 default; Vulkan opt-in; A4n desc cache / async present remain off; AMD/RADV deferred; native Windows Vulkan unverified.

## Measurements (RTX 3080, 90s `a5f_tr1_bench`, A5e combined on)

| Mode | bp loads | bp cat ms | µs/bp | flushSkip | onBpSkip | ps reuse/build | vs reuse/build | title unique_xfb | mid unique_xfb |
|---|---:|---:|---:|---:|---:|---|---|---:|---:|
| baseline (`DEFERRED=0`) | 8.21M | 31721 | 3.86 | 0 | 0 | n/a | n/a | 59.3 | 8.6 |
| deferred (`DEFERRED=1`) | 8.67M | 30367 | 3.50 | 5.36M | 1.70M | 718k / 228k | 809k / 138k | 59.3 | 9.5 |

Primary claim: ~9% lower BP-category time per load with higher in-window throughput; unique_xfb title/mid not regressed in this sample. Remaining BP wall is still dominated by special konst/`Render_OnBP` work and indexed XF — DL execution is **not** eliminated.

## Status

Deferred path is **opt-in** (`SCR_A5F_DEFERRED_BP`, default off). Enable for measured BP decode relief; keep off as the correctness reference. AMD/RADV deferred; native Windows Vulkan unverified. Validation cleanliness not claimed from profiling runs.
