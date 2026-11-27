# A5n — Vulkan XF upload profile

## Path (canonical XF → GPU-visible slot)

1. Guest / native front-end mutates `g_gx.xfmem` (FIFO `LoadXF` / `LoadIndexedXF`, A5g SDK adapters, display-list playback).
2. Every production writer calls `Render_OnXF` → `MarkXfDirtyRange` (sets `s_xfDirty`, unions dirty float range, bumps `s_xfGeneration`).
3. `FlushVulkan` copies live `batch.xfmem = g_gx.xfmem`, `xfDirty*`, `xfGeneration`, then clears host dirty flags.
4. `VkGx_Draw`: if `batch.xfDirty || !s_xfUploaded` → `uploadXfSlot`; else clean skip.
5. `uploadXfSlot` (A4n): optional dedup vs last ring slot; on miss allocate next 16 KiB-aligned slot, `memcpy` full `0x680` floats, zero pad to slot size, bind offset = slot.
6. Descriptor path consumes `s_xfBindOffset` (A5l sticky reuse may skip `UpdateDescriptorSets`).

Payload hashed/compared: **`0x680` floats (6656 bytes)** unless `SCR_A4N_XF_FULL=1` (`0x1000`). Slot size remains 16 KiB. Exact compares read a CPU shadow of the last upload (`s_xfLastCpu`), not the host-visible ring.

## Exclusive CPU stages (always counted)

| Counter | Meaning |
|---|---|
| `xfStateLookups` | Draws that evaluate XF need |
| `xfCleanSkips` | `!dirty && uploaded` |
| `xfSlotLookups` | Entered `uploadXfSlot` |
| `xfHashCalls` / `xfHashBytes` / `xfHashNs` | Fingerprint work |
| `xfMemcmpCalls` / `xfMemcmpBytes` / `xfMemcmpNs` | Exact compare |
| `xfPrepNs` | Slot align / exhaustion check |
| `xfMemcpyNs` / `xfBytesUploaded` / `xfUploads` | GPU-visible update |
| `xfDedupHits` / `xfDirtyRangeHits` / `xfDirtyRangeMisses` / `xfFullCompareFallbacks` | Decision path |
| `xfBindCount` / `xfBindReuse` | Bind offset commits / same-as-prior-in-CB |
| `drawXfNs` / `drawEntryNs` | Parent exclusive draw timing |

Present line: `A5n-present:` (requires `SCR_A4M_PROF` / A4n present logging cadence).

## Baseline expectation (pre-A5n opt)

When XF is dirty every draw with changing matrices: hash(~6.5 KiB) + memcmp miss + memcpy + second hash. When dirty but identical (A5i skipped less often): hash + memcmp hit. When clean: no upload work (`xfCleanSkips`).

Do not attribute `drawEntryNs − Σ(other stages)` entirely to XF — use `drawXfNs` and the A5n sub-counters.
