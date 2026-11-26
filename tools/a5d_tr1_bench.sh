#!/usr/bin/env bash
# A5d: TR1 hot-BP / KSEL benches (RTX 3080). Validation off for perf.
# Usage:
#   tools/a5d_tr1_bench.sh legacy|a5c|hot|ksel|combined|bp_profile|compare
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAND="${SCR_NAND:-out/test_nand}"
TIMEOUT_S="${SCR_A5D_TIMEOUT:-90}"
SCRIPT="${SCR_A5D_INPUT:-tools/a4k_tr1_explore.txt}"
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
  mkdir -p "out/a5d_${tag}" out/a5d_bp_profile out/a5d_compare
  rm -f out/a5d_vk_shot_*.bmp "out/a5d_${tag}/"*.bmp
  set +e
  timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 \
    SCR_VK_SHOT_PREFIX=out/a5d_vk_shot "$@" \
    "${common[@]}" | tee "out/a5d_${tag}/run.log"
  set -e
  cp -f out/a5d_vk_shot_*.bmp "out/a5d_${tag}/" 2>/dev/null || true
  rg -n "A5d-|A5c-|unique_xfb|DIRECT|flushSkip|bpprof" "out/a5d_${tag}/run.log" | tail -50 || true
}

case "$MODE" in
  legacy) run_one legacy SCR_A5A_DIRECT=0 SCR_A5C_DIRECT_BP=0 SCR_A5D_DIRECT_HOT=0 SCR_A5D_DIRECT_KSEL=0 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  a5c) run_one a5c SCR_A5A_DIRECT=1 SCR_A5C_DIRECT_BP=1 SCR_A5D_DIRECT_HOT=0 SCR_A5D_DIRECT_KSEL=0 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  hot) run_one hot SCR_A5A_DIRECT=0 SCR_A5C_DIRECT_BP=0 SCR_A5D_DIRECT_HOT=1 SCR_A5D_DIRECT_KSEL=0 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  ksel) run_one ksel SCR_A5A_DIRECT=0 SCR_A5C_DIRECT_BP=0 SCR_A5D_DIRECT_HOT=0 SCR_A5D_DIRECT_KSEL=1 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  combined) run_one direct SCR_A5A_DIRECT=1 SCR_A5C_DIRECT_BP=1 SCR_A5D_DIRECT_HOT=1 SCR_A5D_DIRECT_KSEL=1 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  bp_profile) run_one bp_profile SCR_A5A_DIRECT=0 SCR_A5C_DIRECT_BP=0 SCR_A5D_DIRECT_HOT=0 SCR_A5D_DIRECT_KSEL=0 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1 ;;
  compare)
    python3 - <<'PY'
from pathlib import Path
import re
for tag in ('legacy','a5c','hot','ksel','direct','bp_profile'):
    log=Path(f'out/a5d_{tag}/run.log')
    if not log.exists():
        print(tag,'missing'); continue
    text=log.read_text(errors='ignore')
    xs=[float(m.group(1)) for m in re.finditer(r'unique_xfb ([0-9.]+)', text)]
    mid=[x for x in xs if 3<=x<=25]
    title=[x for x in xs if x>=50]
    print(f'{tag}: title_n={len(title)} title_mean={sum(title)/len(title) if title else 0:.2f} mid_n={len(mid)} mid_mean={sum(mid)/len(mid) if mid else 0:.2f}')
    for ln in text.splitlines():
        if ('A5d-' in ln or 'A5c-' in ln) and ('fifoIn=' in ln or 'bpprof-present: total=' in ln):
            print(' ', ln[ln.find('A5'):][:300])
PY
    ;;
  *)
    echo "usage: $0 legacy|a5c|hot|ksel|combined|bp_profile|compare" >&2
    exit 2
    ;;
esac
