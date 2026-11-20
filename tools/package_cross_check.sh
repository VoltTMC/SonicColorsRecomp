#!/bin/bash
# Cross-platform redistributable content checks (no game data / no native GUI).
#   tools/package_cross_check.sh
# Verifies staged AppDir + newest Windows ZIP (or paths via env).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

appdir=${SCR_APPDIR:-$root/dist/AppDir}
win=${SCR_WIN_PACKAGE:-}
if [ -z "$win" ]; then
    win=$(ls -1t "$root"/dist/SonicColorsRecomp-*-windows-x86_64.zip 2>/dev/null | head -1 || true)
fi

echo "== Linux AppDir =="
[ -d "$appdir" ] || fail "AppDir missing: $appdir (run tools/package_linux.sh -S)"
"$root/tools/package_smoke.sh" -A "$appdir"
ok "Linux package_smoke"

echo "== Windows ZIP =="
[ -n "$win" ] && [ -f "$win" ] || fail "Windows zip missing under dist/"
"$root/tools/package_windows_music_check.sh" "$win"

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT
unzip -q -d "$tmpdir" "$win"
stage=$tmpdir/SonicColorsRecomp
[ -d "$stage" ] || stage=$tmpdir
[ -f "$stage/SonicColorsRecomp.exe" ] || fail "exe missing in Windows zip"
[ -f "$stage/portable.txt" ] || fail "portable.txt missing"
[ -f "$stage/tools/music_asset.exe" ] || [ -f "$stage/tools/music_asset.cmd" ] \
    || fail "music_asset helper missing from Windows zip"
[ -f "$stage/tools/music_asset_pkg/cli.py" ] || fail "music_asset_pkg missing from Windows zip"
[ -f "$stage/mods/music/catalog.json" ] || fail "catalog.json missing from Windows zip"
ok "Windows package layout ($win)"

[ -f "$appdir/usr/bin/tools/bin/README.md" ] || fail "AppDir tools/bin/README.md missing"
[ -f "$stage/tools/bin/README.md" ] || fail "Windows tools/bin/README.md missing"
[ -f "$stage/tools/music_deps_setup.cmd" ] || fail "Windows music_deps_setup.cmd missing"
ok "tools/bin + setup helpers present"

if command -v python3 >/dev/null; then
    export PYTHONPATH="$root/tools${PYTHONPATH:+:$PYTHONPATH}"
    # Prefer project venv site-packages when present
    if [ -d "$root/.venv-music/lib" ]; then
        sp=$(echo "$root"/.venv-music/lib/python*/site-packages)
        export PYTHONPATH="$sp:$PYTHONPATH"
    fi
    "$root/tools/music_deps_test.sh"
    ok "music_deps_test"
else
    echo "SKIP music_deps_test (no python3)"
fi

echo "== Parity notes =="
echo "Both packages ship catalog.json, music_asset*, tools/bin/README, setup helpers."
echo "Prepare/preview/export still need host Python + cricodecs + vgmstream + ffmpeg"
echo "(drop-ins in tools/bin or PATH; setup via music_deps_setup)."
echo "package_cross_check OK"
