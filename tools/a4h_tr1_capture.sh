#!/usr/bin/env bash
# A4h: matched Tropical Resort Act 1 outdoor capture (D3D11 + Vulkan).
# Extends A4g entry; writes to out/a4h_{d3d,vk}_tr1/ with metadata.
#
# IMPORTANT: run_nvidia_* wrappers use `env -i`. Pass SCR_* via `env` *after* the wrapper.
#
# Usage:
#   tools/a4h_tr1_capture.sh d3d11
#   tools/a4h_tr1_capture.sh vulkan
#   tools/a4h_tr1_capture.sh vulkan-val
#   tools/a4h_tr1_capture.sh compare   # blackish metric on existing shots
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
MODE="${1:-vulkan}"
NAND="${SCR_NAND:-out/test_nand}"
# Start shots after menu; denser in-stage sampling than A4g defaults.
SHOTS_EVERY="${SCR_A4H_SHOTS_EVERY:-40}"
SHOTS_START="${SCR_A4H_SHOTS_START:-1800}"
TIMEOUT_S="${SCR_A4H_TIMEOUT:-240}"

if [[ "$MODE" != compare && ! -d "$NAND" ]]; then
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

blackish_metric() {
  local dir="$1"
  python3 - <<'PY' "$dir"
import sys, struct
from pathlib import Path
d=Path(sys.argv[1])
files=sorted(d.glob('*.bmp'))
if not files:
    print(f'{d}: no bmps'); sys.exit(0)
pcts=[]
for p in files:
    b=p.read_bytes(); off=struct.unpack_from('<I',b,10)[0]
    w,h=struct.unpack_from('<ii',b,18); w,h=abs(w),abs(h)
    row=((w*24+31)//32)*4; near=0; tot=w*h
    for y in range(h):
        o=off+y*row
        for x in range(w):
            if b[o+x*3]+b[o+x*3+1]+b[o+x*3+2] < 24: near+=1
    pct=100.0*near/tot; pcts.append(pct)
    print(f'{p.name}: blackish={pct:.1f}%')
print(f'{d}: n={len(pcts)} mean={sum(pcts)/len(pcts):.1f}% min={min(pcts):.1f}% max={max(pcts):.1f}%')
# A4g comparable band: post-stg in-stage shots often mid/late numbered
late=pcts[len(pcts)//2:]
if late:
    print(f'{d}: late_half mean={sum(late)/len(late):.1f}% (A4g in-stage ref ~16–28%)')
PY
}

case "$MODE" in
  d3d11)
    mkdir -p out/a4h_d3d_tr1
    rm -f shot_*.bmp
    set +e
    timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_A2G_NATURAL_TRACE=1 \
      "${common[@]}" --renderer d3d11 | tee "out/a4h_d3d_tr1/run.log"
    set -e
    cp -f shot_*.bmp out/a4h_d3d_tr1/ 2>/dev/null || true
    {
      echo "renderer=d3d11"
      echo "gpu=via_run_nvidia_clean"
      echo "scale=1"
      echo "shots_every=$SHOTS_EVERY shots_start=$SHOTS_START"
      echo "input=tools/a2g_tr1_enter.txt"
      echo "nand=$NAND"
      echo "validation=0"
      date -Iseconds
    } > out/a4h_d3d_tr1/meta.txt
    echo "D3D shots → out/a4h_d3d_tr1/"
    blackish_metric out/a4h_d3d_tr1
    ;;
  vulkan)
    mkdir -p out/a4h_vk_tr1
    rm -f out/a4h_vk_shot_*.bmp
    set +e
    timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env \
      SCR_VK_DEVICE=3080 SCR_A2G_NATURAL_TRACE=1 SCR_VK_SHOT_PREFIX=out/a4h_vk_shot \
      "${common[@]}" --renderer vulkan | tee "out/a4h_vk_tr1/run.log"
    set -e
    cp -f out/a4h_vk_shot_*.bmp out/a4h_vk_tr1/ 2>/dev/null || true
    {
      echo "renderer=vulkan"
      echo "gpu=SCR_VK_DEVICE=3080"
      echo "scale=1"
      echo "shots_every=$SHOTS_EVERY shots_start=$SHOTS_START"
      echo "input=tools/a2g_tr1_enter.txt"
      echo "nand=$NAND"
      echo "validation=0"
      date -Iseconds
    } > out/a4h_vk_tr1/meta.txt
    echo "VK shots → out/a4h_vk_tr1/"
    blackish_metric out/a4h_vk_tr1
    ;;
  vulkan-val)
    mkdir -p out/a4h_vk_tr1
    set +e
    timeout "${SCR_A4H_TIMEOUT:-450}" tools/run_nvidia_vk_validation.sh env \
      SCR_A2G_NATURAL_TRACE=1 SCR_VK_SHOT_PREFIX=out/a4h_vk_shot \
      "${common[@]}" --renderer vulkan | tee "out/a4h_vk_tr1/run_val.log"
    set -e
    echo "VK+val → out/a4h_vk_tr1/run_val.log"
    ;;
  compare)
    mkdir -p out/a4h_compare
    blackish_metric out/a4h_d3d_tr1
    blackish_metric out/a4h_vk_tr1
    # Also report A4g baseline dirs if present
    [[ -d out/a4g_vk_tr1 ]] && blackish_metric out/a4g_vk_tr1 || true
    ;;
  *)
    echo "usage: $0 d3d11|vulkan|vulkan-val|compare" >&2
    exit 2
    ;;
esac

if [[ "$MODE" == d3d11 || "$MODE" == vulkan || "$MODE" == vulkan-val ]]; then
  log=""
  [[ "$MODE" == d3d11 ]] && log=out/a4h_d3d_tr1/run.log
  [[ "$MODE" == vulkan ]] && log=out/a4h_vk_tr1/run.log
  [[ "$MODE" == vulkan-val ]] && log=out/a4h_vk_tr1/run_val.log
  if [[ -n "$log" && -f "$log" ]]; then
    if rg -q "A2g-Stage: entered 'stg110'" "$log"; then
      echo "OK: stg110 stage entry confirmed"
    else
      echo "WARN: stg110 marker not found" >&2
      exit 3
    fi
    if [[ "$MODE" == vulkan || "$MODE" == vulkan-val ]]; then
      rg -n "DEVICE_LOST|Validation Error" "$log" | head -20 || echo "OK: no DEVICE_LOST/Validation Error lines"
      rg -n "efbCopy=|xfUp=" "$log" | tail -5 || true
    fi
  fi
fi
