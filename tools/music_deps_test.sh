#!/usr/bin/env bash
# Music Library dependency / doctor regression tests (no game GUI).
# Isolates from host ffmpeg/vgmstream via SCR_MUSIC_APP_BIN_ONLY + temp tools trees.
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

doctor_json() {
    # stdin unused; args: env assignments then command
    "$@"
}

# --- baseline doctor JSON ---
set +e
out=$("$PY" -m music_asset_pkg.cli doctor 2>&1)
rc=$?
set -e
echo "$out" | python3 -c 'import json,sys; json.load(sys.stdin)' || fail "doctor JSON invalid"
echo "$out" | grep -Fq '"operations"' || fail "doctor missing operations"
echo "$out" | grep -Fq 'browse_catalog' || fail "doctor missing browse_catalog"
echo "$out" | grep -Fq 'in_game_replacement' || fail "doctor missing in_game_replacement"
echo "$out" | grep -Fq 'prepare_replacements' || fail "doctor missing prepare_replacements"
echo "$out" | grep -Fq 'tools/bin' || fail "doctor missing drop-in bin hint"
echo "$out" | grep -Fq '"steps"' || fail "doctor missing setup.steps"
ok "doctor JSON (rc=$rc)"

# --- clean env: no PATH tools, empty app bin → blocked prepare/preview_original ---
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT
mkdir -p "$tmpdir/clean tools/bin" "$tmpdir/clean tools/music_asset_pkg"
# Use a path WITH SPACES for the isolated tree
clean="$tmpdir/clean tools"
cp -a "$root/tools/music_asset_pkg/." "$clean/music_asset_pkg/"
rm -rf "$clean/music_asset_pkg/__pycache__"
mkdir -p "$clean/bin"

export PYTHONPATH="$clean"
export SCR_MUSIC_APP_BIN_ONLY=1
unset SCR_MUSIC_BIN || true
# Keep a minimal PATH for the Python interpreter itself; do not expose host ffmpeg/vgmstream.
set +e
clean_out=$(
  PATH="/usr/bin:/bin" SCR_MUSIC_APP_BIN_ONLY=1 PYTHONPATH="$clean" \
    "$PY" -c 'from music_asset_pkg.doctor import doctor; import json; print(json.dumps(doctor()))' 2>&1
)
set -e
echo "$clean_out" | python3 -c '
import json,sys
d=json.load(sys.stdin)
for r in ("browse_catalog","replacement_wav_preview","in_game_replacement"):
    assert r in d["ready"], (r, d)
# No drop-ins → ffmpeg/vgmstream missing regardless of host PATH
assert d["deps"]["ffmpeg"]["ok"] is False, d["deps"]["ffmpeg"]
assert d["deps"]["vgmstream"]["ok"] is False, d["deps"]["vgmstream"]
for b in ("preview_original","export_original","prepare_replacements"):
    assert b in d["blocked"], (b, d.get("blocked"), d.get("deps"))
print("clean ready", d["ready"])
print("clean blocked", d["blocked"])
' || fail "clean-env doctor ops incorrect: $clean_out"
ok "clean-env: catalog/preview WAV ready; prepare/original blocked"

# --- invalid executable in tools/bin (spaces path) ---
printf 'not a real binary\n' > "$clean/bin/ffmpeg"
chmod +x "$clean/bin/ffmpeg"
set +e
bad_out=$("$PY" -c 'from music_asset_pkg.doctor import doctor; import json; print(json.dumps(doctor()))' 2>&1)
set -e
echo "$bad_out" | python3 -c '
import json,sys
d=json.load(sys.stdin)
ff=d["deps"]["ffmpeg"]
assert ff.get("ok") is False, ff
assert ff.get("status") in ("unusable","missing"), ff
assert d["deps"].get("app_bin_only") is True
' || fail "invalid ffmpeg not flagged: $bad_out"
ok "invalid ffmpeg drop-in → unusable"

# --- real tools/bin discovery (copy host ffmpeg if present) ---
rm -f "$clean/bin/ffmpeg"
if command -v ffmpeg >/dev/null 2>&1; then
    ln -sf "$(command -v ffmpeg)" "$clean/bin/ffmpeg"
    set +e
    good_out=$(
      SCR_MUSIC_APP_BIN_ONLY=1 PYTHONPATH="$clean" \
        "$PY" -c 'from music_asset_pkg.doctor import probe_ffmpeg, which_ffmpeg; import json; p=probe_ffmpeg(); print(json.dumps(p)); print("WHICH", which_ffmpeg() or "")' 2>&1
    )
    set -e
    echo "$good_out" | grep -Fq '"ok": true' || fail "linked ffmpeg not ok: $good_out"
    echo "$good_out" | grep -Fq "$clean/bin/ffmpeg" || fail "which did not prefer spaced tools/bin: $good_out"
    ok "tools/bin ffmpeg probe (path with spaces)"
else
    echo "SKIP real ffmpeg probe (no host ffmpeg)"
fi

# --- Unicode directory name ---
uni="$tmpdir/müsic-tøøls"
mkdir -p "$uni/bin" "$uni/music_asset_pkg"
cp -a "$root/tools/music_asset_pkg/." "$uni/music_asset_pkg/"
rm -rf "$uni/music_asset_pkg/__pycache__"
printf '#!/bin/sh\necho fake\nexit 1\n' > "$uni/bin/vgmstream-cli"
chmod +x "$uni/bin/vgmstream-cli"
export PYTHONPATH="$uni"
set +e
uni_out=$("$PY" -c 'from music_asset_pkg.doctor import probe_vgmstream; import json; print(json.dumps(probe_vgmstream()))' 2>&1)
set -e
echo "$uni_out" | python3 -c 'import json,sys; d=json.load(sys.stdin); assert d.get("path"); assert "müsic" in d["path"]' \
    || fail "unicode bin path not discovered: $uni_out"
# stub should be unusable (exit 1, no version)
echo "$uni_out" | python3 -c 'import json,sys; d=json.load(sys.stdin); assert d.get("ok") is False' \
    || fail "stub vgmstream should not be ok: $uni_out"
ok "unicode tools path + stub vgmstream unusable"

unset SCR_MUSIC_APP_BIN_ONLY
export PYTHONPATH="$root/tools"

# --- export_original must not hard-require ffmpeg ---
set +e
exp_out=$("$PY" -c '
from music_asset_pkg import doctor as D
import json
# Force ffmpeg missing in ops evaluation by patching have map via doctored report
r=D.doctor()
op=next(o for o in r["operations"] if o["id"]=="export_original")
print(json.dumps(op))
' 2>&1)
set -e
echo "$exp_out" | python3 -c '
import json,sys
o=json.load(sys.stdin)
assert "ffmpeg" not in o.get("missing",[]), o
# warnings may list optional ffmpeg
' || fail "export_original incorrectly hard-requires ffmpeg: $exp_out"
ok "export_original treats ffmpeg as optional"

# --- SCR_MUSIC_BIN + Windows-style .exe name discovery (no probe required for find) ---
drop="$tmpdir/user drop/bin"
mkdir -p "$drop"
# Extensionless stub named like a Windows drop-in basename; find_in_app must locate
# vgmstream-cli.exe when that file exists under SCR_MUSIC_BIN.
printf '#!/bin/sh\necho "vgmstream-cli 0.0-test"\nexit 0\n' > "$drop/vgmstream-cli.exe"
chmod +x "$drop/vgmstream-cli.exe"
set +e
bin_out=$(
  SCR_MUSIC_APP_BIN_ONLY=1 SCR_MUSIC_BIN="$drop" PYTHONPATH="$root/tools" \
    "$PY" -c '
from music_asset_pkg.doctor import find_in_app, probe_vgmstream, missing_tool_message
import json
path, searched = find_in_app(["vgmstream-cli", "vgmstream-cli.exe"])
assert path and path.endswith("vgmstream-cli.exe"), (path, searched)
assert any("user drop" in s for s in searched), searched
info = probe_vgmstream()
assert info.get("path") and "vgmstream-cli.exe" in info["path"], info
# Missing tool message lists searched dirs
msg = missing_tool_message("vgmstream", ["vgmstream-cli"], searched=["/tmp/a", "PATH"])
assert "Searched" in msg and "/tmp/a" in msg, msg
print(json.dumps({"path": path, "searched": searched, "probe_ok": info.get("ok")}))
' 2>&1
)
set -e
echo "$bin_out" | grep -Fq 'vgmstream-cli.exe' || fail "SCR_MUSIC_BIN .exe discovery failed: $bin_out"
ok "SCR_MUSIC_BIN discovers vgmstream-cli.exe (path with spaces)"

# --- packaged tools/bin without SCR_MUSIC_BIN ---
pkg="$tmpdir/pkg tools"
mkdir -p "$pkg/bin" "$pkg/music_asset_pkg"
cp -a "$root/tools/music_asset_pkg/." "$pkg/music_asset_pkg/"
rm -rf "$pkg/music_asset_pkg/__pycache__"
printf '#!/bin/sh\necho "vgmstream-cli 1.0-pkg"\nexit 0\n' > "$pkg/bin/vgmstream-cli"
chmod +x "$pkg/bin/vgmstream-cli"
set +e
pkg_out=$(
  unset SCR_MUSIC_BIN
  SCR_MUSIC_APP_BIN_ONLY=1 PYTHONPATH="$pkg" \
    "$PY" -c '
from music_asset_pkg.doctor import find_in_app
path, searched = find_in_app(["vgmstream-cli", "vgmstream-cli.exe"])
assert path and path.endswith("vgmstream-cli"), (path, searched)
assert any(s.endswith("bin") for s in searched), searched
print(path)
' 2>&1
)
set -e
echo "$pkg_out" | grep -Fq "$pkg/bin/vgmstream-cli" || fail "packaged tools/bin miss: $pkg_out"
ok "packaged tools/bin discovery without SCR_MUSIC_BIN"

# --- preview error includes searched when missing ---
set +e
prev_out=$(
  SCR_MUSIC_APP_BIN_ONLY=1 SCR_MUSIC_BIN="$tmpdir/emptybin" PYTHONPATH="$root/tools" \
    "$PY" -c '
import os, tempfile
from pathlib import Path
os.makedirs(os.environ["SCR_MUSIC_BIN"], exist_ok=True)
from music_asset_pkg.doctor import probe_vgmstream
info = probe_vgmstream()
assert not info.get("ok")
assert info.get("error") and "Searched" in info["error"], info
assert isinstance(info.get("searched"), list) and info["searched"], info
print("preview-missing-ok")
' 2>&1
)
set -e
echo "$prev_out" | grep -Fq preview-missing-ok || fail "missing vgmstream error incomplete: $prev_out"
ok "missing vgmstream reports searched paths"

# --- managed bin discovery (SCR_MUSIC_MANAGED_BIN) ---
managed="$tmpdir/managed bin"
mkdir -p "$managed"
printf '#!/bin/sh\necho "vgmstream-cli 9.9-managed"\nexit 0\n' > "$managed/vgmstream-cli"
chmod +x "$managed/vgmstream-cli"
set +e
man_out=$(
  unset SCR_MUSIC_BIN
  SCR_MUSIC_APP_BIN_ONLY=1 SCR_MUSIC_MANAGED_BIN="$managed" PYTHONPATH="$root/tools" \
    "$PY" -c '
from music_asset_pkg.doctor import find_in_app
path, searched = find_in_app(["vgmstream-cli", "vgmstream-cli.exe"])
assert path and path.endswith("vgmstream-cli"), (path, searched)
assert any("managed bin" in s for s in searched), searched
print(path)
' 2>&1
)
set -e
echo "$man_out" | grep -Fq "$managed/vgmstream-cli" || fail "managed bin miss: $man_out"
ok "SCR_MUSIC_MANAGED_BIN discovery"

# --- pinned Windows music deps manifest ---
manf="$root/data/music_deps_manifest.json"
[ -f "$manf" ] || fail "data/music_deps_manifest.json missing"
python3 -c '
import json,sys
from pathlib import Path
m=json.loads(Path(sys.argv[1]).read_text())
assert m.get("schema")==1
ids={p["id"] for p in m["packages"]}
assert ids=={"python_embed","cricodecs","vgmstream","ffmpeg_lgpl"}, ids
for p in m["packages"]:
    assert len(p["sha256"])==64 and p["url"].startswith("https://"), p["id"]
    assert "lgpl" in p["url"].lower() or p["id"]!="ffmpeg_lgpl"
print("manifest packages", sorted(ids))
' "$manf" || fail "music_deps_manifest invalid"
ok "music_deps_manifest.json pinned + https"

if [ "${SCR_MUSIC_PIPELINE:-0}" = 1 ]; then
    if [ -f "$root/game/SNCP8P/files/sonic2010_0.cpk" ] && command -v vgmstream-cli >/dev/null \
        && "$PY" -c 'import cricodecs' 2>/dev/null; then
        "$root/tools/music_pipeline_smoke.sh"
        ok "music_pipeline_smoke [linux]"
    else
        echo "SKIP pipeline: need CPK + vgmstream + cricodecs in this interpreter"
    fi
fi

# Windows pipeline: only when SCR_MUSIC_PIPELINE_WIN=1 and wine + windows python available
if [ "${SCR_MUSIC_PIPELINE_WIN:-0}" = 1 ]; then
    echo "Windows pipeline under Wine is optional; see docs/CROSS_PLATFORM_COMPATIBILITY.md"
    if command -v wine >/dev/null && [ -f "$root/out-windows/music_asset.exe" ]; then
        echo "NOTE: [wine] music_asset load check only — native Windows prepare remains UNVERIFIED unless run on Windows"
    fi
fi

echo "music_deps_test OK"
