#!/usr/bin/env bash
# Seed an isolated disposable NAND for A2g stage-coverage runs.
# Copies a verified in-tree save fixture into out/test_nand/ (gitignored).
# Never writes to userdata/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${SCR_A2G_SAVE_SRC:-$ROOT/userdata/title/00010000/534e4350/data}"
DEST="${SCR_TEST_NAND_DIR:-$ROOT/out/test_nand}"
TITLE_DATA="$DEST/title/00010000/534e4350/data"

if [[ ! -d "$SRC" ]]; then
  echo "error: save source missing: $SRC" >&2
  echo "  Provide game_*.sav + system.sav under userdata (or set SCR_A2G_SAVE_SRC)." >&2
  exit 1
fi
if [[ ! -f "$SRC/game_0.sav" && ! -f "$SRC/system.sav" ]]; then
  echo "error: no game_0.sav/system.sav in $SRC" >&2
  exit 1
fi

rm -rf "$DEST"
mkdir -p "$TITLE_DATA" "$DEST/sys" "$DEST/shared2/sys" "$DEST/tmp" "$DEST/meta"
# Minimal banner/sys stubs if present in source tree.
cp -a "$SRC"/. "$TITLE_DATA"/
# Ensure writable for game flushes.
chmod -R u+rwX "$DEST" 2>/dev/null || true

echo "A2g test NAND ready: $DEST"
echo "  title data: $TITLE_DATA"
ls -la "$TITLE_DATA" | head -20
echo "Use: --nand $DEST   or   SCR_TEST_NAND_DIR=$DEST"
