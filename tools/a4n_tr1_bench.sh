#!/usr/bin/env bash
# A4n: TR1 outdoor throughput attribution (RTX 3080). Validation off.
# Usage:
#   tools/a4n_tr1_bench.sh a4m          # A4m-like: 0x680 off via SCR_A4N_XF_FULL=1 + no desc cache
#   tools/a4n_tr1_bench.sh xf           # XF 0x680 + dedup (desc cache off)
#   tools/a4n_tr1_bench.sh desc         # descriptor cache only (XF full if SCR_A4N_XF_FULL)
#   tools/a4n_tr1_bench.sh chain        # safe present chain only
#   tools/a4n_tr1_bench.sh combined     # product defaults
#   tools/a4n_tr1_bench.sh compare
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAND="${SCR_NAND:-out/test_nand}"
TIMEOUT_S="${SCR_A4N_TIMEOUT:-300}"
SCRIPT="${SCR_A4N_INPUT:-tools/a4k_tr1_explore.txt}"
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
  mkdir -p "out/a4n_${tag}_tr1" out/a4n_profile out/a4n_compare
  rm -f out/a4n_vk_shot_*.bmp "out/a4n_${tag}_tr1/"*.bmp
  set +e
  timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_VK_DEVICE=3080 SCR_A4L_EFB_PROF=1 SCR_A4N_PROF=1 \
    SCR_VK_SHOT_PREFIX=out/a4n_vk_shot "$@" \
    "${common[@]}" | tee "out/a4n_${tag}_tr1/run.log"
  set -e
  cp -f out/a4n_vk_shot_*.bmp "out/a4n_${tag}_tr1/" 2>/dev/null || true
  {
    echo "tag=$tag"
    echo "gpu=SCR_VK_DEVICE=3080"
    echo "args=$*"
    date -Iseconds
  } > "out/a4n_${tag}_tr1/meta.txt"
  echo "--- unique_xfb (post-stg110) ---"
  awk '/stg110/{p=1} p&&/unique_xfb/{print; c++} c>=12{exit}' "out/a4n_${tag}_tr1/run.log" || true
  rg -n "A4n-present:|A4n:|inline/sync=|descHit|xfMemcpy" "out/a4n_${tag}_tr1/run.log" | tail -25 || true
}

case "$MODE" in
  a4m)
    # Approximate A4m byte behaviour: full 0x1000 XF + no desc cache + chain off.
    run_one a4m SCR_A4N_XF_FULL=1 SCR_A4N_DESC_CACHE=0 SCR_A4M_PRESENT_CHAIN=0 SCR_A4M_XF_DEDUP=1
    ;;
  xf)
    run_one xf SCR_A4N_DESC_CACHE=0 SCR_A4M_PRESENT_CHAIN=0 SCR_A4M_XF_DEDUP=1
    ;;
  desc)
    run_one desc SCR_A4N_XF_FULL=1 SCR_A4N_DESC_CACHE=1 SCR_A4M_PRESENT_CHAIN=0 SCR_A4M_XF_DEDUP=1
    ;;
  chain)
    run_one chain SCR_A4N_DESC_CACHE=0 SCR_A4N_XF_FULL=1 SCR_A4M_PRESENT_CHAIN=1 SCR_A4M_XF_DEDUP=1
    ;;
  combined)
    # Product defaults: 0x680 XF payload + dedup; desc cache opt-in (hang under investigation).
    run_one combined SCR_A4M_PRESENT_CHAIN=0 SCR_A4M_XF_DEDUP=1 SCR_A4N_DESC_CACHE=0
    ;;
  compare)
    python3 - <<'PY'
from pathlib import Path
import re, struct
def blackish(p):
    b=Path(p).read_bytes(); off=struct.unpack_from('<I',b,10)[0]
    w,h=struct.unpack_from('<ii',b,18); w,h=abs(w),abs(h)
    row=((w*24+31)//32)*4; n=blk=0
    for y in range(h):
        o=off+y*row
        for x in range(w):
            B,G,R=b[o+x*3:o+x*3+3]; n+=1
            if R+G+B<24: blk+=1
    return 100*blk/n if n else 0
def summarize(tag):
    d=Path(f'out/a4n_{tag}_tr1')
    if not d.exists():
        print(tag, 'missing'); return
    shots=sorted(d.glob('*.bmp'))
    late=shots[len(shots)//2:] if shots else []
    bh=[blackish(p) for p in late]
    print(f'{tag}: shots={len(shots)} late_half={sum(bh)/len(bh):.2f}%' if bh else f'{tag}: no shots')
    log=d/'run.log'
    if not log.exists(): return
    text=log.read_text(errors='ignore')
    xs=[]; after=False
    for line in text.splitlines():
        if 'stg110' in line: after=True
        if after and 'unique_xfb' in line:
            m=re.search(r'unique_xfb ([0-9.]+)', line)
            if m: xs.append(float(m.group(1)))
    live=[x for x in xs if x>=3]
    if live:
        print(f'{tag}: outdoor unique_xfb n={len(live)} mean={sum(live)/len(live):.2f} last20_mean={sum(live[-20:])/min(20,len(live)):.2f}')
    for pat in ('xfBytes=', 'xfMemcpyMs=', 'descHit=', 'drawFlushMs=', 'inline/sync='):
        hits=[ln for ln in text.splitlines() if pat in ln]
        if hits: print(f'{tag}: {hits[-1][-180:]}')
for tag in ('a4m','xf','desc','chain','combined'):
    summarize(tag)
PY
    ;;
  *)
    echo "usage: $0 a4m|xf|desc|chain|combined|compare" >&2
    exit 2
    ;;
esac
