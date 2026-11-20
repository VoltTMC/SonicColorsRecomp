# Sonic Colors (SNCP8P) — Full-game compatibility matrix

Status legend:

| Tag | Meaning |
|---|---|
| `NT` | Not tested in recomp gameplay |
| `OK` | Tested and working |
| `PART` | Partially working / incomplete verification |
| `BROKEN` | Tested and fails |
| `BLOCKED` | Blocked by missing/broken runtime functionality |

Do **not** mark `OK` because assets exist or a HUD element renders. Gameplay must be exercised.

Stage IDs come from extracted set archives (`stg*_obj_*.orc` in the game CPK) and DOL strings. World names follow the original SNCP8P progression.

---

## Boot / system

| Feature | Status | Notes |
|---|---|---|
| Native DOL boot | `OK` | BlackKnightRecomp Linux build |
| Title screen | `OK` | |
| Controller config / WPAD HLE | `OK` | Script `B` → WPAD `0x0400`; physical `wii_b` bind (config) |
| Persistent NAND saves | `OK` | |
| Language select | `OK` | |
| Pause menu (in-stage) | `OK` | Phase 5 |
| HOME / RSO | `NT` / `BLOCKED` | Deferred (needs RSO) |
| 30 FPS default | `OK` | Guest wait=2 ≈ 30 Hz (native) |
| 60 FPS (`--60fps` / `frame_rate=60`) | `OK` (TR1 + AP1 spiral code) | True wait=1 + spring gate + SpiralPipe input/collide gates. See `docs/FRAME_RATE.md`. |
| Guest-60 without gate (`SCR_SPRING_CD_GATE=0`) | `BROKEN` | Wait=1 without gate soft-locks WideSpring. |
| Uncapped (`--frame-rate 0`) | `PART` | VI EQ-spin bypassed (0 spins). `--perf` shows ~6 ms in-wait + ~11 ms gap ≈ 16.7 ms (host frame cost / unidentified gap time — not a second frame-loop VI spin). See PROGRESS Phase 8 |

---

## Worlds and acts

### Tropical Resort

| Stage | ID | Status | Notes |
|---|---|---|---|
| Act 1 | `stg110` | `PART` | Playable: movement, rings, HUD, audio, pause, White Wisp pickup. See open issues below. |
| Act 2 | `stg120` | `NT` | Set data present in CPK |
| Act 3 | `stg130` | `NT` | |
| Act 4 | `stg140` | `NT` | |
| Act 5 | `stg150` | `NT` | Partial set extract seen |
| Act 6 | `stg160` | `NT` | |
| Boss / special | `stg190` | `NT` | Set data present |

**Act 1 open issues**

| Issue | Status | Notes |
|---|---|---|
| White Wisp → gauge | `OK` | `ReleaseBoxSmall #46` @ ≈(-7299.9, 342.0, 4583.9); jump/collide; 3 Wisps release + home; gauge fills; B tutorial |
| Boost (hold Wii B) | `PART` | WPAD B + `CPluginStateBoost` enter (`800888CC`/`sn_boost`) confirmed. Upright+gauge before `DashPanel #1` OK (Phase 9g). Clean run-boost + gauge drain **not yet verified** (needs forward stick + B; plugin `+0x14/+0x18` are scratch, not gauge). See `docs/PROGRESS.md` Phase 9 |
| Cyan Laser | `NT` | Assets / phantom collisions in TR1 later; not routed |
| Yellow Drill | `NT` | Later worlds |
| Intermittent black world (HUD stays) + soft-lock | `KNOWN DEFERRED` | Stomp-triggered (TR1/TR3; not spring-specific). Restart recovers. Phase 20 fix/diag attempts reverted for stability. Optional Phase 19 diag only: `SCR_GX_XFB_DIAG=1`. **Do not treat as an active fix target.** |
| Spring infinite re-trigger at guest-60 | `FIXED` (TR1 WideSpring) | WideSpring collide had no activate cooldown; `frame_rate=60` uses wait=1 + 0.5s activate hold. See `docs/FRAME_RATE.md`. |
| Aquarium Park Act 1 opening spiral (wait=1) | `DEFERRED` | **`stg510`**. Experimental spiral gates removed. Do not resume without an explicit new plan. |
| Other Dolphin/DX 60fps issues (AP1 loop, AC2/AC4, TV S-rank boost, …) | `AUDIT` | See `docs/FRAME_RATE_60FPS_COMPATIBILITY_AUDIT.md` — mostly POTENTIAL/UNVERIFIED (stages NT). |

### Sweet Mountain

| Stage | ID | Status |
|---|---|---|
| Early acts | `stg210`, `stg220`, … | `NT` |

### Later story worlds (IDs from DOL / CPK; not all sets extracted locally)

| World | Typical IDs | Status |
|---|---|---|
| Starlight Carnival | `stg3xx` (e.g. `stg320` set seen) | `NT` |
| Aquarium Park | `stg5xx` (`stg510` Act 1) | `PART` | Act 1 = stg510 (`qua` BGM). Opening spiral at wait=1 deferred. |
| Asteroid Coaster | `stg5xx` | `NT` |
| Planet Wisp | `stg6xx` | `NT` |
| Terminal Velocity | `stg7xx` / `stg720`, `stg790` | `NT` |
| Game Land | `stgA10`–`stgG10` | `NT` |
| Misc / miss / data | `stg901`, `stgmiss`, `stgdata` | `NT` |

---

## Wisp abilities

| Ability | Status | Notes |
|---|---|---|
| White (Boost energy) | `PART` | Collection + gauge `OK`; enter path `OK`; drain/terminate `NT` pending one forward+B capture or component-`0x19` gauge locate |
| Cyan Laser | `NT` | `CPosturePhantomLaser` / `obj_laser` strings present |
| Yellow Drill | `NT` | `CPosturePhantomDrill` present |
| Orange Rocket | `NT` | |
| Pink Spikes | `NT` | |
| Blue Cube | `NT` | |
| Purple Frenzy | `NT` | |
| Green Hover | `NT` | |
| Violet Void | `NT` | Final / special |

Related addresses / strings (DOL): `CPluginStateBoost` `0x80714ba8`, `obj_pixieget` `0x8071721c`, `sn_boost` `0x80714bbc`, `CObjReleaseBoxSmall` `0x8077d158` (xrefs `0x802d5e04`, `0x802d6ed0`).

---

## Menus / flow

| Feature | Status | Notes |
|---|---|---|
| Stage select / hub | `OK` | Enough to reach TR1 |
| Cutscenes | `NT` | |
| Results / rank screen | `NT` | |
| Special Stage / Red Star Rings | `NT` | HUD slots render empty in TR1 |
| Credits | `NT` | End-goal |

---

## Save / load / transitions

| Feature | Status | Notes |
|---|---|---|
| Save create / load | `OK` | Persistent under user dir |
| Stage load TR1 | `OK` | |
| Act→act transition | `NT` | `stg120` assets present; no clear-save locally — full TR1 clear deferred (testing efficiency) |
| Death / restart | `PART` | Restart restores black-screen cases; not fully matrixed |
| Game over | `NT` | |

---

## Reproduction — White Wisp (TR1)

```bash
./out/SonicColorsRecomp --data game/SNCP8P --skip-launcher --nobox --mute --60fps \
  --input-debug --input-script /tmp/scr_phase8_wisp.txt \
  --shots 15 --shots-start 1800
```

Approach the glass walkway **before** the WELCOME arch; jump into the red/yellow capsule (avoid relying on dash/booster routes that skip collision). Hold logical `B` (Wii B / WPAD `0x0400`) after the B tutorial appears.

Evidence: `out/shots/phase8_capsule_before.png`, `phase8_wisps_released.png`, `phase8_gauge_after_collect.png`, `phase8_collect_wisps_b_prompt.png`, `phase8_absorb_wisp.png`.

---

## Next testing order

1. **One** forward+B boost capture before `DashPanel #1` with `--boost-debug` (or locate gauge via component `0x19` / `fn_80088D28` in code).
2. Act 2 (`stg120`) only with a saved clear or short end-of-act script. (Black-screen H7 is deferred — do not prioritise RCA.)
4. First Cyan Laser spawn when routed.
5. Expand matrix world-by-world; never mark `OK` from asset presence alone.
