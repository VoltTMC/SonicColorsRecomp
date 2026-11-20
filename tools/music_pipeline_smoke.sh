#!/usr/bin/env bash
# H8 pipeline smoke: prepare a Tropical Resort Act 1 (stg110) replacement into an
# isolated cache and verify patched CPK + payload proof. Does not prove in-game
# audibility — only that the WAV→AAX→CPK path for TR1 still works.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CPK="$ROOT/game/SNCP8P/files/sonic2010_0.cpk"
WAV="$ROOT/mods/music/tracks/_diag_tone_48k.wav"
TOOL="$ROOT/tools/music_asset"
CACHE="${SCR_MUSIC_SMOKE_CACHE:-$ROOT/out/music_pipeline_smoke}"

fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

[[ -x "$TOOL" || -f "$TOOL" ]] || fail "missing $TOOL"
[[ -f "$CPK" ]] || fail "missing $CPK (install game data first)"
[[ -f "$WAV" ]] || fail "missing diag WAV $WAV"

rm -rf "$CACHE"
mkdir -p "$CACHE/tracks"
cp "$WAV" "$CACHE/tracks/tr1_diag.wav"

cat >"$CACHE/manifest.toml" <<'EOF'
# Isolated TR1 prepare smoke (do not use as user mods/music)
[assets.stg110_main]
archive = "sonic2010_0.cpk"
path = "sound/Synth/bgm_stg110_rso_wav.aax"
source = "tracks/tr1_diag.wav"
enabled = true
stage = "stg110"
variant = "main"
cue = "bgm_stg110_rso"
EOF

echo "== prepare TR1 diag → $CACHE =="
STATUS=$("$TOOL" prepare --manifest "$CACHE/manifest.toml" --music-dir "$CACHE" --cpk "$CPK" --cache "$CACHE" 2>&1) || fail "prepare failed: $STATUS"
echo "$STATUS" | tail -20

# prepare prints JSON on stdout (last object) or writes status — accept either
JSON="$STATUS"
if ! echo "$JSON" | grep -q '"ok"'; then
  # fallback: current.json
  [[ -f "$CACHE/current.json" ]] || fail "no status JSON / current.json"
  JSON=$(cat "$CACHE/current.json")
fi

echo "$JSON" | grep -q '"ok"[[:space:]]*:[[:space:]]*true' || fail "prepare ok!=true: $JSON"
echo "$JSON" | grep -q '"use_original"[[:space:]]*:[[:space:]]*false' || fail "expected use_original=false"

PATCHED=$(echo "$JSON" | python3 -c "import sys,json,re; t=sys.stdin.read();
m=re.search(r'\{.*\}', t, re.S);
d=json.loads(m.group(0) if m else t);
print(d.get('patched_cpk') or '')" 2>/dev/null || true)
if [[ -z "$PATCHED" && -f "$CACHE/current.json" ]]; then
  PATCHED=$(python3 -c "import json; print(json.load(open('$CACHE/current.json')).get('patched_cpk',''))")
fi
[[ -n "$PATCHED" && -f "$PATCHED" ]] || fail "patched CPK missing ($PATCHED)"
ok "patched CPK $(du -h "$PATCHED" | awk '{print $1}')"

PROOF=""
if [[ -f "$CACHE/current.json" ]]; then
  PROOF=$(python3 -c "import json; print(json.load(open('$CACHE/current.json')).get('payload_proof_tsv',''))" 2>/dev/null || true)
fi
[[ -z "$PROOF" && -f "$CACHE/payload_proof.tsv" ]] && PROOF="$CACHE/payload_proof.tsv"
[[ -n "$PROOF" && -f "$PROOF" ]] || fail "payload_proof.tsv missing"
LINES=$(grep -cve '^#' -e '^$' "$PROOF" || true)
[[ "$LINES" -ge 1 ]] || fail "payload proof empty"
ok "payload proof ($LINES entries)"

# Spot-check TR1 path appears in proof or status
grep -qi 'stg110\|bgm_stg110' "$PROOF" "$CACHE/current.json" 2>/dev/null \
  || echo "$JSON" | grep -qi 'stg110\|bgm_stg110' \
  || fail "TR1 / stg110 not referenced in proof/status"
ok "stg110 referenced"

echo "music_pipeline_smoke OK (TR1 prepare verified; in-game audibility still needs playtest)"
