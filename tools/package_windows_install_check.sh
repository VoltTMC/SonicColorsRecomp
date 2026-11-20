#!/usr/bin/env bash
# Validate the Windows *installed* stage / Setup artifacts (no GUI).
#   tools/package_windows_install_check.sh [stage-dir]
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

stage=${1:-$root/dist/SonicColorsRecomp-install}
[ -d "$stage" ] || fail "stage missing: $stage (run tools/package_windows_installer.sh)"

[ -f "$stage/SonicColorsRecomp.exe" ] || fail "exe missing"
[ -f "$stage/installed.txt" ] || fail "installed.txt missing"
[ ! -f "$stage/portable.txt" ] || fail "portable.txt must not ship in installer stage"
[ -f "$stage/mods/music/catalog.json" ] || fail "catalog.json missing"
[ -f "$stage/tools/music_asset.exe" ] || [ -f "$stage/tools/music_asset.cmd" ] || fail "music_asset missing"
[ -f "$stage/tools/music_deps_setup.cmd" ] || fail "music_deps_setup.cmd missing"
[ -f "$stage/tools/bin/README.md" ] || fail "tools/bin/README missing"
[ -f "$stage/tools/libwinpthread-1.dll" ] || fail "tools/libwinpthread-1.dll missing"
ok "install stage layout ($stage)"

# Doctor from staged pkg with APP_BIN_ONLY + fake APPDATA
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT
export APPDATA=$tmpdir/AppData
mkdir -p "$APPDATA/SonicColorsRecomp/tools/bin"
export PYTHONPATH="$stage/tools"
export SCR_MUSIC_APP_BIN_ONLY=1
export PATH="/usr/bin:/bin"
set +e
doc=$(python3 -m music_asset_pkg.cli doctor 2>&1)
set -e
echo "$doc" | python3 -c '
import json,sys
d=json.load(sys.stdin)
assert "browse_catalog" in d["ready"]
assert "prepare_replacements" in d["blocked"]
user=d["setup"]["drop_in_user"]
assert "SonicColorsRecomp" in user and "tools" in user
' || fail "staged doctor unexpected: $doc"
ok "staged doctor prefers user-data drop-ins"

setup=$(ls -1t "$root"/dist/SonicColorsRecomp-Setup-v*.exe 2>/dev/null | head -1 || true)
if [ -n "$setup" ] && [ -f "$setup" ]; then
    ok "Setup.exe present ($(basename "$setup"))"
else
    echo "SKIP Setup.exe (build with Inno/ISCC via package_windows_installer.sh)"
fi

echo "package_windows_install_check OK"
