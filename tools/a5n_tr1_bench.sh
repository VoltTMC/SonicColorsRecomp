#!/usr/bin/env bash
# A5n: Vulkan XF upload / hash-compare (RTX 3080). Validation off.
# Usage:
#   tools/a5n_tr1_bench.sh baseline|xfopt|a5m_off|a5l_off|compare
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAND="${SCR_NAND:-out/test_nand}"
TIMEOUT_S="${SCR_A5N_TIMEOUT:-90}"
SCRIPT="${SCR_A5N_INPUT:-tools/a4k_tr1_explore.txt}"
MODE="${1:-}"

common=(
  ./out/SonicColorsRecomp
  --skip-launcher --scale 1 --no-vsync
  --nand "$NAND" --mute --renderer vulkan
  --input-script "$SCRIPT"
  --shots 10 --shots-start 1800
)

a5=(
  SCR_A5A_DIRECT=1 SCR_A5C_DIRECT_BP=1 SCR_A5D_DIRECT_HOT=1 SCR_A5D_DIRECT_KSEL=1
  SCR_A5E_DIRECT_TEXOBJ=1 SCR_A5F_DEFERRED_BP="${SCR_A5F_DEFERRED_BP:-0}"
  SCR_A5G_DIRECT_INDEXED_XF=1 SCR_A5H_INDEXED_XF_PLAYBACK=1 SCR_A5I_INDEXED_XF_APPLY=1
  SCR_A5J_INDEXED_XF_DISPATCH=1 SCR_A5K_INDEXED_XF_FLUSH_OPT=1
  SCR_A5A_PROF=1 SCR_A5B_PROF=1 SCR_A5C_BP_PROFILE=1
  SCR_A4M_PROF=1 SCR_A4N_PROF=1
)

run_one() {
  local tag="$1"
  shift
  mkdir -p "out/a5n_${tag}"
  rm -f out/a5n_vk_shot_*.bmp "out/a5n_${tag}/"*.bmp
  set +e
  timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 \
    SCR_VK_SHOT_PREFIX=out/a5n_vk_shot "$@" \
    "${common[@]}" | tee "out/a5n_${tag}/run.log"
  set -e
  cp -f out/a5n_vk_shot_*.bmp "out/a5n_${tag}/" 2>/dev/null || true
  rg -n "A5n-|A5l-|A5m-|A4n-present|unique_xfb|xfMs|hashMs|memcmpMs" \
    "out/a5n_${tag}/run.log" | tail -200 || true
}

case "$MODE" in
  baseline)
    run_one baseline "${a5[@]}" SCR_A5L_VULKAN_DRAW_OPT=1 SCR_A5M_VULKAN_GEOMETRY_STREAM_OPT=1 \
      SCR_A5N_VULKAN_XF_UPLOAD_OPT=0
    ;;
  xfopt)
    run_one xfopt "${a5[@]}" SCR_A5L_VULKAN_DRAW_OPT=1 SCR_A5M_VULKAN_GEOMETRY_STREAM_OPT=1 \
      SCR_A5N_VULKAN_XF_UPLOAD_OPT=1
    ;;
  a5m_off)
    run_one a5m_off "${a5[@]}" SCR_A5L_VULKAN_DRAW_OPT=1 SCR_A5M_VULKAN_GEOMETRY_STREAM_OPT=0 \
      SCR_A5N_VULKAN_XF_UPLOAD_OPT=1
    ;;
  a5l_off)
    run_one a5l_off "${a5[@]}" SCR_A5L_VULKAN_DRAW_OPT=0 SCR_A5M_VULKAN_GEOMETRY_STREAM_OPT=1 \
      SCR_A5N_VULKAN_XF_UPLOAD_OPT=1
    ;;
  compare)
    python3 - <<'PY'
from pathlib import Path
import re
for tag in ('baseline','xfopt','a5m_off','a5l_off'):
    log=Path(f'out/a5n_{tag}/run.log')
    if not log.exists():
        print(tag,'missing'); continue
    text=log.read_text(errors='ignore')
    xs=[float(m.group(1)) for m in re.finditer(r'unique_xfb ([0-9.]+)', text)]
    mid=[x for x in xs if 3<=x<=25]
    title=[x for x in xs if x>=50]
    print(f'{tag}: title_n={len(title)} title_mean={sum(title)/len(title) if title else 0:.2f} mid_n={len(mid)} mid_mean={sum(mid)/len(mid) if mid else 0:.2f}')
    for key in ('A5n-present:','A5l-present:','A5m-present:','A4n-present:'):
        lines=[ln for ln in text.splitlines() if key in ln]
        if lines: print(' ', lines[-1][lines[-1].find(key):][:640])
PY
    ;;
  *)
    echo "usage: $0 baseline|xfopt|a5m_off|a5l_off|compare" >&2
    exit 2
    ;;
esac
