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
ok "AppDir layout"

# Prefer AppRun so LD_LIBRARY_PATH includes bundled libs.
help_out=$("$appdir/AppRun" --help 2>&1) || fail "--help exited non-zero"
echo "$help_out" | grep -Fq "Sonic Colors Recomp" || fail "--help missing title"
echo "$help_out" | grep -Fq -- "--skip-launcher" || fail "--help missing --skip-launcher"
echo "$help_out" | grep -Fq -- "--portable" || fail "--help missing --portable"
echo "$help_out" | grep -Fq -- "--show-fps" || fail "--help missing --show-fps"
echo "$help_out" | grep -Fq -- "--show-hints" || fail "--help missing --show-hints"
ok "--help"

ver_out=$("$appdir/AppRun" --version 2>&1) || fail "--version exited non-zero"
echo "$ver_out" | grep -Fq "SonicColorsRecomp" || fail "--version missing product name"
ok "--version"

echo "$help_out" | grep -Fq -- "--list-audio-devices" || fail "--help missing --list-audio-devices"
adev_out=$("$appdir/AppRun" --list-audio-devices 2>&1) || fail "--list-audio-devices exited non-zero"
echo "$adev_out" | grep -Eq '^default$' || fail "--list-audio-devices missing default line: $adev_out"
ok "--list-audio-devices"

# Headless launcher must abort when UI shot env is set (no accidental game boot).
shot_out=$(SCR_UI_SHOT=/tmp/scr_package_smoke.bmp "$appdir/AppRun" --nobox 2>&1 || true)
echo "$shot_out" | grep -qi "aborting\|SCR_UI_SHOT\|BKR_UI_SHOT\|no window" || fail "shot abort not detected: $shot_out"
ok "SCR_UI_SHOT abort"

echo "package_smoke OK ($appdir)"
