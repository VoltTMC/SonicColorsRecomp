# A4l — Vulkan EFB Copy Synchronisation & Rendering Throughput

**Status (2026-11-25):** Per-copy CPU fence waits on the EFB→texture path **eliminated** by GPU-ordered inline recording. RTX 3080 TR1 sustained unique_xfb **~5.0 → ~5.8** (+~16%); EFB-attributed fence wait **~37s → ~0s** over a matched 300s explore; late_half blackish **~0.2%** (A4j/A4k sky intact). Remaining gap to D3D11 ~60 unique_xfb is **not** claimed closed. AMD/RADV **deferred**. Native Windows Vulkan **unverified**. I1 **out of scope**.

**Baseline:** A4k `6fd4ac1`.

## 1. A4k baseline

| Metric | A4k |
|---|---|
| TR1 unique_xfb (Vulkan outdoor) | ~4.9–5.2 |
| D3D11 unique_xfb | ~60 |
| late_half blackish | ~0.6% |
| Identified bottleneck | Per-copy `WaitForFences` on EFB→texture |

## 2. Architecture before A4l

Every non-XFB EFB copy:

1. `DrainFrameSlot` (submit draws + CPU wait + XF/descriptor reset)
2. Begin a fresh CB
3. Region snap (color blit + depth copy) → util convert → dest texture
4. Submit + **CPU `WaitForFences`**
5. Update `genId` / RAM fingerprint

## 3. Meaning of `inline/sync=0/N`

From `VkGxStats` / status log `(inline/sync=A/B)`:

| Counter | Meaning |
|---|---|
| `efbCopyInline` (A) | Copies recorded into the live draw CB with barriers only — **no per-copy CPU wait** |
| `efbCopySynced` (B) | Copies that used the legacy drain+submit+wait path (`SCR_A4L_EFB_SYNC=1`) |
| `efbCopies` | `A + B` |
| A4k logs `inline/sync=0/N` | All copies were synced; the inline path existed as a counter but was unused |

A4l default: `inline/sync=N/0`. Opt-in baseline: `SCR_A4L_EFB_SYNC=1` → `0/N`.

## 4–5. Profiling (`SCR_A4L_EFB_PROF=1`)

Additional stats: `efbCopySoftFlushes`, `efbCopyFenceWaits`, `efbCopyFenceWaitNs`, `efbCopyRecordNs`, `efbCopySubmitNs`.

Matched 300s TR1 explore (`tools/a4l_tr1_bench.sh`), RTX 3080, validation off, scale 1:

| Path | unique_xfb sustained | efbCopy | inline/sync | softFlush | fenceWaitMs (EFB-attributed) | frames (end) | late_half |
|---|---:|---:|---|---:|---:|---:|---:|
| Baseline `SCR_A4L_EFB_SYNC=1` | **~5.02** | 47389 | 0/47389 | 0 | **37297** | 3001 | ~0.58% |
| A4l inline (default) | **~5.83** | 56725 | 56725/0 | 0 | **0.0** | 3601 | **~0.19%** |

Fence-wait duration is CPU block time, not GPU copy duration.

## 6. Wait classification

| Wait | Class | A4l action |
|---|---|---|
| Per-copy `WaitForFences` after copy submit | Unnecessary for GPU sampling (same queue + barriers suffice) | **Removed** (inline) |
| Soft-submit when CB transfer budget trips | Resource-retirement / TDR bound | Kept (`SoftSubmitContinue`); unused in final TR1 bench (budget not hit) |
| Drain before arena wrap / present | Required retirement | Preserved |
| Dest size-churn destroy | Was forcing mid-frame sync | **Deferred** to `s_efbCopyRetire` until after GPU wait |
| RAM fingerprint CPU read | Required CPU-visible guest RAM | Unchanged (hash only; no GPU readback) |

## 7–8. New design

**Default path:** End EFB render pass → record region snap + convert into the **same** CB as draws → leave recording open. Barriers provide execution + memory visibility for later samples in the same CB. Present / arena drain submits once and waits.

**`SoftSubmitContinue`:** If copy count / region-pixel budget / retire-queue bound trips: submit + wait + re-begin CB **without** resetting XF/descriptor/VB arenas (unlike `DrainFrameSlot`). Avoids A4g XF disruption and A4h unbounded-CB TDR.

**Deferred destroy:** `EnsureEfbCopyDest` size mismatch moves the old image into `s_efbCopyRetire`; destroyed in `RetireEfbCopyEntries()` after the next GPU wait.

**Escape hatch:** `SCR_A4L_EFB_SYNC=1` restores A4k per-copy sync for A/B tests.

## 9–10. In-frame behaviour & A4h TDR avoidance

| A4h failure | A4l mitigation |
|---|---|
| Thousands of **full-EFB** blits inlined into one CB → TDR / `DEVICE_LOST` | **Region** snaps (A4h); CB transfer budget (256 copies / ~64 full-EFB pixel equivalents); soft-submit bound |
| Unbounded deferred full-EFB queue | No async queue of full-frame copies; work stays in the live CB with explicit budget |
| Mid-frame XF wipe on every copy | Soft-submit preserves XF ring; full drain only for arena exhaustion / present |

## 11–13. Tests & RAM fingerprint

- `scr_efb_copy_tex_test`, `scr_topo_invalid_test`, `scr_shader_cache_test`, `scr_cull_winding_test`, `scr_fog_light_parity_test`: **PASS**
- RAM fingerprint + CPU decode fallback: unchanged (A4i); fingerprint still taken at record time
- Title validation (`run_nvidia_vk_validation.sh`, short): no `Validation Error` / `DEVICE_LOST` in log

## 14. Before/after performance (RTX 3080)

| | Before (sync) | After (inline) |
|---|---:|---:|
| Sustained outdoor unique_xfb | ~5.0 | **~5.8** |
| EFB fence wait (300s run) | ~37.3 s | **~0 s** |
| XFB frames completed (300s) | 3001 | **3601** |
| D3D11 reference | ~60 | ~60 (unchanged) |

Material improvement; **not** D3D11 parity.

## 15. TR1 visual / temporal

- late_half ~0.19% (A4k ~0.6%; A4j cull preserved)
- Captures: `out/a4l_vk_tr1/`, baseline ref under `out/a4l_compare/baseline_ref/`
- UI phase vs D3D still limited by remaining FPS gap; guest sim timing unchanged

## 16. Validation

Khronos validation title smoke: clean of `Validation Error` / `DEVICE_LOST` in captured log.

## 17. Remaining bottlenecks

- Present-path submit/wait of large CBs (necessary retirement)
- XF SSBO upload volume / ring high-water
- Draw recording + descriptor allocation
- GPU cost of region snaps + convert (real work)
- D3D11 still ~10× unique_xfb

## 18–20. Deferred / out of scope

- AMD/RADV: **deferred, unverified**
- Native Windows Vulkan GPU: **unverified**
- I1 Windows input: **not covered by A4l**
- Vulkan not default; D3D11 remains default/independent

## Tools

- `tools/a4l_tr1_bench.sh baseline|inline|compare`
- `SCR_A4L_EFB_SYNC=1` — A4k sync path
- `SCR_A4L_EFB_PROF=1` — verbose copy timing lines
