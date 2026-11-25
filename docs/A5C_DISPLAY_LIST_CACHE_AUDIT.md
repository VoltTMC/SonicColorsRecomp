# A5c — Display-list decode cache feasibility

## A5b baseline (self vs inclusive)

| Measurement | Approx |
|---|---:|
| Top-level decode wall | ~42 s / profiled run |
| Nested (in-DL) decode wall | ~8 s |
| BP handler self-time share | ~76% |
| Indexed XF self-time | ~17% |
| Immediate XF | ~4% |
| `Render_Draw` | ~1% |

Nested DL decode is real but secondary to BP processing. A5c prioritised BP shadow + direct adapters over a DL cache.

## What a correct cache would need

- Guest list address + length
- Content generation / hash of list bytes
- Nested DL dependency set (addresses + generations)
- Invalidation on any guest write overlapping a cached list or nested dependency
- Preservation of BP/CP/XF write **order** and side-effecting registers
- No reuse of a “final image” — only a decoded command representation, if anything

A cached render target is **not** equivalent to cached commands.

## Observed patterns (A5b/A5c tooling)

- DLs are frequent (`dlCalls` in `GfxNativeStats`).
- Nested calls exist (nest decode ~8 s).
- Many lists are short; some are reused across frames, but game code can rewrite RAM.
- BP/TEV state inside lists still needs ordered apply; caching must not collapse special writes.

## Decision (A5c)

**Do not implement a general display-list cache in A5c.**

Rationale:

1. Dominant CPU cost remains BP apply (~76% self-time); direct BP adapters + ordinary nop address that first.
2. Reliable invalidation for nested lists + guest mutation is non-trivial and high risk for silent wrong draws.
3. Equivalence tests would need content-hash + nested dependency tracking before any ship.

## Expected benefit if done later

Upper bound roughly nest-decode wall (~8 s in A5b samples) plus some top-level DL-call overhead — **not** the full 42 s top decode. Worth a dedicated milestone only after BP/XF front-end costs are further reduced and an invalidation design is proven.

## Status

Feasibility documented; cache **unimplemented**. AMD/RADV deferred. Native Windows Vulkan unverified.
