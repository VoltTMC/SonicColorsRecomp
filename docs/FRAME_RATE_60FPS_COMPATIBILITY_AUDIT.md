# 60 FPS Compatibility Audit (SNCP8P)

**Date:** 2026-11-20  
**Scope:** Investigation only — no gameplay fixes in this change.  
**Recomp modes (current):**

| Mode | Wait | Notes |
|------|-----:|-------|
| `--frame-rate 30` | 2 | Original baseline |
| `--60fps` / `frame_rate=60` | **1** | True guest-60 + spring CD gate (launcher default) |
| `SCR_SPRING_CD_GATE=0` | 1 | Same wait; spring gate off (Dolphin-AR-like breakage) |

> Earlier docs described “default 60 = wait=2 (sim30)”. That policy was superseded when the WideSpring gate landed: launcher 60 is now wait=1 with gate. This audit uses **current** behaviour.

**Shared guest change vs Dolphin AR:** codes `008F2D97` / `0090DB0B` force wait LSB=1 on SDA `0x808F2D94` and frame-object `+0x8`. Recomp `Perf_WaitFields()` → `1` + `FrameLoopWait` republish is the same guest effect. `fn_803E19B0` sets float dt at SDA `0x808F2D98` to `N/60`.

---

## Sources

| Source | URL | Role |
|--------|-----|------|
| Dolphin Wiki — Sonic Colors (Enhancements / 60FPS) | https://wiki.dolphin-emu.org/index.php?title=Sonic_Colors | Official known problems (wiki PoW-blocked here; text corroborated via mirrors) |
| Wiki content mirror | https://lanadead.weebly.com/sonic-colors.html | Same 60FPS problem list + AC2 “point shown below” |
| Wiki content mirror | https://cooloup389.weebly.com/sonic-the-hedgehog-3d-mac-os.html | Same list; AC2: “Using Boost should allow getting past” |
| Sonic Colors DX (GameBanana) | https://gamebanana.com/mods/325588 | Community / mod known issues |
| Sonic Colors DX (SHC2021) | https://shc.zone/entries/expo2021/584 | DX 60fps issues + AP1 start-skip workaround |
| SNCP8P DOL / decomp | `/home/volt/Decomp/sonic2010-sncp8p` (read-only) | Guest classes / wait / dt |
| Recomp runtime | `runtime/perf.cpp`, `runtime/debug/spring_60.cpp` | Wait + spring gate |

**Evidence grades used below:** Official Dolphin wiki · DX community · Code inspection · Recomp playtest.

---

## Shared timing model (why these bugs cluster)

```
N = wait fields (native steady-state 2; AR / true-60 → 1)
dt = N / 60   // fn_803E19B0 → SDA 0x808F2D98
```

At wait=1 the guest loop runs **twice as often** for the same wall time, with **half** the published dt.

| Kind of logic | Wait=1 effect |
|---------------|---------------|
| Motion integrated with `* dt` | Roughly wall-time correct |
| Per-frame counters / one-shot flags / “arm then clear next update” | Fire **2×** as often → re-triggers, shorter windows |
| Scripted path / posture stepped per update | Path sampling / control windows compress |
| Collision “while overlapping, activate every update” | Soft-lock (WideSpring) |

Relevant guest systems (DOL strings):  
`CPluginPostureForcePath`, `CPluginPostureAutoMoveOnPath`, `CPostureInputOnPath*`, `CObjDashPanel`, `CObjJumpBoard`, `CObjGrindBooster`, `CStateHomingAttack`, `CPosturePhantomLaser*`, `CStateJumpSpring`, `CPluginStateBoost`, boss `CBoss*` family, TV stages `stg720` / `stg790`.

---

## Issue A — Asteroid Coaster Act 2: difficult section

| Field | Detail |
|-------|--------|
| **Status** | **POTENTIAL** (shared guest risk) / **UNVERIFIED** in recomp |
| **Severity** | High (progression friction; Boost workaround) |
| **Stage** | Asteroid Coaster Act 2 (`stg5xx` family; exact set ID not exercised locally) |
| **Source** | Official Dolphin 60FPS section (wiki + mirrors) |
| **Reported** | Section “shown below” (wiki screenshot not recoverable here) becomes hard; **Boost** clears it. Minor physics noted globally. |
| **Expected (30 Hz)** | Completable with normal run / jump timing |
| **Dolphin @60** | Difficult without Boost |
| **Recomp** | Stage **NT** — no in-stage capture. No reason the guest wait=1 change would spare this if it is speed/gap timing. |
| **Guest suspects** | Frame-stepped accel vs slope/`CObjJumpBoard`/`CObjDashPanel`/`CObjGrindBooster`; gravity-flip / pusher-wall timing (Act 2 design: extending walls, gravity flips — wiki/fandom) |
| **Proposed fix (not implemented)** | Identify exact set object from wiki image; compare run speed / airtime at wait=1 vs 2; if underspeed, scale that gimmick’s speed gate or restore wait=2-equivalent integration for that object class |

---

## Issue B — Aquarium Park Act 1: opening scripted loop

| Field | Detail |
|-------|--------|
| **Status** | **DEFERRED** — do not investigate or patch |
| **Severity** | High (death / out-of-bounds; progression) |
| **Stage** | Aquarium Park Act 1 — **`stg510`** (BGM `qua`; was mislabeled `stg410`) |
| **Source** | Dolphin 60FPS wiki; Colors Ultimate TCRF; recomp playtest |
| **Observed (recomp, wait=1)** | Stuck in first loop and/or void death |
| **DX note** | Spawn-skip — **rejected**; not used |
| **Root cause** | Incomplete; prior experimental gates removed from the tree |
| **Fix** | None active. `spiral_60.cpp` and spiral overrides removed. |
| **Scope** | Deferred indefinitely; prefer other 60 FPS / PC work. |

---

## Issue C — Tropical Resort Act 1: jump freeze

| Field | Detail |
|-------|--------|
| **Status** | **UNVERIFIED** |
| **Severity** | Critical **if** real (hard freeze) |
| **Stage** | Tropical Resort Act 1 (`stg110`) |
| **Source** | Official Dolphin 60FPS section — “jump at the right spot” (location unspecified beyond that) |
| **Reported** | Freeze; disable patch for that section |
| **Recomp evidence** | TR1 is the most-played stage. Soft-lock previously seen was **WideSpring re-activate** (fixed), **not** a documented hard freeze on a named jump. Jump-relevant set objects: `JumpBoard`, `DashPanel`, springs, `DestBox`/`IronBox`. Deferred **black-screen** (stomp→box) is a **separate** known issue and was **out of scope** for this audit. |
| **Connection to springs** | **Not established.** Treat as separate until a freeze is captured with a stack / hung guest PC. |
| **Proposed next evidence** | `--perf` + hung-thread dump / guest PC when freeze hits; note camera region / nearby set names |

---

## Issue D — Terminal Velocity boss: inconsistent boost damage

| Field | Detail |
|-------|--------|
| **Status** | **POTENTIAL** / **UNVERIFIED** |
| **Severity** | Medium (S-rank / optional damage; boss still beatable) |
| **Stage** | Terminal Velocity boss (`stg720` / `stg790` in DOL) |
| **Source** | Official Dolphin 60FPS section |
| **Reported** | After attack chain, boost-into-boss for extra damage is inconsistent. Not required to win; **needed for S rank**. |
| **Guest suspects** | Boss i-frames / hurt windows counted in updates; `CPluginStateBoost` contact window half as long in wall-time; collision vs animation state |
| **Recomp** | Boss **NT** |
| **Proposed fix (not implemented)** | Scale boss vulnerable frames by `dt`/wait; or double frame counts when wait=1 so wall-time matches wait=2 |

---

## Issue E — Object flickering during transitions

| Field | Detail |
|-------|--------|
| **Status** | **DOLPHIN-SPECIFIC** (primary) / **UNVERIFIED** in recomp |
| **Severity** | Low (visual) |
| **Source** | DX GameBanana: “(Emulation issue) Very rarely, some objects may flicker to a solid color for a single frame”; SHC: “Occasionally objects will flicker during transitions” under 60fps patch issues |
| **Analysis** | Solid-color one-frame flicker strongly matches EFB/texture-cache / shader compilation behaviour in Dolphin, not SNCP8P guest logic. Transition flicker could also be one-frame visibility before guest draw — still often host-GPU. |
| **Recomp** | Native D3D11/DXVK path; no matching user report in project logs. Not attributed to wait=1 guest code without a capture. |
| **Black-screen** | Explicitly **deferred / out of scope** (not this flicker). |

---

## Issue F — Asteroid Coaster Act 4: ramp needs Boost

| Field | Detail |
|-------|--------|
| **Status** | **POTENTIAL** / **UNVERIFIED** |
| **Severity** | Medium (gimmick requires Boost; originally run-only) |
| **Stage** | Asteroid Coaster Act 4 (`stg5xx`) |
| **Source** | DX GameBanana + SHC2021 — **community / mod docs**, not the Dolphin wiki’s official problem list |
| **Reported** | Specific ramp: run was enough at 30 Hz; Boost required at 60 Hz patch |
| **Guest suspects** | Same class as A: underspeed on slope / `JumpBoard` / insufficient accel frames before gap |
| **Compare plan** | wait=2 run clear vs wait=1 run fail vs wait=1 Boost clear |
| **Proposed fix (not implemented)** | Same as A — wall-time speed / launch thresholds |

---

## Issue G — Tropical Resort Targets mission: laser crystal redirect

| Field | Detail |
|-------|--------|
| **Status** | **POTENTIAL** / **UNVERIFIED** |
| **Severity** | Medium (mission objective; workaround = other crystal) |
| **Stage** | Tropical Resort Targets mission (mission set; not main `stg110`) |
| **Source** | DX GameBanana — **community / mod**, not Dolphin wiki |
| **Reported** | Homing/laser redirect wrong for one laser crystal; target another |
| **Guest suspects** | `CStateHomingAttack` / `CPosturePhantomLaser*` target pick uses per-frame angle/distance; wait=1 changes which candidate wins |
| **Recomp** | Mission **NT** |

---

## Spring soft-lock (recheck)

| Field | Detail |
|-------|--------|
| **Status** | **ALREADY FIXED** (TR1 WideSpring soft-lock) |
| **Severity** | Was Critical; gated |
| **Source** | Recomp diag `guest60c` + user verify; Dolphin/DX also report spring issues |
| **Root cause** | `CObjWideSpring` collide `fn_800B90E0` → activate `fn_800B9768` for collider type `0x9000` **every guest frame** while overlapped. `+0xA8` bit0 is a one-frame arm flag, **not** a re-trigger cooldown. |
| **Fix** | `runtime/debug/spring_60.cpp`: when gate on (default at `frame_rate≥60`), early-return `ObjWideSpringActivate` for ~0.5s guest time per instance; also OR `CObjSpring` `+0xF0` bit15 so native `+0x188` cooldown runs. |
| **Does not touch** | Generic physics, paths, bosses, laser targeting |
| **Residual risk** | DX “stuck between springs without horizontal input” may still exist (spacing / multi-spring). Gate reduces re-fire; **not** proven to fix all spring spacing soft-locks. Ordinary `CObjSpring` / SpringPole lightly exercised. |
| **30 FPS** | Gate off when `frame_rate=30` and no `SCR_GUEST_60` — original wait=2 path unchanged. |

---

## Generic physics / gameplay (focused)

No full playthrough. Code-level prioritisation:

| Area | Risk at wait=1 | Status |
|------|----------------|--------|
| Run/jump integrated with `dt` | Low–moderate | POTENTIAL |
| Boost plugin / gauge drain | Moderate (per-frame drain → 2× wall-time) | POTENTIAL — Boost enter verified in TR1; drain not fully characterised |
| Homing windows | Moderate | POTENTIAL (see G) |
| Moving platforms / pusher walls | High in AC acts | POTENTIAL (see A) |
| Scripted paths / loops | High | POTENTIAL (see B) |
| Boss hurt / i-frame counts | Medium (S-rank) | POTENTIAL (see D) |
| Rank timers (seconds) | Usually wall-time via dt — lower risk | UNVERIFIED |
| Wisp phantoms (Laser/Drill/…) | Untested | UNVERIFIED |

Dolphin’s blanket “minor physics issues” line is accepted as **Official** background: game remains “easily completable” per wiki, with the named exceptions above.

---

## Summary matrix

| ID | Issue | Source | Status | Severity | Affects recomp? |
|----|-------|--------|--------|----------|-----------------|
| A | AC Act 2 hard section | Dolphin wiki | POTENTIAL / UNVERIFIED | High | Likely shared guest; not playtested |
| B | AP Act 1 scripted loop (`stg410`) | Dolphin wiki + Ultimate | **CONFIRMED** | High | Stuck in loop and/or ejected to void/death at wait=1 |
| C | TR Act 1 jump freeze | Dolphin wiki | UNVERIFIED | Critical if real | Unknown; not reproduced |
| D | TV boss boost S-rank | Dolphin wiki | POTENTIAL / UNVERIFIED | Medium | Likely shared guest; not playtested |
| E | Object flicker | DX (emu / transitions) | DOLPHIN-SPECIFIC lean | Low | No recomp evidence |
| F | AC Act 4 ramp needs Boost | DX community | POTENTIAL / UNVERIFIED | Medium | Likely shared guest; not playtested |
| G | Targets laser crystal | DX community | POTENTIAL / UNVERIFIED | Medium | Likely shared guest; not playtested |
| S | Spring soft-lock | Recomp + Dolphin/DX | **ALREADY FIXED** (WideSpring) | Was Critical | Fixed with gate |

**Already fixed:** WideSpring soft-lock at true-60.  
**Confirmed open:** **B** AP1 opening loop (`stg410`) — stuck / void death at wait=1.  
**Dolphin/mod specific:** E (primarily); DX AP1 start-skip is a **mod workaround** only.  
**Not reproduced in recomp:** A, C, D, F–G.  
**Closest to “full 60 FPS compatibility”:** TR1 + gated springs OK; **AP1 loop blocks clean Aquarium progression** until fixed.

---

## Recommended fix priority (do not implement in this audit)

1. **B — AP1 scripted loop (`stg410`) — CONFIRMED** — High; stuck/void death; path posture / input isolation or dt-scaled path advance.  
2. **A — AC2 traversal** — High; progression; identify wiki screenshot object, compare wait=1 vs 2 speed.  
3. **C — TR1 jump freeze** — Critical if confirmed; needs a single hang capture before coding.  
4. **F — AC4 ramp** — Medium; same physics class as A.  
5. **D — TV boss boost window** — Medium; S-rank completeness.  
6. **G — Targets crystal** — Medium; mission-only.  
7. **E — Flicker** — Low; only chase if seen on native GX.  
8. **Spring residual** — Optional: multi-spring spacing stuck (DX); regression suite for `CObjSpring` / Pole / Boost-between-springs.

---

## Testing gaps (explicit)

Local set extracts heavily cover early Tropical Resort (`stg110`…); Aquarium / Asteroid / Terminal Velocity acts are **not** played in this repo’s compatibility matrix (`NT`). Therefore **absence of a recomp bug report ≠ fixed**.

Suggested minimal future captures (each ≤ one short session + log):

1. `stg110` wait=1: hunt Issue C jump; exercise JumpBoard / multi-spring.  
2. Aquarium Act 1 open: no-input vs stick.  
3. Asteroid Act 2 / Act 4: the reported gap/ramp with/without Boost.  
4. Terminal Velocity boss: post-chain Boost hits for S-rank.  
5. TR Targets mission: laser crystal picks.

---

## Conclusion

True 60 FPS in this recomp matches the Dolphin AR **guest** change (wait=1 + dt). The **WideSpring soft-lock is fixed** and is the only Dolphin-class 60 FPS gameplay bug with **CONFIRMED → FIXED** evidence here.

Most other wiki/DX issues are **credible shared guest timing bugs** that remain **UNVERIFIED** because those stages were never exercised. Object flicker is **unlikely** to be a guest wait=1 defect. Full “gameplay-compatible 60 FPS” (all acts, missions, S-ranks) is **not** claimed until A/B/C/D/F/G are either reproduced and fixed or systematically cleared with wait=1 vs wait=2 captures.
