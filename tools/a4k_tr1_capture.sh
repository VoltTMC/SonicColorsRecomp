#!/usr/bin/env bash
# A4k: expanded Tropical Resort Act 1 visual sign-off captures (D3D11 + Vulkan).
# Keeps out/a4j_* and out/a4i_* intact. Writes out/a4k_{d3d,vk}_tr1/ + out/a4k_compare/.
#
# IMPORTANT: run_nvidia_* wrappers use `env -i`. Pass SCR_* via `env` *after* the wrapper.
#
# Usage:
#   tools/a4k_tr1_capture.sh d3d11
#   tools/a4k_tr1_capture.sh vulkan
#   tools/a4k_tr1_capture.sh vulkan-val
#   tools/a4k_tr1_capture.sh compare
#   tools/a4k_tr1_capture.sh mae
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
MODE="${1:-vulkan}"
NAND="${SCR_NAND:-out/test_nand}"
SHOTS_EVERY="${SCR_A4K_SHOTS_EVERY:-30}"
SHOTS_START="${SCR_A4K_SHOTS_START:-1800}"
TIMEOUT_S="${SCR_A4K_TIMEOUT:-320}"
SCRIPT="${SCR_A4K_INPUT:-tools/a4k_tr1_explore.txt}"

if [[ "$MODE" != compare && "$MODE" != mae && ! -d "$NAND" ]]; then
  echo "missing NAND $NAND — run tools/setup_a2g_test_nand.sh" >&2
  exit 1
fi

common=(
  ./out/SonicColorsRecomp
  --skip-launcher --scale 1 --no-vsync
  --nand "$NAND"
  --input-script "$SCRIPT"
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
late=pcts[len(pcts)//2:]
if late:
    print(f'{d}: late_half mean={sum(late)/len(late):.1f}% (A4j VK ~0.5%, D3D ~0.2%)')
PY
}

sky_geom_metric() {
  local dir="$1"
  python3 - <<'PY' "$dir"
import sys, struct
from pathlib import Path
d=Path(sys.argv[1])
files=sorted(d.glob('*.bmp'))
if not files:
    print(f'{d}: no bmps for sky/geom'); sys.exit(0)
def pct(b, off, w, h, y0, y1):
    row=((w*24+31)//32)*4; near=0; tot=0
    for y in range(y0, y1):
        o=off+y*row
        for x in range(w):
            tot+=1
            if b[o+x*3]+b[o+x*3+1]+b[o+x*3+2] < 24: near+=1
    return 100.0*near/tot if tot else 0.0
sky=[]; geom=[]
for p in files[len(files)//2:]:
    b=p.read_bytes(); off=struct.unpack_from('<I',b,10)[0]
    w,h=struct.unpack_from('<ii',b,18); w,h=abs(w),abs(h)
    sky.append(pct(b,off,w,h,0,h//3))
    geom.append(pct(b,off,w,h,h//3,h))
print(f'{d}: late sky_top_third blackish mean={sum(sky)/len(sky):.1f}%')
print(f'{d}: late geom_bot_two_thirds blackish mean={sum(geom)/len(geom):.1f}%')
PY
}

mae_compare() {
  mkdir -p out/a4k_compare
  python3 - <<'PY'
import struct
from pathlib import Path
from PIL import Image
def load(p):
    b=Path(p).read_bytes(); off=struct.unpack_from('<I',b,10)[0]
    w,h=struct.unpack_from('<ii',b,18); td=h<0; w,h=abs(w),abs(h)
    row=((w*24+31)//32)*4
    im=Image.new('RGB',(w,h)); px=im.load()
    for y in range(h):
        o=off+y*row; iy=y if td else h-1-y
        for x in range(w):
            B,G,R=b[o+x*3:o+x*3+3]; px[x,iy]=(R,G,B)
    return im
def mae(a,b):
    if a.size!=b.size: return None
    w,h=a.size; s=0; n=w*h
    pa,pb=a.load(),b.load()
    for y in range(h):
        for x in range(w):
            ra,ga,ba=pa[x,y]; rb,gb,bb=pb[x,y]
            s += abs(ra-rb)+abs(ga-gb)+abs(ba-bb)
    return s/(3.0*n)
vk=sorted(Path('out/a4k_vk_tr1').glob('*.bmp'))
d3=sorted(Path('out/a4k_d3d_tr1').glob('*.bmp'))
# Pair by ordinal in late half (landmarks, not raw XFB identity).
n=min(len(vk),len(d3))
if n<4:
    print(f'mae: need more shots (vk={len(vk)} d3d={len(d3)})'); raise SystemExit(0)
i0=n//2
pairs=[]
for i in range(i0, n):
    a,b=load(vk[i]),load(d3[i])
    m=mae(a,b)
    if m is None: continue
    name=f'pair_{i:04d}'
    a.save(f'out/a4k_compare/vk_{name}.png'); b.save(f'out/a4k_compare/d3d_{name}.png')
    pairs.append((name,m,vk[i].name,d3[i].name))
    print(f'{name}: MAE={m:.2f} ({vk[i].name} vs {d3[i].name})')
if pairs:
    vals=[m for _,m,_,_ in pairs]
    print(f'mae late pairs n={len(vals)} mean={sum(vals)/len(vals):.2f} min={min(vals):.2f} max={max(vals):.2f}')
    print('NOTE: ordinal pairing is landmark-ish only; high MAE may be capture timing, not shader error.')
PY
}

case "$MODE" in
  d3d11)
    mkdir -p out/a4k_d3d_tr1
    rm -f shot_*.bmp out/a4k_d3d_tr1/*.bmp
    set +e
    # SCR_SHOTS_FULL=1: native XFB BMP size (matches Vulkan EFB shots; default D3D dumps are 2x downscaled).
    timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env SCR_A2G_NATURAL_TRACE=1 SCR_A4K_FOG_STATS=1 SCR_SHOTS_FULL=1 \
      "${common[@]}" --renderer d3d11 | tee "out/a4k_d3d_tr1/run.log"
    set -e
    cp -f shot_*.bmp out/a4k_d3d_tr1/ 2>/dev/null || true
    {
      echo "renderer=d3d11"
      echo "scale=1"
      echo "shots_full=1"
      echo "shots_every=$SHOTS_EVERY shots_start=$SHOTS_START"
      echo "input=$SCRIPT"
      date -Iseconds
    } > out/a4k_d3d_tr1/meta.txt
    blackish_metric out/a4k_d3d_tr1
    sky_geom_metric out/a4k_d3d_tr1
    ;;
  vulkan)
    mkdir -p out/a4k_vk_tr1
    rm -f out/a4k_vk_shot_*.bmp
    set +e
    timeout "$TIMEOUT_S" tools/run_nvidia_clean.sh env \
      SCR_VK_DEVICE=3080 SCR_A2G_NATURAL_TRACE=1 SCR_A4K_FOG_STATS=1 SCR_VK_SHOT_PREFIX=out/a4k_vk_shot \
      "${common[@]}" --renderer vulkan | tee "out/a4k_vk_tr1/run.log"
    set -e
    cp -f out/a4k_vk_shot_*.bmp out/a4k_vk_tr1/ 2>/dev/null || true
    {
      echo "renderer=vulkan"
      echo "gpu=SCR_VK_DEVICE=3080"
      echo "scale=1"
      echo "shots_every=$SHOTS_EVERY shots_start=$SHOTS_START"
      echo "input=$SCRIPT"
      date -Iseconds
    } > out/a4k_vk_tr1/meta.txt
    blackish_metric out/a4k_vk_tr1
    sky_geom_metric out/a4k_vk_tr1
    ;;
  vulkan-val)
    mkdir -p out/a4k_vk_tr1
    set +e
    timeout "${SCR_A4K_TIMEOUT:-500}" tools/run_nvidia_vk_validation.sh env \
      SCR_A2G_NATURAL_TRACE=1 SCR_VK_SHOT_PREFIX=out/a4k_vk_shot \
      "${common[@]}" --renderer vulkan | tee "out/a4k_vk_tr1/run_val.log"
    set -e
    ;;
  compare)
    mkdir -p out/a4k_compare
    blackish_metric out/a4k_d3d_tr1
    sky_geom_metric out/a4k_d3d_tr1
    blackish_metric out/a4k_vk_tr1
    sky_geom_metric out/a4k_vk_tr1
    ;;
  mae)
    mae_compare
    ;;
  *)
    echo "usage: $0 d3d11|vulkan|vulkan-val|compare|mae" >&2
    exit 2
    ;;
esac

if [[ "$MODE" == d3d11 || "$MODE" == vulkan || "$MODE" == vulkan-val ]]; then
  log=""
  [[ "$MODE" == d3d11 ]] && log=out/a4k_d3d_tr1/run.log
  [[ "$MODE" == vulkan ]] && log=out/a4k_vk_tr1/run.log
  [[ "$MODE" == vulkan-val ]] && log=out/a4k_vk_tr1/run_val.log
  if [[ -n "$log" && -f "$log" ]]; then
    if rg -q "A2g-Stage: entered 'stg110'" "$log"; then
      echo "OK: stg110 stage entry confirmed"
    else
      echo "WARN: stg110 marker not found" >&2
      exit 3
    fi
    rg -n "A4k-fog|unique_xfb|efbCopy|xfUp|DEVICE_LOST|Validation Error" "$log" | tail -40 || true
  fi
fi
