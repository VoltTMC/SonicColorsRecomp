#!/usr/bin/env bash
# A5a: TR1 front-end attribution (RTX 3080). Validation off.
# Usage:
#   tools/a5a_tr1_bench.sh legacy     # FIFO path (default product)
#   tools/a5a_tr1_bench.sh direct     # SCR_A5A_DIRECT=1 matrix overrides
#   tools/a5a_tr1_bench.sh compare
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAND="${SCR_NAND:-out/test_nand}"
TIMEOUT_S="${SCR_A5A_TIMEOUT:-180}"
SCRIPT="${SCR_A5A_INPUT:-tools/a4k_tr1_explore.txt}"
MODE="${1:-}"

common=(
  ./out/SonicColorsRecomp
  --skip-launcher --scale 1 --no-vsync
  --nand "$NAND" --mute --renderer vulkan
  --input-script "$SCRIPT"
  --shots 20 --shots-start 1800
)

run_one() {
  local tag="$1"
  shift
  mkdir -p "out/a5a_${tag}_tr1" out/a5a_profile
  rm -f out/a5a_vk_shot_*.bmp "out/a5a_${tag}_tr1/"*.bmp
  set +e
  timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 SCR_A5A_PROF=1 SCR_A4N_PROF=1 \
    SCR_VK_SHOT_PREFIX=out/a5a_vk_shot "$@" \
    "${common[@]}" | tee "out/a5a_${tag}_tr1/run.log"
  set -e
  cp -f out/a5a_vk_shot_*.bmp "out/a5a_${tag}_tr1/" 2>/dev/null || true
  rg -n "A5a-|A4n-present:|directXf|unique_xfb" "out/a5a_${tag}_tr1/run.log" | tail -30 || true
}

case "$MODE" in
  legacy) run_one legacy SCR_A5A_DIRECT=0 ;;
  direct) run_one direct SCR_A5A_DIRECT=1 ;;
  compare)
    python3 - <<'PY'
from pathlib import Path
import re
for tag in ('legacy','direct'):
    log=Path(f'out/a5a_{tag}_tr1/run.log')
    if not log.exists():
        print(tag,'missing'); continue
    text=log.read_text(errors='ignore')
    xs=[float(m.group(1)) for m in re.finditer(r'unique_xfb ([0-9.]+)', text)]
    mid=[x for x in xs if 3<=x<=20]
    print(f'{tag}: mid_n={len(mid)} mid_mean={sum(mid)/len(mid) if mid else 0:.2f} wd={text.count("watchdog")}')
    for ln in text.splitlines():
        if 'A5a-' in ln and ('fifoIn=' in ln or 'directXf=' in ln):
            print(' ', ln[-200:])
PY
    ;;
  *)
    echo "usage: $0 legacy|direct|compare" >&2
    exit 2
    ;;
esac
