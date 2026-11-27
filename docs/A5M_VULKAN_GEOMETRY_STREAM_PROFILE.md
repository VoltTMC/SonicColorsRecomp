# A5m — Vulkan geometry streaming profile

## Path

```
FlushVulkan → GfxDrawBatch { verts*, indices* }  (HostVertex / uint32)
  → VkGx_Draw
       align vbPos to sizeof(HostVertex)
       if vb/ib would overflow → flushFrame(Geom)
       … tex / XF / descriptors (may flushFrame Xf|Desc) …
       memcpy verts → frame.vb.mapped + vbOff   // host-visible coherent
       memcpy indices → frame.ib.mapped + ibOff
       bind VB/IB offsets → DrawIndexed
```

No geometry staging buffer. Rings are `HOST_VISIBLE | HOST_COHERENT` bump allocators (`s_arenaBytes` per VB and IB, × `kFrames`).

## Ownership

| Resource | Scope | Reclaim |
|---|---|---|
| `FrameSlot.vb` / `.ib` | Per slot, linear bump | Mid-frame flush wait → pos=0; or present retire / EnsureFrameRecording |
| XF SSBO ring | Shared | Full flush / DrainFrameSlot / present |
| dynUbo + desc sets | Shared | Full flush / ResetFrameDescriptors |

Mid-frame `flushFrame` submits the current CB and **CPU-waits** its fence before reusing any range written into that CB. GPU ownership ends only after that wait.

## Flush reasons (instrumented)

| Counter | Trigger |
|---|---|
| `flushGeom` | `vbOff+vbytes > vb.size` or `ibPos+ibytes > ib.size` |
| `flushXf` | XF ring slot would not fit |
| `flushDesc` | dynUbo or descriptor set pool exhausted |

Legacy behaviour: every reason resets VB/IB **and** XF/desc. A5m soft geom path (opt-in) resets only VB/IB after the wait.

## Copy model

One host `memcpy` per draw into the mapped ring (already GPU-visible). Index format is always `uint32`. Alignment padding on VB is tracked as `geomVbPad`.
