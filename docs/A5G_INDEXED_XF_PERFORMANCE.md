# A5g — Indexed XF Performance

## Baseline (A5e/A5f TR1 samples)

Indexed XF (`GfxDecodeCat::XfIdx`) was ~17% of RunCommands handler self-time in A5b profiling and remained a large `catMs xfIdx=` wall after A5f (~23–30 s in 90s TR1 explores with A5e adapters on). Imm XF adapters do not cover this path.

## Candidate configuration

Both arms enable A5a–A5e adapters; `SCR_A5F_DEFERRED_BP=0` unless noted. Switch under test: `SCR_A5G_DIRECT_INDEXED_XF`.

Matched 90s TR1 explore, RTX 3080, A5a–A5e on, `SCR_A5F_DEFERRED_BP=0` (isolate indexed XF).

| Metric | Baseline (`INDEXED=0`) | Candidate (`INDEXED=1`) |
|---|---:|---:|
| End-of-run `fifoIn` | ≈203M | ≈371M (more work in-window) |
| `catMs xfIdx` (end) | ≈31115 | ≈8191 |
| `xfIdx` at ~200M fifoIn | ≈20955 (`fifoIn≈157–203M`) | ≈5348 (`fifoIn≈202M`) |
| `idxXf` / `idxPos` / `idxNrm` | 0 / 0 / 0 | ≈1.24M / ≈1.15M / ≈84k |
| `fallback` / `dlRec` | 0 / 0 | 0 / 0 |
| `avoided` bytes (end) | ≈25.3MiB | ≈55.3MiB |
| Title unique XFB/s (mean ≥50) | ~59-class (script title phase) | ~59-class |
| TR1 mid-band unique XFB/s | ~6.5–9 late samples | ~14–21 late samples (faster decode → further into stage) |
| Guest VI Hz | ≈60 | ≈60 |

Primary claim: **indexed-XF decode category wall drops ~4–6× at matched FIFO progress**; Pos Indx dominates migrated ops (~93%). Overall unique_xfb mid rises because the host completes more guest frames in the fixed wall window — do not treat as a GPU FPS claim.

## Expected savings

Each successful direct Indx avoids **5 FIFO bytes** (opcode + word) and the corresponding decode dispatch. Guest-memory read and XF apply still occur in `GX_ApplyIndexedXf` (same as FIFO). Primary win is PPC FIFO encode + gather/decode overhead for Pos/Nrm Indx SDK calls outside DL recording.

## Remaining bottlenecks

- FIFO-resident indexed XF inside display lists (playback).
- Opcodes `0x30`/`0x38` (no SDK adapter this milestone).
- Special konst BP `0xE0–E7` (intentionally immediate).
- Indexed XF guest reads and SSBO upload path.
