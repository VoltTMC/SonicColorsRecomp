# Performance measurement

Repeatable checks for Sonic Colors Recomp. Prefer frame-time consistency over peak FPS.

## Host FPS overlay

- System → **Show FPS overlay**, or CLI `--show-fps` (this run only).
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

## What not to change

- Do not alter gameplay timing / physics to raise FPS.
- Do not treat average FPS alone as success; watch 1% lows and hitch spikes when measurable.
