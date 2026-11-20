#!/usr/bin/env bash
# Create/update a local .venv-music with cricodecs for Music Library prepare/preview.
# Does not download FFmpeg/vgmstream (place those in tools/bin/ — see tools/bin/README.md).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
venv=${SCR_MUSIC_VENV:-$root/.venv-music}
py=${SCR_MUSIC_PYTHON:-}
if [ -z "$py" ]; then
    for c in /usr/bin/python3 /usr/local/bin/python3 python3 python; do
        if [ -x "$c" ] 2>/dev/null || command -v "$c" >/dev/null 2>&1; then
            py=$(command -v "$c" 2>/dev/null || echo "$c")
            [ -x "$py" ] && break
        fi
    done
fi
[ -n "${py:-}" ] && [ -x "$py" ] || { echo "FAIL: Python 3 not found" >&2; exit 1; }

echo "== Music Library dependency setup (Linux) =="
echo "Using Python: $py"
echo "Venv: $venv"
echo "Drop-in tools: $root/tools/bin/  (see $root/tools/bin/README.md)"
"$py" -m venv "$venv"
"$venv/bin/python" -m pip install --upgrade pip
if ! "$venv/bin/python" -m pip install 'cricodecs==1.2.0'; then
    echo "WARN: cricodecs==1.2.0 unavailable; installing latest cricodecs" >&2
    "$venv/bin/python" -m pip install cricodecs
fi

export PYTHONPATH="$root/tools${PYTHONPATH:+:$PYTHONPATH}"
echo
echo "== Verifying with music_asset doctor =="
set +e
report=$("$venv/bin/python" -m music_asset_pkg.cli doctor 2>&1)
rc=$?
set -e
echo "$report"
echo
echo "$report" | "$venv/bin/python" -c '
import json,sys
d=json.load(sys.stdin)
print("Ready:", ", ".join(d.get("ready") or []) or "(none)")
print("Blocked:", ", ".join(d.get("blocked") or []) or "(none)")
for s in (d.get("setup") or {}).get("steps") or []:
    print("Next:", s)
'
if [ $rc -eq 0 ]; then
    echo "OK: all Music Library host tools usable."
else
    echo "OK: cricodecs venv ready. Remaining blockers are listed above (usually ffmpeg / vgmstream in tools/bin)."
fi
exit 0
