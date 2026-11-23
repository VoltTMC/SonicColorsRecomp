# A3w — Offline Shader Precompilation and Pipeline Cache

**Status:** implemented (portable DXBC layer) · **Date:** 2026-11-23  
**Branch:** `main` · **Depends on:** existing BlackKnight-derived shadercache (not a parallel system)  
**Independent of A3v:** yes — A3v player composite remains **BENCHED** / default **off**

---

## 1. Architecture audit (what already existed)

### Graphics API path (Linux / Windows)

| Layer | Role |
|---|---|
| Game GX FIFO | Wii GX command stream |
| `runtime/gx/*` | Decode + BP/XF/CP state → D3D11 draws |
| `shadergen.*` | GX state → HLSL (`VSKey` / `PSKey`) |
| D3DCompile / vkd3d-shader | HLSL → DXBC |
| D3D11 (`Create*Shader`, state objects) | Host API used by the recomp |
| **Linux:** DXVK-native | D3D11 → Vulkan |
| **Linux NVIDIA:** NVIDIA Vulkan ICD | Device/driver PSO compile |
| **Windows:** native D3D11 | Driver shader/PSO path |

The running app does **not** submit Vulkan pipelines directly. On Linux it is **D3D11 → DXVK → Vulkan**. Claiming “the whole game is precompiled into Vulkan” would be false.

### Existing BlackKnight-derived shader infrastructure (reused)

Already present before A3w:

1. **GX → key** — `VSKey` / `PSKey` in `runtime/gx/shadergen.h` enumerate every field that changes generated HLSL.
2. **Generation** — `GenerateVertexShader` / `GeneratePixelShader`.
3. **Disk blob cache** — `shadercache/{vs,ps}_5_0_<hash>_<len>.cso` keyed by **source hash**, not by key alone.
4. **Key lists** — `vs_keys.bin` / `ps_keys.bin` (raw key structs) for preload/precompile.
5. **Offline tool** — `SonicColorsRecomp --precompile-shaders <dir>` (no window): HLSL→DXBC for every unique source from keys.
6. **Runtime preload** — `PreloadShaders()` at `Renderer_Init`: load blobs, `Create*Shader`, async compile misses.
7. **Async workers** — background HLSL compile for first-seen keys.
8. **Fixed-function caches** — blend / depth / rasterizer / sampler maps (create-on-first-use).

### What A3w adds

| Piece | Purpose |
|---|---|
| `runtime/gx/shader_cache.{h,cpp}` | Manifest schema, stats, collect hooks, `--shader-cache-report` |
| `manifest.json` | Versioned portable-cache metadata (schema / generator / game / key sizes / API) |
| `SCR_SHADER_COLLECT` (+ `_AREA`) | Area-tagged key dumps under `shadercache/collect/` |
| `tools/shader_collect_merge.py` | Merge collect dumps into root key lists (dedupe) |
| `tools/a3w_collect_area.sh` | NVIDIA-clean collection helper |
| `scr_shader_cache_test` | Manifest / invalidation / stats tests |
| Stats overlay (`SCR_SHADER_CACHE_STATS=1`) + `--perf` line | Hit/miss / compile counters |

### What remains runtime / driver-dependent

| Stage | Offline? | Notes |
|---|---|---|
| 1. GX key → HLSL generation | **Yes** (keys + generator) | Keys collected from real play |
| 2. HLSL → DXBC compile | **Yes** (`--precompile-shaders`) | Portable across D3D11 hosts using same generator |
| 3. D3D11 `Create*Shader` | **Startup preload** | Needs a live device; not a shippable binary alone |
| 4. Blend/depth/raster/sampler | **First-use + cache** | Cheap; not DXBC |
| 5. DXVK / Vulkan PSO | **No (not shipped)** | Device + driver specific; DXVK may persist its own cache locally |

---

## 2. Shader keys (collision safety)

`VSKey` / `PSKey` are the cache identity for generation. Blob files are additionally keyed by **HLSL source hash**, so two keys that generate identical source share one `.cso` safely. A wrong shader reuse would require either:

- incomplete key fields (generator ignores a GX input that still changes HLSL), or  
- manifest/generator version mismatch with stale keys.

A3w invalidates via `manifest.json` when schema, generator id, game id, key byte sizes, or API disagree.

Current key field coverage (from `shadergen.h`):

- **VS:** components, texgens, dual-tex, tex/post matrices, colour channels  
- **PS:** TEV stages/orders/indirect, ksel, alpha test, z-texture, fog, pixel format, dst-alpha, per-pixel depth  

If the generator gains new inputs, bump `kScrShaderGeneratorId` / schema and rebuild the cache.

---

## 3. Collection workflow

1. Play (or script) with collection on:
   ```bash
   tools/a3w_collect_area.sh title --present-limit 180
   tools/a3w_collect_area.sh tr1 --nand out/test_nand --input-script tools/a2g_tr1_enter.txt
   ```
2. Merge:
   ```bash
   python3 tools/shader_collect_merge.py ~/.local/share/SonicColorsRecomp/shadercache
   # or the portable/user shadercache path in use
   ```
3. Offline compile:
   ```bash
   ./out/SonicColorsRecomp --precompile-shaders /path/to/shadercache
   ```
4. Report (does **not** claim complete coverage):
   ```bash
   ./out/SonicColorsRecomp --shader-cache-report /path/to/shadercache
   ```

Coverage targets (operator checklist — not auto-proven): title, menus, world map, Tropical Resort Acts 1–6, other worlds/acts, bosses, wisps, cutscenes, HUD/UI, pause, loading.

A collection session **must not** silently claim complete coverage. The merge tool prints an explicit incomplete-coverage note.

---

## 4. Cache format and versioning

### Portable (shippable)

- `vs_keys.bin` / `ps_keys.bin` — raw `VSKey` / `PSKey` arrays  
- `*.cso` — DXBC blobs named `{vs,ps}_5_0_<srcHash>_<srcLen>.cso`  
- `manifest.json` — schema fields:

```json
{
  "schemaVersion": 1,
  "gameId": "SNCP8P",
  "gameRev": "00",
  "generatorId": "scr-shadergen-1",
  "api": "d3d11",
  "vsTarget": "vs_5_0",
  "psTarget": "ps_5_0",
  "vsKeyBytes": 96,
  "psKeyBytes": 300,
  "vsKeyCount": …,
  "psKeyCount": …,
  "csoCount": …,
  "portableDxbc": 1,
  "deviceSpecificPipelines": 0
}
```

### Device-specific (not shipped as universal)

- DXVK state/pipeline cache under the user’s DXVK cache directory  
- NVIDIA/AMD/Intel driver shader caches  

These may be warmed by normal play or by startup preload creating D3D11 shaders (which exercises DXVK translation). They are **not** distributed as a single cross-GPU Vulkan pipeline pack.

### Invalidation

Runtime `CheckCacheManifestOnStartup` / `ShaderCache_ManifestCompatible`:

- wrong schema / generator / game / key sizes / api → warn and fall back to per-blob source-hash load + runtime compile  
- missing cache → boot continues (fallback path intact)

---

## 5. Runtime behaviour

1. `Renderer_Init` → workers + `PreloadShaders`  
2. Manifest check (user dir, then shipped `shadercache/` next to exe)  
3. Load keys → `LoadCachedBlob` → `Create*Shader` (or async compile)  
4. First-use miss: generate HLSL → disk blob if present → else compile (async by default) → remember key  
5. Stats: mem hit/miss, disk hit/miss, compile counts, blend/depth/raster create vs hit  

Missing or corrupt cache **never** blocks boot.

---

## 6. Pipeline / state prewarming

| Mechanism | Status |
|---|---|
| Preload DXBC → `Create*Shader` at init | **Done** (existing + A3w stats/manifest) |
| Offline HLSL→DXBC | **Done** |
| Warm blend/depth/raster from a recorded state list | **Not done** (create-on-first-use; usually cheap) |
| Offscreen full-game FIFO warmup | **Declined** (would risk sim/audio/save side effects) |
| Ship universal Vulkan PSOs | **Declined** (device/driver specific) |

Progressive loading: packaging ships keys+DXBC; first launch still lets DXVK build device PSOs without hitching on HLSL compile for covered variants.

---

## 7. Benchmark methodology

Environment: `tools/run_nvidia_clean.sh` (forces `nvidia_icd.json`, clean `env -i`). Confirm adapter is RTX 3080 / NVIDIA, not RADV.

Compare at minimum:

1. Empty cache (no `.cso`, optional empty keys)  
2. Partial keys/blobs  
3. Fully populated portable cache  
4. Cache disabled (clear / rename dir) vs enabled  

Metrics: startup to first frame, shader compile counts (`SCR_SHADER_CACHE_STATS` / `--perf`), first TR1 entry hitch, frame-time spikes, average frame time, RSS of cache on disk.

Keep guest sim and audio at normal timing (`--60fps`). Do **not** attribute A3v composite gains to A3w.

### Results (2026-11-23, RTX 3080 / `tools/run_nvidia_clean.sh` / Wayland)

Confirmed adapter: `NVIDIA GeForce RTX 3080` (DXVK v3.1.1, driver 620.73.4). Not RADV.

| Scenario | Preload | First ~10s ShCache (`--perf`) | Runtime HLSL compiles |
|---|---|---|---|
| Empty `shadercache/` | none | `vs=0/…+6 ps=0/…+8 disk=0/13 cmp=5/8 man=0` | **yes** (title path) |
| Full portable cache | **330 VS + 6793 PS** | `vs=330/…+0 ps=6793/…+0 disk=1/0 cmp=0/0 man=1` | **none** |
| Offline `--precompile-shaders` (already warm) | — | — | 0 compiled / 0 failed in ~0.05 s |
| Cache report | — | — | 330 unique VS keys, 6793 unique PS keys, **640** DXBC `.cso` (7.12 MiB) |

Title-screen boot (~25 s wall timeout): sim≈60, underruns 0 on both; full cache shows **zero mem-miss and zero compile** after preload. Empty cache compiles on first use (async). DXVK/Vulkan PSO cost is outside this portable layer.

---

## 8. Honest achievement matrix

| Goal | Achieved? |
|---|---|
| GX shader generation inputs recorded offline | **Yes** (keys + collect) |
| Shader compilation moved offline (HLSL→DXBC) | **Yes** (`--precompile-shaders`) |
| D3D11 shader objects created before gameplay | **Partial** (preload at init for known keys) |
| Backend state objects fully precreated | **Partial** (shaders yes; FF states on first use) |
| GPU-driver Vulkan PSO cache shipped universally | **No** (correctly refused) |
| Zero first-use hitch for entire game | **No** — unknown variants + DXVK PSO still runtime |

---

## 9. Testing

- `scr_shader_cache_test` — keys, manifest R/W, corrupt/mismatch rejection, stats  
- `tools/regression_smoke.sh` includes the test  
- Linux native build + RTX 3080 clean run  
- MinGW Windows build (compile); native Win GPU = separate checklist  
- A3v remains off; music/launcher paths untouched  

---

## 10. Remaining limitations

- Collection coverage is only as good as played/scripted areas.  
- Source-hash `.cso` names mean generator changes orphan old blobs (manifest/generator bump).  
- DXVK/NVIDIA PSO cost is outside the portable cache.  
- No offscreen FIFO warmup (by design).  
- Native Windows GPU verification may remain open.

---

## Related

- A3v (benched): `docs/A3V_PRODUCTION_PLAYER_COMPOSITE.md`  
- Progress: `docs/PROGRESS.md`  
- Packaging: `tools/package_linux.sh`, `tools/package.ps1`
