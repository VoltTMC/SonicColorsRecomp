#!/bin/bash
# Verify a Windows redistributable ZIP (or staged dist/SonicColorsRecomp) ships the
# Music Library catalog. Does not require Wine or a Windows host.
#
#   tools/package_windows_music_check.sh [path-to.zip|path-to-staged-dir]
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
target=${1:-}
if [ -z "$target" ]; then
    target=$(ls -1t "$root"/dist/SonicColorsRecomp-*-windows-x86_64.zip 2>/dev/null | head -1 || true)
fi
[ -n "$target" ] || { echo "FAIL: no Windows zip/stage given" >&2; exit 1; }

fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

tmpdir=
cleanup() { [ -n "${tmpdir:-}" ] && rm -rf "$tmpdir"; }
trap cleanup EXIT

if [ -f "$target" ]; then
    tmpdir=$(mktemp -d)
    unzip -q -d "$tmpdir" "$target"
    stage=$tmpdir/SonicColorsRecomp
    [ -d "$stage" ] || stage=$tmpdir
elif [ -d "$target" ]; then
    stage=$target
else
    fail "not a file or directory: $target"
fi

[ -f "$stage/SonicColorsRecomp.exe" ] || [ -f "$stage/mods/music/catalog.json" ] \
    || fail "unrecognised Windows stage layout under $stage"
[ -f "$stage/mods/music/catalog.json" ] || fail "mods/music/catalog.json missing from Windows package"

python3 - <<PY
import json, sys
p = r'''$stage/mods/music/catalog.json'''
with open(p, encoding='utf-8') as f:
    d = json.load(f)
assets = d.get('assets') or []
n = d.get('music_count', len(assets))
if len(assets) < 100:
    print(f'FAIL: catalog has only {len(assets)} assets (expected ~114)', file=sys.stderr)
    sys.exit(1)
fx = sum(1 for a in assets if (a.get('variant') or '') == 'fx' or str(a.get('id','')).endswith('_fx'))
print(f'assets={len(assets)} music_count={n} fx≈{fx}')
if n < 100:
    print('FAIL: music_count too small', file=sys.stderr)
    sys.exit(1)
PY
ok "Windows package Music Library catalog ($target)"
