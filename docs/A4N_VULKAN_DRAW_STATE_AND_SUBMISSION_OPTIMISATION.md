# A4n — Vulkan Draw-State Streaming, Descriptor Reuse & Submission Overlap

**Status (2026-11-25):** Measured XF/descriptor/present costs after A4m; cut XF CPU traffic by matching the D3D `0x680`-float payload while keeping a full 16 KiB SSBO slot binding; dynamic-UBO descriptor layout landed; descriptor-set reuse left **opt-in** after TR1 load/outdoor hangs; present-chain hang root-caused (A4m async) with a safe record-overlap variant still default **off**. AMD/RADV **deferred**. Native Windows Vulkan **unverified**. I1 **out of scope**. D3D11 remains default.

**Baseline:** A4m `8ab50f3`.

## 1. A4m baseline & reproduction

| Metric | A4l | A4m combined |
|---|---:|---:|
| Mean outdoor unique XFB/s | ~5.83 | ~6.37 |
| Sustained last-20 unique XFB/s | — | ~5.2–5.5 |
| XFB presentations / ~300s | 3,601 | 3,713 |
| XF uploads | ~2.74M | ~2.74M |
| XF dedup hits | 0 | ~232K |
| XF bytes | ~44.8 GiB | ~44.9 GiB |
| Descriptor updates | ~3.68M | ~4.00M |

Reproduce (RTX 3080, clean Vulkan env):

```bash
tools/a4n_tr1_bench.sh a4m       # A4m-like: SCR_A4N_XF_FULL=1, desc cache off
tools/a4n_tr1_bench.sh xf        # 0x680 XF + dedup, desc cache off
tools/a4n_tr1_bench.sh desc      # desc cache on + full XF (experimental)
tools/a4n_tr1_bench.sh chain     # safe present chain only
tools/a4n_tr1_bench.sh combined  # product defaults
tools/a4n_tr1_bench.sh compare
```

`tools/run_nvidia_clean.sh` + `SCR_VK_DEVICE=3080`, `--scale 1 --no-vsync`, `tools/a4k_tr1_explore.txt`, ~300s.

Outputs: `out/a4n_{a4m,xf,desc,chain,combined}_tr1/`, `out/a4n_profile/`, `out/a4n_compare/`.

## 2. XF snapshot & upload lifecycle

```
GX xfmem[0x1000] + xfregs[0x100]
  → renderer packs GfxDrawBatch (xfmem ptr, xfFloatCount)
  → VkGx_Draw::uploadXfSlot
       hash+memcmp vs last slot (A4m dedup)
       allocate 16 KiB-aligned ring slot
       memcpy payload (A4n: 0x680 floats) + zero-pad to 16 KiB
       bind SSBO range = full slot
  → descriptor write (STORAGE_BUFFER at slot offset)
  → draw CB references slot until fence retirement
  → ring wrap / present / drain resets after GPU wait
```

`xfregs` (viewport etc.) are **not** in the SSBO; they feed UBOs / other state. Uploading `0x1000` floats on Vulkan was copying unused high xfmem.

## 3. Meaning of XF counters

| Counter | Measures |
|---|---|
| `xfUploads` | Ring slots written (misses) |
| `xfDedupHits` | Consecutive identical payloads reused (no memcpy) |
| `xfBytesUploaded` | Bytes actually `memcpy`'d into the ring |
| `xfBytesAvoided` | Bytes skipped by dedup |
| `xfMemcpyNs` / `xfHashNs` | Host time in copy / hash |
| `xfRingHighWater` / wraps | Peak occupancy / wrap events |
| Descriptor `xf.range` | GPU-visible bytes per bind (**slot**, not payload) |

Uploads ≠ unique XF states globally (only consecutive-slot dedup). Bytes uploaded ≠ bytes bound.

## 4. Why A4m dedup did not cut XF bytes

Outdoor dedup hit rate ~8% (`~232K / ~2.74M`). Each miss still copied **`0x1000` floats (16 KiB)** while D3D copies **`0x680` floats (6.5 KiB)**. Byte volume is dominated by per-miss payload size, not miss rate.

A4n default payload `0x680` → ~40.6% of A4m bytes per miss (`0x680/0x1000`).

## 5. XF change (shipped)

- Default `xfFloatCount = 0x680` (D3D parity). `SCR_A4N_XF_FULL=1` restores A4m `0x1000` for A/B.
- Slot stride remains 16 KiB; ring fencing/alignment unchanged.
- **Descriptor range = full slot.** Binding only `0x680*4` hung the GPU during TR1 load (watchdog, no Validation Error) — unsized `float xf[]` SSBO reads must stay in-range.
- Zero-fill the unused slot tail after memcpy.
- Dedup still hashes/compares the payload size only.

## 6. Descriptor lifecycle

Per draw (legacy A4m): allocate set → write XF + VS UBO + PS UBO + 8 combined images → bind.

A4n:

- VS/PS bindings are `UNIFORM_BUFFER_DYNAMIC`; host writes pack into `s_dynUbo` with 256-byte alignment; bind passes dynamic offsets.
- Optional cache (`DescCacheKey` = XF offset + 8 views + 8 samplers) skips `vkUpdateDescriptorSets` on hit.
- Cache cleared on `ResetFrameDescriptors` and on `RetireEfbCopyEntries` (destroyed EFB views).

### Cache status

`SCR_A4N_DESC_CACHE=1` achieved ~50–60% hit rates in smoke, but TR1 load/outdoor produced watchdog hangs (guest VI alive, unique_xfb→0, no Validation Error / DEVICE_LOST). **Default off.** Dynamic UBO layout remains so the cache can be re-enabled for further RCA.

Do not key by guest texture address alone; views/samplers/generations matter. Stale EFB views must not remain reachable through cached sets.

## 7. Present-chain hang — root cause

**A4m `SCR_A4M_PRESENT_CHAIN=1` (async):** draw submit signals `s_efbReadySem`; present submit waits it **without** a CPU draw-fence join before Acquire/SubmitPresent. Under heavy outdoor CBs this produced liveness failures (XFB stream stops; no Khronos validation fault).

**A4n safe chain (`SCR_A4M_PRESENT_CHAIN=1` without async):** signal sem + **record** present while GPU runs draw, then `WaitForFences(draw)` before Acquire/SubmitPresent (sem consumed by present wait). Overlaps record only.

**A4n full async:** `SCR_A4N_PRESENT_ASYNC=1` (still experimental).

`SCR_A4N_CHAIN_DIAG=1` logs stage names for wait-for graph reconstruction. Default chain **off**.

Bounded multi-slot present CBs: **not** shipped — arena retirement still serialises on `WaitLastSubmit`.

## 8. GPU EFB-copy timing

A4m title GPU timestamps ~11 μs/copy. A4n TR1 explores with `SCR_A4M_GPU_TS` unset reported `gpuEfbMs=0 (n=0)` — outdoor GPU copy cost not re-measured as a limiting factor vs host XF/present waits. A4l inline path preserved (`inline/sync=N/0`, fenceWaitMs≈0).

## 9. Benchmark results (RTX 3080)

Product combined run (`0x680` XF, dedup on, desc cache off, chain off, ~300s):

| Metric | A4m ref (`8ab50f3`) | A4n combined |
|---|---:|---:|
| Sustained unique_xfb (last 20 >0) | ~5.2–5.5 | **~6.59** |
| Mid-band unique_xfb mean (3–20) | ~6.37 outdoor | ~7.67 (83 samples) |
| XFB presentations | 3,713 | 3,905 |
| XF uploads | ~2.74M | ~0.97M |
| XF dedup hits | ~232K | ~90K |
| XF bytes | ~44.9 GiB | **~6.03 GiB** |
| xfMemcpy host | (not separated) | ~245 ms |
| Descriptor updates | ~4.00M | ~1.52M |
| desc cache hits | n/a | 0 (disabled) |
| present drawFlush | ~3–8 s | ~4.1 s |
| present WaitLastSubmit | ~1.5–2.3 s | ~3.2 s |
| EFB CPU fence wait | ~0 | ~0 (`inline/sync=N/0`) |
| Watchdog hang | no | **no** |

Isolated experiments:

| Variant | Result |
|---|---|
| `0x680` + tight descriptor range | **hang** (load) — fixed by full-slot bind |
| `SCR_A4N_DESC_CACHE=1` | **hang** (load/outdoor) — default off |
| `SCR_A4N_XF_FULL=1` (a4m-like) | A4m-class bytes; entry flaky on some runs |
| Safe present chain | implemented; default off |

Do not equate presentations with unique_xfb. Guest sim/audio timing unchanged.

## 10. Validation & liveness

- Title / TR1: no unresolved Validation Error expected on NVIDIA 3080 path when chain/cache defaults used.
- Present-chain async & desc cache: hangs possible → left disabled.
- Unit tests: `scr_efb_copy_tex_test`, `scr_topo_invalid_test`, `scr_shader_cache_test`, `scr_cull_winding_test`, `scr_fog_light_parity_test`.
- Linux + MinGW builds required.

## 11. Lifetime guarantees (preserved)

Per-swapchain `renderFinished`; acquire semaphore rules; 256-byte dyn UBO align; fence retirement of CB/XF/desc/staging; versioned XF ring; scoped barriers; `Vk_WaitLastSubmit`; bounded `SoftSubmitContinue`; A4l inline EFB; `s_efbCopyRetire`; safe shutdown.

## 12. Remaining bottlenecks

- Outdoor unique_xfb still far below D3D11 (~60).
- Present draw-fence + `WaitLastSubmit` still multi-second host waits per explore when chain off.
- Descriptor updates still ≈ draws while cache stays off.
- Guest/EFB-copy volume still dominates outdoor frame time vs title.

## 13. Deferred / out of scope

- AMD/RADV: **deferred, unverified**
- Native Windows Vulkan GPU: **unverified**
- I1 Windows input reliability: **not covered by A4n**
- Vulkan not default; D3D11 independent and default
