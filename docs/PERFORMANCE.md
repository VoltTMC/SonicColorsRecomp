# Performance measurement

Repeatable checks for Sonic Colors Recomp. Prefer frame-time consistency over peak FPS.

## Host FPS overlay

- System → **Show FPS overlay**, or CLI `--show-fps` (this run only).
- Shows smoothed FPS, instantaneous frame time (ms), approximate 1% low
  (reciprocal of the slowest frame in a 120-frame window), smoothed Present()
  wall time (includes VSync wait), D3D11 GPU timestamp for the present blit
  (one frame late; when the query resolves), 3D draw calls since the last XFB
  copy, guest FrameLoopWait Hz, process RSS (MiB), and cumulative audio underruns.
- Reports host present rate (ImGui delta), not guest VI wait alone.

## Linux (native)

```bash
# Build
./scripts/cmake.sh --build --preset linux-clang -j$(nproc)

# Boot to title with FPS (needs installed game data)
./out/SonicColorsRecomp --skip-launcher --data game/SNCP8P --show-fps --mute

# Optional: log CPU samples while playing (external)
# pidstat -u -p $(pgrep -n SonicColorsRecomp) 1
```

Record for ~60s in a quiet stage (e.g. Tropical Resort Act 1):

| Metric | How |
|---|---|
| Average FPS | Overlay reading / status log |
| Stutter | Visual + log underruns (`g_audioUnderruns`) |
| Memory | `ps -o rss= -p <pid>` |
| Startup | Log line `startup: guest entry ready in N ms` (process start → guest entry) |

## Windows

- Cross-build: `./scripts/cmake.sh --build --preset windows-mingw`
- Native verification: `docs/windows-native-checklist.md` (audio, DPI, pads).
- Wine smoke does **not** prove native frame pacing.

## Launcher

| Metric | Target approach |
|---|---|
| Cold start | Time to first launcher frame |
| Music Library | Time to list tracks after open |
| Resize | No blurry intermediate frames; UI stays interactive |
| Size restore | Quit after resize; reopen — size matches `launcher_width/height` |

### HiDPI / resize smoke (manual)

1. Start launcher at 100% / 125% / 150% desktop scale.
2. Resize continuously for ~10 s; text stays sharp; no frozen controls.
3. Maximise / restore; Play and Music tabs remain usable.
4. Quit and relaunch; window size restored from config.

## What not to change

- Do not alter gameplay timing / physics to raise FPS.
- Do not treat average FPS alone as success; watch 1% lows and hitch spikes when measurable.
