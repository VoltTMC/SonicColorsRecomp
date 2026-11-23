#!/usr/bin/env bash
# Run SonicColorsRecomp on the discrete NVIDIA GPU with a clean env.
# Cursor/agent shells can pollute LD_LIBRARY_PATH and break DXVK CreateDevice,
# which previously forced an accidental RADV (iGPU) fallback.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# XAUTHORITY is required for some SDL/X11 fallbacks even under Wayland.
XA="${XAUTHORITY:-}"
if [[ -z "$XA" && -f "$HOME/.Xauthority" ]]; then XA="$HOME/.Xauthority"; fi
exec env -i \
  HOME="$HOME" USER="${USER:-$(id -un)}" \
  DISPLAY="${DISPLAY:-:0}" WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}" \
  XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}" \
  XDG_SESSION_TYPE="${XDG_SESSION_TYPE:-wayland}" \
  ${XA:+XAUTHORITY="$XA"} \
  PATH="/usr/bin:/bin:$ROOT/out" \
  VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/nvidia_icd.json \
  VK_LOADER_LAYERS_DISABLE='*' \
  SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}" \
  "$@"
