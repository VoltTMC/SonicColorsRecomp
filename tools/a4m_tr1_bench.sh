#!/usr/bin/env bash
# A4m: TR1 outdoor throughput / present+XF attribution (RTX 3080). Validation off.
# Usage:
#   tools/a4m_tr1_bench.sh a4l          # A4l-equivalent: chain+dedup off
#   tools/a4m_tr1_bench.sh present      # present chain only
#   tools/a4m_tr1_bench.sh xf           # XF dedup only
#   tools/a4m_tr1_bench.sh combined     # chain + XF dedup (default A4m)
#   tools/a4m_tr1_bench.sh compare      # extract unique_xfb + counters
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAND="${SCR_NAND:-out/test_nand}"
TIMEOUT_S="${SCR_A4M_TIMEOUT:-280}"
SCRIPT="${SCR_A4M_INPUT:-tools/a4k_tr1_explore.txt}"
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
  mkdir -p "out/a4m_${tag}_tr1" out/a4m_profile
  rm -f out/a4m_vk_shot_*.bmp "out/a4m_${tag}_tr1/"*.bmp
  set +e
  timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 SCR_A4L_EFB_PROF=1 SCR_A4M_PROF=1 \
    SCR_VK_SHOT_PREFIX=out/a4m_vk_shot "$@" \
    "${common[@]}" | tee "out/a4m_${tag}_tr1/run.log"
  set -e
  cp -f out/a4m_vk_shot_*.bmp "out/a4m_${tag}_tr1/" 2>/dev/null || true
  {
    echo "tag=$tag"
    echo "gpu=SCR_VK_DEVICE=3080"
    echo "scale=1"
    echo "input=$SCRIPT"
    echo "args=$*"
    date -Iseconds
  } > "out/a4m_${tag}_tr1/meta.txt"
  echo "--- unique_xfb (post-stg110 sample) ---"
  awk '/entered .stg110/{p=1} p&&/unique_xfb/{print; c++} c>=12{exit}' "out/a4m_${tag}_tr1/run.log" || true
  echo "--- a4m/a4l counters ---"
  rg -n "inline/sync=|A4m-present:|A4m:|xfDedup|presentWaitMs|A4l-efb:" "out/a4m_${tag}_tr1/run.log" | tail -30 || true
}

case "$MODE" in
  a4l)
    # Reconstruct A4l behaviour: no present chain, no XF dedup.
    run_one a4l SCR_A4M_PRESENT_CHAIN=0 SCR_A4M_XF_DEDUP=0
    ;;
  present)
    run_one present SCR_A4M_PRESENT_CHAIN=1 SCR_A4M_XF_DEDUP=0
    ;;
  xf)
    run_one xf SCR_A4M_PRESENT_CHAIN=0 SCR_A4M_XF_DEDUP=1
    ;;
  combined)
    # Default product config: XF dedup on; present chain opt-in (hang under investigation).
    run_one combined SCR_A4M_PRESENT_CHAIN=0 SCR_A4M_XF_DEDUP=1
    ;;
  chain)
    run_one chain SCR_A4M_PRESENT_CHAIN=1 SCR_A4M_XF_DEDUP=1
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
def summarize(tag):
    d=Path(f'out/a4m_{tag}_tr1')
    if not d.exists():
        print(tag, 'missing'); return
    shots=sorted(d.glob('*.bmp'))
    late=shots[len(shots)//2:] if shots else []
    bh=[blackish(p) for p in late]
    print(f'{tag}: shots={len(shots)} late_half mean={sum(bh)/len(bh):.2f}%' if bh else f'{tag}: no shots')
    log=d/'run.log'
    if not log.exists(): return
    text=log.read_text(errors='ignore')
    xs=[]; after=False
    for line in text.splitlines():
        if "entered 'stg110'" in line or "entered \"stg110\"" in line: after=True
        if after and 'unique_xfb' in line:
            m=re.search(r'unique_xfb ([0-9.]+)', line)
            if m: xs.append(float(m.group(1)))
            if len(xs)>=20: break
    if xs:
        mid=xs[len(xs)//2:]
        print(f'{tag}: outdoor unique_xfb sample n={len(mid)} mean={sum(mid)/len(mid):.2f} min={min(mid):.2f} max={max(mid):.2f}')
    for pat in ('xfDedup=', 'presentWaitMs=', 'drawFlushMs=', 'inline/sync=', 'xfBytes='):
        hits=[ln for ln in text.splitlines() if pat in ln]
        if hits: print(f'{tag}: last {pat!r}: {hits[-1][-160:]}')
for tag in ('a4l','present','xf','combined','chain'):
    summarize(tag)
PY
    ;;
  *)
    echo "usage: $0 a4l|present|xf|combined|chain|compare" >&2
    exit 2
    ;;
esac
