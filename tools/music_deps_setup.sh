#!/usr/bin/env bash
# Create/update a local .venv-music with cricodecs for Music Library prepare/preview.
# Does not download FFmpeg/vgmstream (place those in tools/bin/ — see tools/bin/README.md).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
venv=${SCR_MUSIC_VENV:-$root/.venv-music}
py=${SCR_MUSIC_PYTHON:-}
if [ -z "$py" ]; then
    for c in /usr/bin/python3 python3 python; do
        if command -v "$c" >/dev/null 2>&1; then py=$(command -v "$c"); break; fi
    done
fi
[ -n "$py" ] || { echo "FAIL: Python 3 not found" >&2; exit 1; }

echo "Using Python: $py"
echo "Venv: $venv"
"$py" -m venv "$venv"
# Prefer the venv pip/python exclusively
"$venv/bin/python" -m pip install --upgrade pip
# Pin when possible for reproducibility; fall back to latest if pin unavailable.
if ! "$venv/bin/python" -m pip install 'cricodecs==1.2.0'; then
    echo "WARN: cricodecs==1.2.0 unavailable; installing latest cricodecs" >&2
    "$venv/bin/python" -m pip install cricodecs
fi

export PYTHONPATH="$root/tools${PYTHONPATH:+:$PYTHONPATH}"
"$venv/bin/python" -m music_asset_pkg.cli doctor
echo "OK: music deps venv ready. Optional: copy ffmpeg + vgmstream-cli into tools/bin/"
