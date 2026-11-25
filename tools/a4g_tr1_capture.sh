#!/usr/bin/env bash
# A4g: repeatable Tropical Resort Act 1 entry + screenshot capture.
# Uses guest-timed input (tools/a2g_tr1_enter.txt) against out/test_nand.
#
# IMPORTANT: run_nvidia_* wrappers use `env -i`. Pass SCR_* via `env` *after* the wrapper.
#
# Usage:
#   tools/a4g_tr1_capture.sh d3d11          # reference (~60 unique_xfb)
#   tools/a4g_tr1_capture.sh vulkan         # visual (no validation)
#   tools/a4g_tr1_capture.sh vulkan-val     # correctness + validation (slow)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
MODE="${1:-vulkan}"
NAND="${SCR_NAND:-out/test_nand}"
SHOTS_EVERY="${SCR_A4G_SHOTS_EVERY:-60}"
SHOTS_START="${SCR_A4G_SHOTS_START:-1900}"
TIMEOUT_S="${SCR_A4G_TIMEOUT:-200}"

if [[ ! -d "$NAND" ]]; then
  echo "missing NAND $NAND — run tools/setup_a2g_test_nand.sh" >&2
  exit 1
fi

common=(
  ./out/SonicColorsRecomp
  --skip-launcher --scale 1 --no-vsync
  --nand "$NAND"
  --input-script tools/a2g_tr1_enter.txt
  --mute --music-debug
  --shots "$SHOTS_EVERY" --shots-start "$SHOTS_START"
)

case "$MODE" in
  d3d11)
    mkdir -p out/a4g_d3d_tr1
    rm -f shot_*.bmp
    timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_A2G_NATURAL_TRACE=1 \
      "${common[@]}" --renderer d3d11 | tee "out/a4g_d3d_tr1/run.log"
    cp -f shot_*.bmp out/a4g_d3d_tr1/ 2>/dev/null || true
    echo "D3D shots → out/a4g_d3d_tr1/; expect A2g-Stage stg110 in run.log"
    ;;
  vulkan)
    mkdir -p out/a4g_vk_tr1
    rm -f out/a4g_vk_shot_*.bmp
    timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 SCR_A2G_NATURAL_TRACE=1 \
      "${common[@]}" --renderer vulkan | tee "out/a4g_vk_tr1/run.log"
    cp -f out/a4g_vk_shot_*.bmp out/a4g_vk_tr1/ 2>/dev/null || true
    echo "VK shots → out/a4g_vk_tr1/; expect A2g-Stage stg110 in run.log"
    ;;
  vulkan-val)
    mkdir -p out/a4g_vk_tr1
    # Validation slows guest time; raise timeout. Stage entry still works if long enough.
    timeout "${SCR_A4G_TIMEOUT:-400}" tools/run_nvidia_vk_validation.sh env SCR_A2G_NATURAL_TRACE=1 \
      "${common[@]}" --renderer vulkan | tee "out/a4g_vk_tr1/run_val.log"
    echo "VK+val log → out/a4g_vk_tr1/run_val.log"
    ;;
  *)
    echo "usage: $0 d3d11|vulkan|vulkan-val" >&2
    exit 2
    ;;
esac

# Marker check
log=""
[[ "$MODE" == d3d11 ]] && log=out/a4g_d3d_tr1/run.log
[[ "$MODE" == vulkan ]] && log=out/a4g_vk_tr1/run.log
[[ "$MODE" == vulkan-val ]] && log=out/a4g_vk_tr1/run_val.log
if [[ -n "$log" && -f "$log" ]]; then
  if rg -q "A2g-Stage: entered 'stg110'" "$log"; then
    echo "OK: stg110 stage entry confirmed"
  else
    echo "WARN: stg110 marker not found (increase timeout / check NAND)" >&2
    exit 3
  fi
fi
