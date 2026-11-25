#!/usr/bin/env bash
# A4l: TR1 outdoor throughput bench (RTX 3080). Validation off.
# Usage:
#   tools/a4l_tr1_bench.sh baseline   # SCR_A4L_EFB_SYNC=1 (A4k per-copy wait)
#   tools/a4l_tr1_bench.sh inline     # default GPU-ordered inline copies
#   tools/a4l_tr1_bench.sh compare    # blackish + unique_xfb extract from logs
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAND="${SCR_NAND:-out/test_nand}"
TIMEOUT_S="${SCR_A4L_TIMEOUT:-280}"
SCRIPT="${SCR_A4L_INPUT:-tools/a4k_tr1_explore.txt}"
MODE="${1:-}"

common=(
  ./out/SonicColorsRecomp
  --skip-launcher --scale 1 --no-vsync
  --nand "$NAND" --mute --renderer vulkan
  --input-script "$SCRIPT"
  --shots 40 --shots-start 1800
)

run_one() {
  local tag="$1"
  shift
  mkdir -p "out/a4l_${tag}_tr1" out/a4l_compare
  rm -f out/a4l_vk_shot_*.bmp
  set +e
  timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 SCR_A4L_EFB_PROF=1 \
    SCR_VK_SHOT_PREFIX=out/a4l_vk_shot "$@" \
    "${common[@]}" | tee "out/a4l_${tag}_tr1/run.log"
  set -e
  cp -f out/a4l_vk_shot_*.bmp "out/a4l_${tag}_tr1/" 2>/dev/null || true
  {
    echo "tag=$tag"
    echo "gpu=SCR_VK_DEVICE=3080"
    echo "scale=1"
    echo "input=$SCRIPT"
    date -Iseconds
  } > "out/a4l_${tag}_tr1/meta.txt"
  echo "--- unique_xfb (post-stg110 sample) ---"
  awk '/entered .stg110/{p=1} p&&/unique_xfb/{print; c++} c>=12{exit}' "out/a4l_${tag}_tr1/run.log" || true
  echo "--- efb counters ---"
  rg -n "inline/sync=|A4l-efb:|A4l: GPU-ordered|A4l: SCR_A4L_EFB_SYNC" "out/a4l_${tag}_tr1/run.log" | tail -20 || true
}

case "$MODE" in
  baseline)
    run_one baseline SCR_A4L_EFB_SYNC=1
    ;;
  inline)
    run_one inline
    ;;
  compare)
    python3 - <<'PY'
from pathlib import Path
import re, struct
def blackish(p):
    b=Path(p).read_bytes(); off=struct.unpack_from('<I',b,10)[0]
    w,h=struct.unpack_from('<ii',b,18); td=h<0; w,h=abs(w),abs(h)
    row=((w*24+31)//32)*4; n=blk=0
    for y in range(h):
        o=off+y*row
        for x in range(w):
            B,G,R=b[o+x*3:o+x*3+3]; n+=1
            if R+G+B<24: blk+=1
    return 100*blk/n if n else 0
for tag in ('baseline','inline'):
    d=Path(f'out/a4l_{tag}_tr1')
    if not d.exists():
        print(tag, 'missing'); continue
    shots=sorted(d.glob('*.bmp'))
    late=shots[len(shots)//2:] if shots else []
    bh=[blackish(p) for p in late]
    print(f'{tag}: shots={len(shots)} late_half mean={sum(bh)/len(bh):.2f}%' if bh else f'{tag}: no shots')
    log=d/'run.log'
    if log.exists():
        xs=[]
        text=log.read_text(errors='ignore')
        after=False
        for line in text.splitlines():
            if "entered 'stg110'" in line or 'entered \'stg110\'' in line: after=True
            if after and 'unique_xfb' in line:
                m=re.search(r'unique_xfb ([0-9.]+)', line)
                if m: xs.append(float(m.group(1)))
                if len(xs)>=20: break
        if xs:
            mid=xs[len(xs)//2:]
            print(f'{tag}: outdoor unique_xfb sample n={len(mid)} mean={sum(mid)/len(mid):.2f} min={min(mid):.2f} max={max(mid):.2f}')
PY
    ;;
  *)
    echo "usage: $0 baseline|inline|compare" >&2
    exit 2
    ;;
esac
