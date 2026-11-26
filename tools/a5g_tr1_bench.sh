#!/usr/bin/env bash
# A5g: indexed XF direct adapters (RTX 3080). Validation off for perf.
# Usage:
#   tools/a5g_tr1_bench.sh baseline|indexed|combined|compare
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAND="${SCR_NAND:-out/test_nand}"
TIMEOUT_S="${SCR_A5G_TIMEOUT:-90}"
SCRIPT="${SCR_A5G_INPUT:-tools/a4k_tr1_explore.txt}"
MODE="${1:-}"

common=(
  ./out/SonicColorsRecomp
  --skip-launcher --scale 1 --no-vsync
  --nand "$NAND" --mute --renderer vulkan
  --input-script "$SCRIPT"
  --shots 10 --shots-start 1800
)

# Prior A5 adapters on; deferred BP left off to isolate indexed XF (override via env).
a5=(
  SCR_A5A_DIRECT=1 SCR_A5C_DIRECT_BP=1 SCR_A5D_DIRECT_HOT=1 SCR_A5D_DIRECT_KSEL=1
  SCR_A5E_DIRECT_TEXOBJ=1 SCR_A5F_DEFERRED_BP="${SCR_A5F_DEFERRED_BP:-0}"
  SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1
)

run_one() {
  local tag="$1"
  shift
  mkdir -p "out/a5g_${tag}" out/a5g_compare
  rm -f out/a5g_vk_shot_*.bmp "out/a5g_${tag}/"*.bmp
  set +e
  timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 \
    SCR_VK_SHOT_PREFIX=out/a5g_vk_shot "$@" \
    "${common[@]}" | tee "out/a5g_${tag}/run.log"
  set -e
  cp -f out/a5g_vk_shot_*.bmp "out/a5g_${tag}/" 2>/dev/null || true
  rg -n "A5e-|A5g-|A5f-|unique_xfb|idxXf|xfIdx|DIRECT" "out/a5g_${tag}/run.log" | tail -80 || true
}

case "$MODE" in
  baseline)
    run_one baseline "${a5[@]}" SCR_A5G_DIRECT_INDEXED_XF=0
    ;;
  indexed)
    run_one indexed "${a5[@]}" SCR_A5G_DIRECT_INDEXED_XF=1
    ;;
  combined)
    # Indexed + deferred BP together
    run_one combined "${a5[@]}" SCR_A5F_DEFERRED_BP=1 SCR_A5G_DIRECT_INDEXED_XF=1
    ;;
  compare)
    python3 - <<'PY'
from pathlib import Path
import re
for tag in ('baseline','indexed','combined'):
    log=Path(f'out/a5g_{tag}/run.log')
    if not log.exists():
        print(tag,'missing'); continue
    text=log.read_text(errors='ignore')
    xs=[float(m.group(1)) for m in re.finditer(r'unique_xfb ([0-9.]+)', text)]
    mid=[x for x in xs if 3<=x<=25]
    title=[x for x in xs if x>=50]
    print(f'{tag}: title_n={len(title)} title_mean={sum(title)/len(title) if title else 0:.2f} mid_n={len(mid)} mid_mean={sum(mid)/len(mid) if mid else 0:.2f}')
    for ln in text.splitlines():
        if 'A5e-present:' in ln and 'fifoIn=' in ln:
            print(' ', ln[ln.find('A5e'):][:400])
PY
    ;;
  *)
    echo "usage: $0 baseline|indexed|combined|compare" >&2
    exit 2
    ;;
esac
