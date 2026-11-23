#!/usr/bin/env bash
# Opt-in A3w shader-variant collection for one tagged area.
# Does not invent shaders — records real VSKey/PSKey bytes via SCR_SHADER_COLLECT.
#
# Usage:
#   tools/a3w_collect_area.sh <area_tag> [extra SonicColorsRecomp args...]
# Examples:
#   tools/a3w_collect_area.sh title --present-limit 120
#   tools/a3w_collect_area.sh tr1 --input-script tools/a2g_tr1_enter.txt --nand out/test_nand
#
# After several areas: python3 tools/shader_collect_merge.py <userdata>/shadercache
# Then: ./out/SonicColorsRecomp --precompile-shaders <dir>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
AREA="${1:?area tag required (title|menu|tr1|...)}"
shift || true
export SCR_SHADER_COLLECT=1
export SCR_SHADER_COLLECT_AREA="$AREA"
exec "$ROOT/tools/run_nvidia_clean.sh" env \
  SCR_SHADER_COLLECT=1 \
  SCR_SHADER_COLLECT_AREA="$AREA" \
  SCR_SHADER_CACHE_STATS=1 \
  SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}" \
  DISPLAY="${DISPLAY:-:0}" \
  XAUTHORITY="${XAUTHORITY:-$HOME/.Xauthority}" \
  "$ROOT/out/SonicColorsRecomp" --skip-launcher --nobox --mute --scale 1 --60fps "$@"
