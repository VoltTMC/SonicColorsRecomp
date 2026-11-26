# A5f — KColorSel Dirty / Flush

## SNCP8P behaviour

`GXSetTevSwapMode` (`@8036D2B0`) packs ras/tex swap into KSEL BP words (`0xF6–0xFD`) and emits them immediately through the FIFO/BP path. Guest dirty at `__gx+0x5FC` uses `|= 3` for related TEV bookkeeping; `__GXSetDirtyState@8036A380` bit0 → texcoord-scale helper, bit1 → NOP. Classic “defer KSEL color-sel until flush” is **not** how the SDK writers work for SwapMode — the BP write itself is the commit.

## Host model (A5f)

Under `SCR_A5F_DEFERRED_BP=1`:

1. Each KSEL BP write still updates `g_gx.bp[0xF6+i]` immediately (canonical).
2. `GxDefer_MarkBp` sets `Ksel|Tev` dirty and invalidates the cached `PSKey`.
3. `Render_OnBP` is **not** called (no immediate handler for `0xF6–0xFD`).
4. Empty `Render_Flush` is skipped when no pending geometry.
5. Next draw `BuildPSKey` reads live `BP(0xF6+i)` into `PSKey.ksel[]` and clears dirty.

Repeated identical ordinary KSEL writes remain `bpNop` (A5b) when the shadow is valid.

## Equivalence

Immediate mode rebuilds derived keys on every Flush path that builds keys; deferred mode rebuilds only when dirty. At every consuming draw, `PSKey.ksel[]` must match the current BP shadow — verified by `scr_a5f_bp_deferred_state_test` cases for multi-KSEL → draw, two draws with intervening KSEL, and mixed konst+TEV.

## Display lists

Recording: still `PPC_ORIGINAL`. Playback: FIFO-ordered `GX_ApplyBpWord`; deferred only coalesces host derived work, never reorders or drops KSEL writes.
