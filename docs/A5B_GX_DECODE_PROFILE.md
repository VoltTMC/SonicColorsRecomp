# A5b — GX RunCommands Decode Profile

**Date:** 2026-11-25  
**Baseline:** A5a `b87768c`  
**Workload:** TR1 explore (`tools/a4k_tr1_explore.txt`), RTX 3080, `SCR_VK_DEVICE=3080`, ~75–90s, validation off.  
**Env:** `SCR_A5A_PROF=1 SCR_A5B_PROF=1`

## Methodology

A5a reported ~47s cumulative `decodeMs`. That figure was **inclusive wall time of every `RunCommands` invocation**, including nested display-list bodies, so nested DL work was **double-counted** into the sum.

A5b splits:

| Counter | Meaning |
|---|---|
| `topDecodeMs` | Wall time of **top-level** (`!inDisplayList`) `RunCommands` only |
| `nestDecodeMs` | Wall time of **in-DL** `RunCommands` bodies |
| `catMs.*` | Self-ish handler walls: CP / XF imm / XF indexed / BP / DL-call / `Render_Draw` |
| `bpNop` | Identical non-special BP writes skipped (A5b decoder opt) |

Category times are **not** additive with `topDecodeMs` (DL-call includes nested decode; handlers run inside both top and nested walks).

## Dominant cost (legacy-class run, `out/a5b_decode`)

End-of-run sample:

| Metric | Value |
|---|---:|
| fifoIn | ~47.4M |
| topDecoded | ~47.4M |
| dlDecoded | ~30.5M |
| cmds | ~3.08M |
| bp / bpNop | ~1.49M / ~0.51M |
| xf (imm+idx opcodes) | ~0.66M |
| draws | ~0.74M |
| topDecodeMs | ~41.6s |
| nestDecodeMs | ~8.3s |
| gatherMs | ~0.8s |

Handler category ms (same sample):

| Category | ms | Share of (cp+xfImm+xfIdx+bp+draw) |
|---|---:|---:|
| **BP** | **31563** | **76.5%** |
| XF indexed | 7183 | 17.4% |
| XF immediate | 1813 | 4.4% |
| Draw (`Render_Draw`) | 520 | 1.3% |
| CP | 168 | 0.4% |
| DL-call (inclusive) | 8344 | (overlaps nestDecode) |

### Verdict

1. The A5a ~47s number was largely **real host work**, but inflated by nested DL double-counting; top-level alone is still ~40s-class on this workload.
2. **BP register processing dominates** decoder handler time — not draw submission and not immediate XF.
3. ~25–34% of BP opcodes are **identical rewrites** (`bpNop`) that can skip flush/OnBP safely for non-special regs.
4. Immediate XF (A5a/A5b adapters) is a minority of XF opcodes; most XF cost is **indexed XF** + DL-resident packets.
5. `Render_Draw` self-time is small vs BP; vertex work is not the primary “decode” cost under this instrumentation.

## Implications for adapters

| Opportunity | Why |
|---|---|
| Keep Pos/Nrm/TexMtxImm direct | Proven; removes encode+imm decode for those SDK calls |
| BP SDK fast paths (blend/TEV/Z/…) | Highest remaining CPU; need `__gx` shadow + DL-record discipline |
| Indexed XF | Hot, but array/base/stride + guest RAM — harder than imm |
| DL predecode cache | Only if guest memory invalidation is airtight; deferred |

## Opt-in knobs

- `SCR_A5A_DIRECT=1` — Pos/Nrm/TexMtxImm typed XF  
- `SCR_A5A_PROF=1` / `SCR_A5B_PROF=1` — counters + category ms  
- Bench: `tools/a5b_tr1_bench.sh legacy|direct|decode|compare`
