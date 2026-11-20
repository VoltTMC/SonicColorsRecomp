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
[ -f "$appdir/usr/bin/tools/music_deps_setup.sh" ] || fail "AppDir music_deps_setup.sh missing"
[ -f "$stage/tools/bin/README.md" ] || fail "Windows tools/bin/README.md missing"
[ -f "$stage/tools/music_deps_setup.cmd" ] || fail "Windows music_deps_setup.cmd missing"
# MinGW music_asset.exe needs winpthread beside it (not only next to the main exe).
if [ -f "$stage/tools/music_asset.exe" ]; then
    [ -f "$stage/tools/libwinpthread-1.dll" ] \
        || fail "Windows tools/libwinpthread-1.dll missing (music_asset.exe cannot load)"
fi
ok "tools/bin + setup helpers present"

if command -v wine >/dev/null && [ -f "$stage/tools/music_asset.exe" ]; then
    set +e
    wine_err=$(mktemp)
    wine_out=$(WINEDEBUG=-all wine "$stage/tools/music_asset.exe" doctor 2>"$wine_err")
    wine_rc=$?
    set -e
    if echo "$wine_out" | grep -Fq '"operations"'; then
        ok "Wine music_asset doctor (rc=$wine_rc)"
    elif grep -qi 'libwinpthread\|Importing dlls\|c0000135' "$wine_err"; then
        rm -f "$wine_err"
        fail "Wine music_asset.exe failed to load (missing MinGW DLL beside tools/)"
    else
        # DLL load OK; Wine often lacks a Windows Python — that is not a packaging failure.
        ok "Wine music_asset.exe loads (doctor needs Windows Python; rc=$wine_rc)"
    fi
    rm -f "$wine_err"
fi

if command -v python3 >/dev/null; then
    export PYTHONPATH="$root/tools${PYTHONPATH:+:$PYTHONPATH}"
    # Prefer project venv site-packages when present
    if [ -d "$root/.venv-music/lib" ]; then
        sp=$(echo "$root"/.venv-music/lib/python*/site-packages)
        export PYTHONPATH="$sp:$PYTHONPATH"
    fi
    "$root/tools/music_deps_test.sh"
    ok "music_deps_test"

    # Clean extracted AppDir doctor: ignore host PATH tools
    app_tools=$appdir/usr/bin/tools
    if [ -d "$app_tools/music_asset_pkg" ]; then
        set +e
        pkg_doc=$(
            PATH="/usr/bin:/bin" SCR_MUSIC_APP_BIN_ONLY=1 PYTHONPATH="$app_tools" \
                python3 -m music_asset_pkg.cli doctor 2>&1
        )
        set -e
        echo "$pkg_doc" | grep -Fq 'browse_catalog' || fail "AppDir doctor missing browse_catalog"
        echo "$pkg_doc" | python3 -c '
import json,sys
d=json.load(sys.stdin)
assert "browse_catalog" in d["ready"]
assert "prepare_replacements" in d["blocked"]
assert d["deps"]["ffmpeg"]["ok"] is False
assert d["deps"]["vgmstream"]["ok"] is False
' || fail "AppDir clean doctor unexpected: $pkg_doc"
        ok "AppDir clean-env doctor (no host tool bleed)"
    fi
else
    echo "SKIP music_deps_test (no python3)"
fi

if [ -d "$root/dist/SonicColorsRecomp-install" ]; then
    "$root/tools/package_windows_install_check.sh" "$root/dist/SonicColorsRecomp-install"
    ok "Windows install stage check"
fi

echo "== Parity notes =="
echo "Linux AppImage + optional Windows portable ZIP + Windows Setup.exe (primary)."
echo "Prepare/preview/export need host Python + cricodecs + vgmstream + ffmpeg"
echo "(user-data tools/bin when installed; setup via music_deps_setup)."
echo "Windows prepare/export pipeline: UNVERIFIED native (Wine load only)."
echo "package_cross_check OK"
