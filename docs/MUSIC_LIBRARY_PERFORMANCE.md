# Music Library performance

## Bottlenecks found (2026-11-20)

Measured and code-audited hot paths (UI thread, Music tab every frame):

| Hotspot | Effect | Fix |
|---------|--------|-----|
| `MusicManifest_AssetStatus` → **`Wav_Load` full PCM decode** for every replacement row every frame | Multi‑MB allocations × N tracks × FPS; multi‑second stalls with a few assigned WAVs | **`Wav_Probe`** header-only (~64 KiB read); list status never decodes samples |
| `MusicDeps_Status()` / `RefreshStatusLocked` every frame (Windows) | Re-read + JSON-parse manifest; filesystem probes; previously could **`LinkBins` copy DLLs** | Status **cached** until Ensure/install dirties it; LinkBins only in Ensure/ApplyEnv |
| Track list: no clipper; O(n) `FindMapping` + filter with per-field `tolower` string copies | All ~114+ rows submitted to ImGui every frame; quadratic mapping lookup | **`ImGuiListClipper`**, path→index map, filter index rebuilt only when inputs change |
| Search filter rebuilt lowercase strings for every asset every frame | Extra allocations while typing/scrolling | Cached `s_searchLower` + `AsciiContainsI` |

Preview decode / Prepare / doctor were already async or button-driven and were not the browse lag source.

## Architecture notes

- **Catalog**: still loaded once via `Reload()` (`catalog.json`); not rebuilt on tab switch (`s_loaded`).
- **Filtered rows**: `s_filtered` indices; signature includes search, combos, catalog size, and mapping fingerprint.
- **Deps**: `g_statusDirty`; UI re-copies status at most ~every 45 frames when idle.
- **Profiling**: `SCR_MUSIC_PERF=1` logs `[MUSIC_PERF] filter_rebuild … ms`. `./tools/music_perf_check.sh` compares Probe vs Load.

## Benchmarks (dev host, 2026-11-20)

From `./tools/music_perf_check.sh` (8.6 MiB silent stereo WAV, 45 s @ 48 kHz):

| Operation | Result |
|-----------|--------|
| `Wav_Probe` | **0.042 ms** |
| `Wav_Load` (same file) | **15.3 ms** (~365× slower) |
| `catalog.json` parse (114 assets) | **1.7 ms** |

| UI path | Before | After |
|---------|--------|--------|
| List status with assigned WAVs | Full decode × rows × frames | Header probe × **clipped** rows |
| Windows deps panel idle | Manifest/FS (± LinkBins) every frame | Cached until Ensure dirties |
| Filter/search | Per-frame string allocs over all assets | Rebuild on change only |

Native Windows “feel” of scrolling remains UNVERIFIED here (Wine ≠ native). Re-run `music_perf_check.sh` / `SCR_MUSIC_PERF=1` on target hardware.

## Remaining limitations

- First `Reload()` still synchronous (JSON + optional catalog tool); acceptable for ~100 tracks.
- Visible-row status still does a cheap filesystem probe (exists + header); not a full RAM catalog of PCM.
- Prepare / original preview remain process-bound (vgmstream); UI stays interactive via existing async paths.
