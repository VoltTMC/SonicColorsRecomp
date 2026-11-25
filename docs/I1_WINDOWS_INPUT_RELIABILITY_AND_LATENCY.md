# I1 — Windows Input Reliability & End-to-End Latency

**Status (2026-11-25):** Short press/release **transition loss** addressed with a host edge-latch layer. Automated latch tests pass. Linux + MinGW builds pass. Wine `--list-gamepads` smoke OK. **Native Windows physical keyboard/controller gameplay remains required for full sign-off and is UNVERIFIED on this machine.**

**Baseline:** `e87f431` (A4j on `main`). Prior Windows pad freshness fix: `a4aac0a` / `docs/WINDOWS_XBOX_INPUT_LATENCY.md`.

## 1. Original input architecture

| Layer | Implementation |
|---|---|
| Windows window | Dedicated Win32 thread (`GetMessage`); game on another thread |
| Linux window | Dedicated SDL event thread (`WaitEventTimeout`) |
| Keyboard | Win: `GetAsyncKeyState`; Linux: `SDL_GetKeyboardState` |
| Gamepad | Shared SDL3 (`SdlGamepad_Read` + `SDL_UpdateJoysticks`) |
| Host publish | `Gamepad_Poll` → bindings / dead zones / keyboard P1 |
| Guest | WPAD/KPAD HLE and PAD/SI HLE each call `Gamepad_Poll` |

## 2. Host→guest pipeline

```
OS event (WM_KEY*/SDL_KEY* / XInput·HIDAPI level)
  → InputEdges_KeyDown/Up  (window thread; no auto-repeat)
  → InputEdges_NotePadButtons / SamplePads  (game thread; FrameLoopWait + poll)
  → sticky rising edges held until guest-frame publish
  → Gamepad_Poll (per guest frame snapshot shared by WPAD + PAD)
  → WPADRead / KPADReadCore / PADRead
  → guest gameplay
```

| Stage | Thread | Rate | Mode |
|---|---|---|---|
| Win32 / SDL window events | Window | OS | Transitions |
| `GetAsyncKeyState` / keyboard state | Game | Per `Gamepad_Poll` | Level |
| `SDL_UpdateJoysticks` | Game | Poll + FrameLoopWait | Level |
| Edge latch | Shared (mutex) | On sample / event | Sticky until publish |
| `Gamepad_Poll` | Game (guest HLE) | ~VI / read | Snapshot |
| WPAD / PAD HLE | Game | Guest-driven | Consumes snapshot |

## 3. Windows-specific path

- Gamepad: SDL3 GAMEPAD only (no Raw Input added). Hotplug `PumpEvents` still throttled ~250 ms; **freshness** via per-read `UpdateJoysticks` (A4aac0a).
- Keyboard: level poll was insufficient for sub-frame taps; **WM_KEYDOWN/UP** now latch sticky VKs.
- Focus loss / minimize: `InputEdges_OnFocusChange(false)` clears stickies and pad held state (no stuck buttons).
- Present: `MaxFrameLatency` default remains 3 (`SCR_MAX_FRAME_LATENCY` override). Not changed blindly for iGPU Present blocking.

## 4. Root cause — dropped presses

**Exclusive current-state sampling.** A press and release that both finish between two `Gamepad_Poll` samples never appear in `GetAsyncKeyState` / `SDL_GetGamepadButton`. Guest sees neither down nor a distinct edge. This matches “short taps skipped.”

Not caused by Wii semantics, guest sim rate, or Vulkan.

## 5. Root cause — remaining delay

Separate from misses:

1. Guest samples at most once per sim/VI step → up to ~1 frame of inherent delay.
2. Acquire→publish was previously worsened by 250 ms stale pads (fixed in A4aac0a).
3. DXGI `MaxFrameLatency=3` can add display queue delay under load (opt-in lower via env).

Software timestamps (`--input-latency`) measure acquire→publish only, not button-to-photon.

## 6. Transition-preservation design

`runtime/input/input_edges.*`:

- Pad: rising `(live & ~prev)` → `sticky`; publish `live | sticky` once per `Perf_GuestFrameIndex`; clear sticky after fold.
- Key: `KeyDown` sets sticky VK; `KeyEffectiveDown` = live OR sticky; `PublishKeys` clears stickies once per guest frame.
- WPAD and PAD in the same guest frame share one published pad mask (no double-consume).
- Bounded: sticky is fixed-size (`uint16_t` per slot + 256 VK bytes). Overflow counter reserved (no unbounded queue).
- Focus clear prevents stuck holds.

## 7. WPAD/KPAD and PAD/SI

No HLE semantic changes. Both still call `Gamepad_Poll`. Same-frame consistency via `InputEdges_PublishPad` frame index.

## 8. Threading

- Window thread: key/mouse edge latch only (mutex).
- Game thread: pad sample, publish, HLE.
- No blocking wait for devices on the guest thread.

## 9. Frame-loop interaction

`InputEdges_SamplePads()` runs:

- At `FrameLoopWait` entry (before guest tick).
- During uncapped pace sleeps (between presents).

So edges arriving during Present/wait are latched before the next WPAD/PAD read.

## 10. Automated tests

`scr_input_edges_test`:

| Case | Result |
|---|---|
| Pad short press+release before publish (A/B/X/Y/START) | OK |
| Hold without duplicate rising edges | OK |
| Key short tap with live=false | OK |
| WPAD+PAD same guest frame equal snapshot | OK |
| Focus loss clears sticky | OK |

Synthetic durations 1–16 ms class are covered by the press+release-before-publish model (not wall-clock injected ms).

## 11. Native Windows runtime

| Check | Result |
|---|---|
| MinGW `SonicColorsRecomp.exe` build | OK |
| Wine `--list-gamepads` | OK (0 pads in CI host; SDL backend ready) |
| Physical Win10/11 keyboard menus/gameplay | **UNVERIFIED** |
| Physical Xbox / DS pad short taps | **UNVERIFIED** |
| Focus alt-tab / reconnect on native Windows | **UNVERIFIED** |

Procedure for hardware sign-off: `docs/CONTROLLER_INPUT.md` + run with `--input-i1 --input-latency --perf`.

## 12. Linux regression

- Build OK; D3D11/DXVK smoke with `--input-i1` enabled latch logs.
- Edge latch active for SDL key events (parity with Windows intent).
- Existing controller mappings unchanged.

## 13. Latency percentiles

Use `--input-latency` / `SCR_INPUT_LATENCY=1` (existing). Reports acquire-interval p50/p95/p99 and acquire→publish. Re-measure on native Windows with a pad; not claimed improved beyond A4aac0a freshness + earlier sampling in FrameLoopWait without physical data here.

## 14. Known limitations

- Sticky shows a short tap as **one guest-frame down**; ultra-fast double-taps within one guest frame may coalesce (Wii-typical).
- Analog sticks/triggers are still level-only (no edge latch).
- Native Windows button-to-photon **UNVERIFIED**.
- AMD/RADV deferred; native Windows Vulkan GPU deferred.

## Diagnostics

| Switch | Role |
|---|---|
| `--input-i1` / `SCR_INPUT_I1=1` | Edge latch logs + counters |
| `--input-latency` / `SCR_INPUT_LATENCY=1` | Acquire→publish timing |
| `--input-debug` | Host/WPAD/PAD button transition log |
| `SCR_MAX_FRAME_LATENCY` | DXGI present queue (1–8) |
