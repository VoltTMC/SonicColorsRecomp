# A5i — Indexed XF Guest-Read Profile

## Path (shared by FIFO decode and A5g typed `XfIdx`)

```
opcode / typed cmd
  → GX_ApplyIndexedXf(array, index, size, addr)
      CP base/stride → guestEa = base + index * stride
      PhysPtr(guestEa)
      [A5h] optional empty Render_Flush skip
      PoseInterp override (opt-in debug) or guest BE→host words → g_gx.xfmem
      A3u_OnIndexedXf (early-out when disabled)
      Render_OnXF → MarkXfDirtyRange (skipped when A5i APPLY + identical)
      A3h_OnIndexedXf (early-out when disabled)
```

## Stages measured (A5i counters)

| Counter | Meaning |
|---|---|
| `idxXfApply` | Apply invocations |
| `idxXfGuestBytes` | `size * 4` per apply |
| `idxXfIdentical` / `idxXfChanged` | Payload vs prior `xfmem` |
| `idxXfGuestNs` | Guest read + endian + xfmem write |
| `idxXfOnXfNs` | `Render_OnXF` wall |
| `idxXfApplyNs` | Apply self-time excluding Flush |

Logged as `A5i-present:` alongside A5h opcode/origin counters.

## Bottleneck ranking (TR1 explore, A5g+A5h playback, A5f off, RTX 3080)

From A5h/A5i matched runs (see `A5I_INDEXED_XF_PERFORMANCE.md`):

1. **Guest apply / `catMs xfIdx`** — multi-second CPU wall for DL-resident idx traffic.
2. **Draw flush / descriptor work** — large, but not indexed-XF-specific.
3. **Vulkan XF memcpy (`xfMemcpyMs`)** — tens of ms; A4n `0x680` + dedup already in place.
4. Empty Flush — already skipped by A5h; not material at matched fifo.

Conclusion: remaining indexed-XF CPU cost is dominated by **guest payload apply + dirty marking**, not GPU upload bandwidth.
