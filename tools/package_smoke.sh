#!/bin/bash
# Smoke-test a staged Linux AppDir (no copyrighted game data required).
#   tools/package_smoke.sh              # stage + verify dist/AppDir
#   tools/package_smoke.sh -A dist/AppDir   # verify an existing AppDir only
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
appdir=""
while getopts "A:" o; do
    case $o in A) appdir=$OPTARG ;; *) exit 2 ;; esac
done
appdir=${appdir:-$root/dist/AppDir}
out_exe=$root/out/SonicColorsRecomp
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

[ -x "$out_exe" ] || fail "build out/SonicColorsRecomp first"
[ -f "$root/tools/package.ps1" ] || fail "tools/package.ps1 missing (Windows pack script)"
[ -x "$root/tools/package_test_linux.sh" ] || fail "tools/package_test_linux.sh missing/not executable"
ok "packaging scripts present"

# Always restage when verifying the default AppDir so the binary matches `out/`.
if [ "$appdir" = "$root/dist/AppDir" ]; then
    echo "staging AppDir from current build..."
    "$root/tools/package_linux.sh" -S
elif [ ! -x "$appdir/AppRun" ] || [ ! -x "$appdir/usr/bin/SonicColorsRecomp" ]; then
    fail "incomplete AppDir at $appdir"
fi

[ -x "$appdir/AppRun" ] || fail "AppRun missing/not executable"
[ -x "$appdir/usr/bin/SonicColorsRecomp" ] || fail "usr/bin/SonicColorsRecomp missing"
[ -f "$appdir/soniccolorsrecomp.desktop" ] || fail "desktop file missing"
[ -f "$appdir/usr/share/metainfo/soniccolorsrecomp.metainfo.xml" ] || fail "AppStream metainfo missing"
[ -f "$appdir/soniccolorsrecomp.svg" ] || fail ".svg icon missing"
[ -f "$appdir/usr/bin/libSDL3.so.0" ] || fail "libSDL3.so.0 missing"
[ -f "$appdir/usr/bin/libdxvk_d3d11.so.0" ] || fail "libdxvk_d3d11.so.0 missing"
[ -f "$appdir/usr/bin/libdxvk_dxgi.so.0" ] || fail "libdxvk_dxgi.so.0 missing"
[ -f "$appdir/usr/bin/mods/music/catalog.json" ] || fail "mods/music/catalog.json missing from AppDir"
[ -x "$appdir/usr/bin/tools/music_asset" ] || fail "tools/music_asset missing/not executable in AppDir"
[ -f "$appdir/usr/bin/tools/music_asset_pkg/cli.py" ] || fail "music_asset_pkg missing from AppDir"
python3 - <<PY || fail "catalog.json incomplete"
import json, sys
p = "$appdir/usr/bin/mods/music/catalog.json"
d = json.load(open(p, encoding="utf-8"))
n = len(d.get("assets") or [])
print(f"catalog assets={n}")
sys.exit(0 if n >= 100 else 1)
PY
ok "AppDir layout"

# Prefer AppRun so LD_LIBRARY_PATH includes bundled libs.
help_out=$("$appdir/AppRun" --help 2>&1) || fail "--help exited non-zero"
echo "$help_out" | grep -Fq "Sonic Colors Recomp" || fail "--help missing title"
echo "$help_out" | grep -Fq -- "--skip-launcher" || fail "--help missing --skip-launcher"
echo "$help_out" | grep -Fq -- "--portable" || fail "--help missing --portable"
echo "$help_out" | grep -Fq -- "--show-fps" || fail "--help missing --show-fps"
echo "$help_out" | grep -Fq -- "--show-hints" || fail "--help missing --show-hints"
echo "$help_out" | grep -Fq -- "--check-updates" || fail "--help missing --check-updates"
echo "$help_out" | grep -Fq -- "--prepare" || fail "--help missing --prepare"
echo "$help_out" | grep -Fq -- "--game" || fail "--help missing --game"
[ -f "$appdir/recomplauncher.json" ] || fail "recomplauncher.json missing from AppDir root"
ok "--help"
echo "$help_out" | grep -Fq -- "--prepare" || fail "--help missing --prepare"
echo "$help_out" | grep -Fq -- "--download-update" || fail "--help missing --download-update"
echo "$help_out" | grep -Fq -- "--save-diagnostics" || fail "--help missing --save-diagnostics"
echo "$help_out" | grep -Fq -- "--verify-data" || fail "--help missing --verify-data"
echo "$help_out" | grep -Fq -- "--game" || fail "--help missing --game"
ok "help flags (prepare/game/download-update/save-diagnostics/verify-data)"
verify_out=$(SCR_PORTABLE=1 "$appdir/AppRun" --portable --verify-data-quick --nobox 2>&1) || true
echo "$verify_out" | grep -Eqi 'not installed|progress|intact|damaged' || fail "--verify-data-quick unexpected: $verify_out"
ok "--verify-data-quick"

diag_out=$(SCR_PORTABLE=1 "$appdir/AppRun" --portable --save-diagnostics --nobox 2>&1) || fail "--save-diagnostics exited non-zero: $diag_out"
echo "$diag_out" | grep -Fq "scr_diag_" || fail "--save-diagnostics missing report path: $diag_out"
ok "--save-diagnostics"

ver_out=$("$appdir/AppRun" --version 2>&1) || fail "--version exited non-zero"
echo "$ver_out" | grep -Fq "SonicColorsRecomp" || fail "--version missing product name"
ok "--version"

echo "$help_out" | grep -Fq -- "--list-audio-devices" || fail "--help missing --list-audio-devices"
echo "$help_out" | grep -Fq -- "--list-gamepads" || fail "--help missing --list-gamepads"
adev_out=$("$appdir/AppRun" --list-audio-devices 2>&1) || fail "--list-audio-devices exited non-zero"
echo "$adev_out" | grep -Eq '^default$' || fail "--list-audio-devices missing default line: $adev_out"
ok "--list-audio-devices"
pad_out=$("$appdir/AppRun" --list-gamepads 2>&1) || fail "--list-gamepads exited non-zero: $pad_out"
echo "$pad_out" | grep -Eq '^pads: [0-9]+$' || fail "--list-gamepads missing pads count: $pad_out"
ok "--list-gamepads"

# Headless launcher must abort when UI shot env is set (no accidental game boot).
shot_out=$(SCR_UI_SHOT=/tmp/scr_package_smoke.bmp "$appdir/AppRun" --nobox 2>&1 || true)
echo "$shot_out" | grep -qi "aborting\|SCR_UI_SHOT\|BKR_UI_SHOT\|no window" || fail "shot abort not detected: $shot_out"
ok "SCR_UI_SHOT abort"

echo "package_smoke OK ($appdir)"
