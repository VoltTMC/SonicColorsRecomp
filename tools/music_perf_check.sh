#!/usr/bin/env bash
# Micro-benchmarks for Music Library hot paths (no GUI).
# Usage: ./tools/music_perf_check.sh
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

# Synthetic stereo 16-bit WAV ~8 MiB PCM (about 45s at 48 kHz) — large enough that
# full Wav_Load would dominate a per-frame UI call if used for status.
python3 - <<'PY' "$tmpdir/big.wav"
import struct, sys, pathlib
path = pathlib.Path(sys.argv[1])
sr, ch, bits = 48000, 2, 16
nframes = sr * 45
data = bytes(nframes * ch * (bits // 8))  # silence
byte_rate = sr * ch * (bits // 8)
block = ch * (bits // 8)
fmt = struct.pack('<HHIIHH', 1, ch, sr, byte_rate, block, bits)
chunks = b'fmt ' + struct.pack('<I', len(fmt)) + fmt + b'data' + struct.pack('<I', len(data)) + data
riff = b'RIFF' + struct.pack('<I', 4 + len(chunks)) + b'WAVE' + chunks
path.write_bytes(riff)
print(path, 'bytes', path.stat().st_size)
PY

# Prefer built wav_load_test if present; else compile a tiny probe harness.
bin=
if [ -x "$root/out/wav_load_test" ]; then
  bin=$root/out/wav_load_test
elif [ -x "$root/out-windows/wav_load_test.exe" ] && command -v wine >/dev/null; then
  bin="wine $root/out-windows/wav_load_test.exe"
fi

# Inline C++ timing via python ctypes is awkward; use a one-shot compile when needed.
probe_src=$tmpdir/probe.cpp
cat > "$probe_src" <<'CPP'
#include "audio/wav_file.h"
#include <chrono>
#include <cstdio>
#include <string>
int main(int argc, char** argv) {
  if (argc < 2) return 2;
  using clock = std::chrono::steady_clock;
  WavProbe p;
  auto t0 = clock::now();
  bool pok = Wav_Probe(argv[1], p);
  auto t1 = clock::now();
  WavData w;
  bool lok = Wav_Load(argv[1], w);
  auto t2 = clock::now();
  double pms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  double lms = std::chrono::duration<double, std::milli>(t2 - t1).count();
  if (!pok) { std::fprintf(stderr, "probe fail: %s\n", p.error.c_str()); return 1; }
  if (!lok) { std::fprintf(stderr, "load fail: %s\n", w.error.c_str()); return 1; }
  std::printf("probe_ms=%.3f load_ms=%.3f duration=%.1f rate=%d\n", pms, lms, p.durationSec, p.sampleRate);
  if (!(lms > pms * 5.0 || lms > 5.0)) {
    // Tiny files may not show a huge gap; still require probe to be faster.
    if (!(pms <= lms)) { std::fprintf(stderr, "probe not faster than load\n"); return 1; }
  }
  return 0;
}
CPP

cxx=${CXX:-c++}
inc="-I$root/runtime"
# Match project: wav_file is header+cpp
$cxx -std=c++20 -O2 $inc "$probe_src" "$root/runtime/audio/wav_file.cpp" -o "$tmpdir/probe" \
  ${CXXFLAGS:-} 2>"$tmpdir/cxx.err" || {
  echo "compile err:"; cat "$tmpdir/cxx.err"; fail "probe harness compile";
}
out=$("$tmpdir/probe" "$tmpdir/big.wav") || fail "probe run: $out"
echo "$out"
echo "$out" | grep -Eq 'probe_ms=[0-9]' || fail "bad output"
# Probe must be clearly cheaper on an 8MiB+ WAV
python3 - <<'PY' "$out"
import sys,re
s=sys.argv[1]
m=re.search(r'probe_ms=([0-9.]+).*load_ms=([0-9.]+)', s)
assert m, s
p,l=float(m.group(1)),float(m.group(2))
assert p < l, (p,l)
assert p < 50.0, p  # header probe should be tens of ms worst-case on HDD
print(f"speedup load/probe = {l/max(p,1e-6):.1f}x")
PY
ok "Wav_Probe vs Wav_Load (status-list hotspot)"

# Catalog load timing (JSON parse) if catalog exists
catf=
for c in "$root/mods/music/catalog.json" "$root/data/catalog.json" \
         "$root/out/mods/music/catalog.json" "$root/dist/SonicColorsRecomp/mods/music/catalog.json"; do
  [ -f "$c" ] && catf=$c && break
done
if [ -n "$catf" ]; then
  python3 - <<'PY' "$catf"
import json,time,sys
from pathlib import Path
p=Path(sys.argv[1])
t0=time.perf_counter()
j=json.loads(p.read_text())
t1=time.perf_counter()
n=len(j.get("assets") or j.get("tracks") or [])
print(f"catalog_parse_ms={(t1-t0)*1000:.2f} assets≈{n} path={p}")
assert n>0
PY
  ok "catalog JSON parse"
else
  echo "SKIP catalog parse (no catalog.json found)"
fi

echo "music_perf_check OK"
echo "GUI: launch with SCR_MUSIC_PERF=1 to log filter_rebuild ms in the Music tab."
