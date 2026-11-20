#!/usr/bin/env bash
# Capture Music Library screenshots at several window sizes (Phase 17 layout checks).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/out/ui_shots/music"
BIN="$ROOT/out/SonicColorsRecomp"
mkdir -p "$OUT"
if [[ ! -x "$BIN" ]]; then
  echo "missing $BIN — build first" >&2
  exit 1
fi

sizes=(
  "800x600"
  "1280x720"
  "1366x768"
  "1920x1080"
  "2560x1440"
)

for sz in "${sizes[@]}"; do
  echo "== Music Library @ $sz =="
  env -u DISPLAY_FAKE \
    SCR_UI_SIZE="$sz" \
    SCR_UI_SHOT="$OUT/music_${sz}.bmp" \
    SCR_UI_SHOT_TAB=music \
    SCR_UI_LAYOUT_CHECK=1 \
    SCR_UI_LAYOUT_CHECK_LOG=1 \
    "$BIN" || true
done

echo "shots in $OUT"
ls -la "$OUT" || true
