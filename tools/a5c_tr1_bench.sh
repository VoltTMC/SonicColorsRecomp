#!/usr/bin/env bash
# A5c: TR1 BP profile + direct BP/XF benches (RTX 3080). Validation off for perf.
# Usage:
#   tools/a5c_tr1_bench.sh legacy
#   tools/a5c_tr1_bench.sh a5a            # DIRECT XF only
#   tools/a5c_tr1_bench.sh bp             # DIRECT_BP only
#   tools/a5c_tr1_bench.sh combined       # A5A_DIRECT + A5C_DIRECT_BP
#   tools/a5c_tr1_bench.sh bp_profile     # per-register BP profile (legacy path)
#   tools/a5c_tr1_bench.sh compare
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAND="${SCR_NAND:-out/test_nand}"
TIMEOUT_S="${SCR_A5C_TIMEOUT:-90}"
SCRIPT="${SCR_A5C_INPUT:-tools/a4k_tr1_explore.txt}"
MODE="${1:-}"

common=(
  ./out/SonicColorsRecomp
  --skip-launcher --scale 1 --no-vsync
  --nand "$NAND" --mute --renderer vulkan
  --input-script "$SCRIPT"
  --shots 10 --shots-start 1800
)

run_one() {
  local tag="$1"
  shift
  mkdir -p "out/a5c_${tag}" out/a5c_bp_profile out/a5c_xf_profile out/a5c_compare
  rm -f out/a5c_vk_shot_*.bmp "out/a5c_${tag}/"*.bmp
  set +e
  timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 \
    SCR_VK_SHOT_PREFIX=out/a5c_vk_shot "$@" \
    "${common[@]}" | tee "out/a5c_${tag}/run.log"
  set -e
  cp -f out/a5c_vk_shot_*.bmp "out/a5c_${tag}/" 2>/dev/null || true
  rg -n "A5c-|A5b-|A5a-|unique_xfb|DIRECT" "out/a5c_${tag}/run.log" | tail -50 || true
}

case "$MODE" in
  legacy) run_one legacy SCR_A5A_DIRECT=0 SCR_A5C_DIRECT_BP=0 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  a5a|xf) run_one direct_xf SCR_A5A_DIRECT=1 SCR_A5C_DIRECT_BP=0 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  bp|direct_bp) run_one direct_bp SCR_A5A_DIRECT=0 SCR_A5C_DIRECT_BP=1 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  combined) run_one direct SCR_A5A_DIRECT=1 SCR_A5C_DIRECT_BP=1 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  bp_profile) run_one bp_profile SCR_A5A_DIRECT=0 SCR_A5C_DIRECT_BP=0 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  compare)
    python3 - <<'PY'
from pathlib import Path
import re
for tag in ('legacy','direct_xf','direct_bp','direct','bp_profile'):
    log=Path(f'out/a5c_{tag}/run.log')
    if not log.exists():
        print(tag,'missing'); continue
    text=log.read_text(errors='ignore')
    xs=[float(m.group(1)) for m in re.finditer(r'unique_xfb ([0-9.]+)', text)]
    mid=[x for x in xs if 3<=x<=25]
    print(f'{tag}: mid_n={len(mid)} mid_mean={sum(mid)/len(mid) if mid else 0:.2f}')
    for ln in text.splitlines():
        if ('A5c-' in ln or 'A5b-' in ln) and ('fifoIn=' in ln or 'bpprof' in ln):
            print(' ', ln[ln.find('A5'):][:280])
PY
    ;;
  *)
    echo "usage: $0 legacy|a5a|bp|combined|bp_profile|compare" >&2
    exit 2
    ;;
esac
