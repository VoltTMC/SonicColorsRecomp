#!/usr/bin/env bash
# Refresh dist/windows-build-src + zip for copying to a Windows build machine.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/dist/windows-build-src"
rm -rf "$DEST"
mkdir -p "$DEST"
rsync -a \
  --exclude '.git/' --exclude 'build/' --exclude 'out/' --exclude 'dist/' \
  --exclude 'game/' --exclude 'external/' --exclude 'userdata/' --exclude 'nand/' \
  --exclude 'shadercache/' --exclude 'textures/' --exclude '.venv/' --exclude '.venv-music/' \
  --exclude '.cache/' --exclude '__pycache__/' --exclude '*.log' --exclude 'config.ini' \
  --exclude 'mods/music/tracks/' --exclude 'mods/music/manifest.toml' \
  "$ROOT/CMakeLists.txt" "$ROOT/CMakePresets.json" "$ROOT/README.md" "$ROOT/.gitignore" \
  "$ROOT/cmake" "$ROOT/runtime" "$ROOT/tools" "$ROOT/generated" "$ROOT/data" "$ROOT/docs" \
  "$ROOT/licenses" "$ROOT/scripts" "$DEST/"
mkdir -p "$DEST/mods/music"
cp -a "$ROOT/mods/music/catalog.json" "$ROOT/mods/music/README.md" "$DEST/mods/music/" 2>/dev/null || true
cat >"$DEST/BUILD_WINDOWS.txt" <<'EOF'
Sonic Colors Recomp — Windows build

Prereqs: VS 2022+ Build Tools (C++ workload), CMake, Ninja, network on first configure.

Preferred compiler: clang-cl (VS Installer → Individual components →
  "C++ Clang tools for Windows"). Then open a NEW x64 Developer prompt:

  where clang-cl
  rem expect: ...\VC\Tools\Llvm\x64\bin\clang-cl.exe

  cd <this folder>
  cmake --preset windows-clang
  cmake --build --preset windows-clang --target SonicColorsRecomp

MSVC fallback (may OOM on ppc_recomp — use -j 1 or -j 2):

  cmake --preset windows-msvc
  cmake --build --preset windows-msvc

Output: out\SonicColorsRecomp.exe

Package testers:
  powershell -ExecutionPolicy Bypass -File tools\package.ps1
EOF
test -f "$DEST/cmake/mod_deps.cmake"
test -f "$DEST/generated/sources.cmake"
rm -f "$ROOT/dist/SonicColorsRecomp-windows-build-src.zip"
(cd "$ROOT/dist" && zip -qr SonicColorsRecomp-windows-build-src.zip windows-build-src)
ls -lh "$ROOT/dist/SonicColorsRecomp-windows-build-src.zip"
echo "OK: $DEST"
