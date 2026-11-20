#!/usr/bin/env bash
# Automated Music Library dependency / doctor checks (no game GUI).
# Optional: SCR_MUSIC_PIPELINE=1 also runs tools/music_pipeline_smoke.sh when CPK+deps exist.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

export PYTHONPATH="$root/tools${PYTHONPATH:+:$PYTHONPATH}"
PY=
if [ -x "$root/.venv-music/bin/python" ]; then
    PY=$root/.venv-music/bin/python
elif command -v python3 >/dev/null; then
    PY=$(command -v python3)
else
    fail "no python3"
fi

# doctor must run even when deps are missing (exit 1 is OK)
set +e
out=$("$PY" -m music_asset_pkg.cli doctor 2>&1)
rc=$?
set -e
echo "$out" | python3 -c 'import json,sys; json.load(sys.stdin)' || fail "doctor JSON invalid"
echo "$out" | grep -Fq '"operations"' || fail "doctor missing operations"
echo "$out" | grep -Fq 'browse_catalog' || fail "doctor missing browse_catalog"
echo "$out" | grep -Fq 'prepare_replacements' || fail "doctor missing prepare_replacements"
echo "$out" | grep -Fq 'tools/bin' || fail "doctor missing drop-in bin hint"
ok "doctor JSON (rc=$rc)"

# App-local bin discovery: fake ffmpeg in tools/bin must win over missing PATH name
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT
mkdir -p "$tmpdir/tools/bin" "$tmpdir/tools/music_asset_pkg"
cp -a "$root/tools/music_asset_pkg/." "$tmpdir/tools/music_asset_pkg/"
rm -rf "$tmpdir/tools/music_asset_pkg/__pycache__"
cat >"$tmpdir/tools/bin/ffmpeg" <<'EOF'
#!/bin/sh
echo fake-ffmpeg
EOF
chmod +x "$tmpdir/tools/bin/ffmpeg"
export PYTHONPATH="$tmpdir/tools"
hit=$("$PY" -c 'from music_asset_pkg.doctor import which_ffmpeg; print(which_ffmpeg() or "")')
echo "$hit" | grep -Fq "$tmpdir/tools/bin/ffmpeg" || fail "which_ffmpeg did not prefer tools/bin ($hit)"
ok "tools/bin ffmpeg discovery"

if [ "${SCR_MUSIC_PIPELINE:-0}" = 1 ]; then
    if [ -f "$root/game/SNCP8P/files/sonic2010_0.cpk" ] && command -v vgmstream-cli >/dev/null \
        && "$PY" -c 'import cricodecs' 2>/dev/null; then
        "$root/tools/music_pipeline_smoke.sh"
        ok "music_pipeline_smoke"
    else
        echo "SKIP pipeline: need CPK + vgmstream + cricodecs in this interpreter"
    fi
fi

echo "music_deps_test OK"
