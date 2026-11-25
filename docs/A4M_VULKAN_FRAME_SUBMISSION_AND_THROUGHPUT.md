# A4m — Vulkan Frame Submission & Rendering Throughput

**Status (2026-11-25):** Measured remaining TR1 frame-pipeline costs after A4l; shipped XF content dedup + opt-in present semaphore chain + CPU/GPU profiling counters. Outdoor unique_xfb remains on the order of A4l (~5–6.5 sustained in matched explores). Title/menu can reach ~60 unique_xfb when not EFB-copy bound. Present chain (`SCR_A4M_PRESENT_CHAIN=1`) removes the pre-present draw-fence CPU wait but has exhibited a rare outdoor/load hang under investigation — **default off**. AMD/RADV **deferred**. Native Windows Vulkan **unverified**. I1 **out of scope**. D3D11 remains default.

**Baseline:** A4l `a761189`.

## 1. A4l baseline & reproduction

| Metric | A4l (documented) |
|---|---|
| Outdoor unique_xfb | ~5.83 |
| XFB presentations / 300s | 3,601 |
| EFB fence-wait | ~0 ms (`inline/sync=N/0`) |
| D3D11 outdoor unique_xfb | ~60 |

Reproduce TR1 explore (RTX 3080):

```bash
tools/a4m_tr1_bench.sh a4l       # A4l-equivalent: chain=0 dedup=0
tools/a4m_tr1_bench.sh combined  # product defaults: chain=0 dedup=1
tools/a4m_tr1_bench.sh present   # chain only
tools/a4m_tr1_bench.sh xf        # dedup only
tools/a4m_tr1_bench.sh compare
```

Clean NVIDIA launch: `tools/run_nvidia_clean.sh` + `SCR_VK_DEVICE=3080` (wrapper uses `env -i`; pass `SCR_*` after the wrapper).

## 2. Frame-pipeline diagram

```
Host input/events
    → Guest simulation (VI cadence; unchanged by A4m)
    → GX draw generation (recomp)
    → VkGx_Draw:
         XF prepare → XF SSBO ring upload (A4m dedup) → dyn UBO pack
         → descriptor alloc/update → pipeline lookup → record DrawIndexed
         → inline EFB-copy record (A4l) into same CB
    → PresentXfb:
         End draw CB → QueueSubmit(+optional signal s_efbReadySem)
         [legacy] CPU WaitForFences(draw)
         → record present blit CB
         → Acquire → SubmitPresent(wait imageAvailable [+ efbReady]) → Present
         → CPU WaitLastSubmit (present fence) → retire XF/desc/staging/EFB dests
    → Fence completion unlocks arena reuse
```

Unnecessary serialization candidates vs required safety waits are classified in §3–4.

## 3. Present command-buffer wait

| Question | Finding |
|---|---|
| Which CB? | Dedicated `s_presentCmd` (blit EFB→swapchain) |
| Which fence? | Device-layer `s_inFlight[syncFrame]` via `Vk_WaitLastSubmit` |
| Shared with draw? | EFB color image only; present CB is not the draw CB |
| Work? | Layout barriers + `vkCmdBlitImage` + present |
| CPU wait required? | Yes for current arena retirement (XF/desc/staging reset after present) |
| Pre-present draw wait? | Legacy: `WaitForFences(fr.fence)` before recording present — **CPU-serialises** draw GPU vs present record |

**A4m chain (opt-in):** draw submit signals `s_efbReadySem`; `Vk_SubmitPresent(..., extraWaitSem)` waits it at `TRANSFER`. Removes `presentDrawFlushNs`. A4d `renderFinished` / acquire semaphore lifecycle unchanged.

**Multi-slot present CBs:** not shipped — `WaitLastSubmit` remains required for arena retirement; a ring alone does not free XF/descriptor reuse without versioned arenas. Bounded slots remain a follow-up if retirement is redesigned.

**Hang note:** with `SCR_A4M_PRESENT_CHAIN=1`, some TR1 load/outdoor transitions stopped producing XFBs (watchdog, guest VI still ~60) without Khronos `Validation Error` / `DEVICE_LOST`. Default leaves chain **off**.

## 4. CPU/GPU profiling methodology

| Knob | Effect |
|---|---|
| `SCR_A4M_PROF=1` | Periodic `A4m-present:` lines |
| `SCR_A4L_EFB_PROF=1` | A4l copy timing lines |
| `SCR_A4M_GPU_TS=1` | Timestamp queries around EFB-copy + present blit (`timestampPeriod` / valid bits) |
| `SCR_A4M_PRESENT_CHAIN=1` | Enable draw→present sem chain |
| `SCR_A4M_XF_DEDUP=0` | Disable XF dedup (default on) |

Stats (`VkGxStats` / `vulkan gx:` log): `xfUploads`, `xfDedupHits`, `xfBytesUploaded`, `descUpdates`, `presentDrawFlushNs`, `presentWaitNs`, `presentCount`, `gpuEfbCopyNs` / `gpuPresentNs` (when GPU_TS on).

Do not treat host submit duration as GPU time. Do not equate XFB presentations with unique_xfb.

## 5. XF upload

A4g versioned ring preserved. A4m adds last-slot content dedup: `Hash64Bytes` + `memcmp` before allocating a new 16 KiB-aligned slot.

Matched A4m `a4l` explore (chain=0, dedup=0), ~280s:

| Counter | Approx end-of-run |
|---|---:|
| xfUploads | ~2.7M |
| xfBytesUploaded | ~44.8 GiB |
| xfDedupHits | 0 |
| drawFlushMs | ~3396 |
| presentWaitMs | ~1504 |

Outdoor dedup hit rate is low (matrices change per draw); title/menu sees higher reuse. Lifetime rules unchanged (no overwrite of pending slots; no `vkDeviceWaitIdle`).

## 6. Descriptors

Still per-draw sets (A4d lifetime). `descUpdates` ≈ draw count (~3.6M in the a4l explore). No immutable-set cache shipped in A4m — measured cost is high but secondary to XF memcpy + present retirement waits for this milestone’s safe changes.

## 7. GPU EFB-copy cost

A4l removed CPU fence waits (`fenceWaitMs=0`, `inline/sync=N/0`). Opt-in `SCR_A4M_GPU_TS=1` timestamps each `RecordEfbCopyCommands` region and the present blit. Normal path does not create the query pool (zero overhead when unset). No A4h full-EFB queue; no per-copy CPU wait restored.

## 8. Queue / command-buffer

Still typically: one draw submit (+ optional soft-submit) + one present submit per XFB. Soft-submit budget unchanged (`softFlush=0` in TR1 benches). No `vkQueueWaitIdle` / per-XFB `vkDeviceWaitIdle`.

## 9. Synchronisation & lifetime (preserved)

- Per-swapchain-image `renderFinished`
- Acquire semaphore lifecycle
- 256 B dynamic UBO alignment
- Dedicated present CB + fence retirement
- Staging / XF ring lifetime
- `Vk_WaitLastSubmit`
- Bounded `SoftSubmitContinue`
- A4l inline EFB copies + `s_efbCopyRetire`
- Versioned XF ring; A4e–A4k visual/sync behaviour

## 10–12. Before/after & per-change results

| Config | Outdoor unique_xfb | Notes |
|---|---:|---|
| A4l documented | ~5.83 sustained | Prior milestone |
| A4m `a4l` (chain=0,dedup=0) | mean ~6.5 after stg110 (live≥3); sustained ~5.2–5.8 | late_half ~0.19%; `out/a4m_a4l_tr1/` |
| A4m chain=1 | Title ~60; some load hangs | Default **off** |
| A4m combined (dedup=1, chain=0) | mean **~6.37** (n=81 live); sustained last20 ~5.2–5.5 | late_half **~0.31%**; xfDedup ~232k / xfUp ~2.74M (~8%); presents n=3713 |

Presentations vs unique_xfb kept separate in logs (`A4m-present: n=` vs `unique_xfb`).

**GPU_TS title sample** (`SCR_A4M_GPU_TS=1`): ~945 EFB-copy regions ≈ 10.6 ms GPU total (~11 µs/copy); present blit samples ≈ 0.9 ms / 65 ≈ 14 µs. GPU copy work is small vs CPU XF/present waits on title.

## 13. UI temporal comparison

Same A4k/A4l capture path (`--shots` / `SCR_VK_SHOT_PREFIX`). late_half blackish on A4m `a4l` explore ~0.19% (A4j/A4k sky intact). Full D3D↔VK ordinal MAE still phase-limited by FPS gap.

## 14–15. Validation & regressions

- Khronos validation title smoke: no `Validation Error` / `DEVICE_LOST` in A4m captured logs
- `scr_efb_copy_tex_test`, `scr_topo_invalid_test`, `scr_shader_cache_test`, `scr_cull_winding_test`, `scr_fog_light_parity_test`: PASS
- Linux + MinGW builds: PASS

## 16. Remaining bottlenecks

1. XF SSBO upload volume (~full 16 KiB payload scale) — largest measured CPU memcpy cost outdoors
2. Present-path `WaitLastSubmit` + arena retirement (required with current lifetime model)
3. Legacy pre-present draw fence when chain off (`presentDrawFlushNs`)
4. Per-draw descriptor update volume
5. GPU EFB region snap + convert (real work; measure with `SCR_A4M_GPU_TS`)
6. D3D11 still ~10× outdoor unique_xfb

## 17–18. Deferred

- AMD/RADV: **deferred, unverified**
- Native Windows Vulkan GPU: **unverified**
- I1 Windows input reliability: **not covered by A4m**
- Vulkan not default; D3D11 remains default/independent

## Tools / env

- `tools/a4m_tr1_bench.sh`
- `SCR_A4M_PROF`, `SCR_A4M_GPU_TS`, `SCR_A4M_PRESENT_CHAIN`, `SCR_A4M_XF_DEDUP`
- Captures: `out/a4m_{a4l,combined,present,xf,vk,d3d}_tr1/`, `out/a4m_profile/`
