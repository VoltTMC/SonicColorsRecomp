#!/usr/bin/env bash
# Build a shareable Linux test folder + archive (no game data).
# Output: dist/SonicColorsRecomp-<ver>-linux-x86_64[.tar.zst|.tar.gz]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

./tools/package_linux.sh -S

VER=$(git describe --tags --always --dirty 2>/dev/null || echo dev)
NAME="SonicColorsRecomp-${VER}-linux-x86_64"
DEST="$ROOT/dist/$NAME"
rm -rf "$DEST"
mkdir -p "$DEST"
cp -a "$ROOT/dist/AppDir/." "$DEST/"

cat >"$DEST/TESTING.txt" <<'EOF'
Sonic Colors Recomp — Linux test build (no game data included)

You must own Sonic Colors (Wii) Europe / PAL SNCP8P (USA/Japan discs are not supported yet).
Use the in-launcher installer, RecompLauncher Easy Setup, or wit EXTRACT to game/SNCP8P/.

Run the launcher:
  ./AppRun

Or:
  ./usr/bin/SonicColorsRecomp

First launch: Install / locate extracted data, then Play.
Config / saves / logs: ~/.local/share/SonicColorsRecomp
  (or next to the binary with --portable / SCR_PORTABLE=1)

Useful flags:
  ./AppRun --portable
  ./AppRun --list-audio-devices
  ./AppRun --check-updates
  ./AppRun --save-diagnostics
  ./AppRun --help
  ./AppRun --version

Do not redistribute copyrighted game files with this package.
EOF

chmod +x "$DEST/AppRun" "$DEST/usr/bin/SonicColorsRecomp"

ARCHIVE=""
if command -v zstd >/dev/null 2>&1; then
  ARCHIVE="$ROOT/dist/${NAME}.tar.zst"
  rm -f "$ARCHIVE"
  tar -C "$ROOT/dist" -I 'zstd -19' -cf "$ARCHIVE" "$NAME"
else
  ARCHIVE="$ROOT/dist/${NAME}.tar.gz"
  rm -f "$ARCHIVE"
  tar -C "$ROOT/dist" -czf "$ARCHIVE" "$NAME"
fi

echo "test folder: $DEST"
echo "archive:     $ARCHIVE ($(du -h "$ARCHIVE" | awk '{print $1}'))"
echo "Send the archive. Recipients: tar xf <archive> && cd $NAME && ./AppRun"
