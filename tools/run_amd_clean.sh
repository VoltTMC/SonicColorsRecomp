#!/usr/bin/env bash
# Run SonicColorsRecomp on the AMD integrated GPU (RADV) with a clean env.
# Forces radeon ICD only — do not use NVIDIA ICD vars here.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
XA="${XAUTHORITY:-}"
if [[ -z "$XA" && -f "$HOME/.Xauthority" ]]; then XA="$HOME/.Xauthority"; fi
ICD=/usr/share/vulkan/icd.d/radeon_icd.json
if [[ ! -f "$ICD" ]]; then
  echo "error: missing $ICD" >&2
  exit 1
fi
exec env -i \
  HOME="$HOME" USER="${USER:-$(id -un)}" \
  DISPLAY="${DISPLAY:-:0}" WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}" \
  XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}" \
  XDG_SESSION_TYPE="${XDG_SESSION_TYPE:-wayland}" \
  ${XA:+XAUTHORITY="$XA"} \
  PATH="/usr/bin:/bin:$ROOT/out" \
  VK_ICD_FILENAMES="$ICD" \
  VK_LOADER_LAYERS_DISABLE='*' \
  SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}" \
  "$@"
