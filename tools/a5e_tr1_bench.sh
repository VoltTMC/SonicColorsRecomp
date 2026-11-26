#!/usr/bin/env bash
# A5e: TR1 texture-object benches (RTX 3080). Validation off for perf.
# Usage:
#   tools/a5e_tr1_bench.sh legacy|a5d|texobj|combined|tex_profile|compare
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAND="${SCR_NAND:-out/test_nand}"
TIMEOUT_S="${SCR_A5E_TIMEOUT:-90}"
SCRIPT="${SCR_A5E_INPUT:-tools/a4k_tr1_explore.txt}"
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
  mkdir -p "out/a5e_${tag}" out/a5e_tex_profile out/a5e_compare
  rm -f out/a5e_vk_shot_*.bmp "out/a5e_${tag}/"*.bmp
  set +e
  timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 \
    SCR_VK_SHOT_PREFIX=out/a5e_vk_shot "$@" \
    "${common[@]}" | tee "out/a5e_${tag}/run.log"
  set -e
  cp -f out/a5e_vk_shot_*.bmp "out/a5e_${tag}/" 2>/dev/null || true
  rg -n "A5e-|A5d-|unique_xfb|DIRECT|texObj|bpprof" "out/a5e_${tag}/run.log" | tail -60 || true
}

case "$MODE" in
  legacy)
    run_one legacy SCR_A5A_DIRECT=0 SCR_A5C_DIRECT_BP=0 SCR_A5D_DIRECT_HOT=0 SCR_A5D_DIRECT_KSEL=0 \
      SCR_A5E_DIRECT_TEXOBJ=0 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1
    ;;
  a5d)
    run_one a5d SCR_A5A_DIRECT=1 SCR_A5C_DIRECT_BP=1 SCR_A5D_DIRECT_HOT=1 SCR_A5D_DIRECT_KSEL=1 \
      SCR_A5E_DIRECT_TEXOBJ=0 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1
    ;;
  texobj)
    run_one direct SCR_A5A_DIRECT=0 SCR_A5C_DIRECT_BP=0 SCR_A5D_DIRECT_HOT=0 SCR_A5D_DIRECT_KSEL=0 \
      SCR_A5E_DIRECT_TEXOBJ=1 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1
    ;;
  combined)
    run_one combined SCR_A5A_DIRECT=1 SCR_A5C_DIRECT_BP=1 SCR_A5D_DIRECT_HOT=1 SCR_A5D_DIRECT_KSEL=1 \
      SCR_A5E_DIRECT_TEXOBJ=1 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1
    ;;
  tex_profile)
    run_one tex_profile SCR_A5A_DIRECT=0 SCR_A5C_DIRECT_BP=0 SCR_A5D_DIRECT_HOT=0 SCR_A5D_DIRECT_KSEL=0 \
      SCR_A5E_DIRECT_TEXOBJ=0 SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1
    ;;
  compare)
    python3 - <<'PY'
from pathlib import Path
import re
for tag in ('legacy','a5d','direct','combined','tex_profile'):
    log=Path(f'out/a5e_{tag}/run.log')
    if not log.exists():
        print(tag,'missing'); continue
    text=log.read_text(errors='ignore')
    xs=[float(m.group(1)) for m in re.finditer(r'unique_xfb ([0-9.]+)', text)]
    mid=[x for x in xs if 3<=x<=25]
    title=[x for x in xs if x>=50]
    print(f'{tag}: title_n={len(title)} title_mean={sum(title)/len(title) if title else 0:.2f} mid_n={len(mid)} mid_mean={sum(mid)/len(mid) if mid else 0:.2f}')
    for ln in text.splitlines():
        if ('A5e-' in ln or 'A5d-' in ln) and ('fifoIn=' in ln or 'bpprof-present: total=' in ln):
            print(' ', ln[ln.find('A5'):][:320])
PY
    ;;
  *)
    echo "usage: $0 legacy|a5d|texobj|combined|tex_profile|compare" >&2
    exit 2
    ;;
esac
