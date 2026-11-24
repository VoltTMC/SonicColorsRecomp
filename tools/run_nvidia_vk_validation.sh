#!/usr/bin/env bash
# NVIDIA RTX Vulkan run with Khronos validation enabled.
# Uses the same clean ICD env as run_nvidia_clean.sh, but does NOT disable layers.
# Prefers repo-local out/vvl (copied from a host Vulkan SDK / UE distribution) when present.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
XA="${XAUTHORITY:-}"
if [[ -z "$XA" && -f "$HOME/.Xauthority" ]]; then XA="$HOME/.Xauthority"; fi
ICD=/usr/share/vulkan/icd.d/nvidia_icd.json
if [[ ! -f "$ICD" ]]; then
  echo "error: missing $ICD" >&2
  exit 1
fi

LAYER_PATH=""
LAYER_LIB=""
if [[ -f "$ROOT/out/vvl/VkLayer_khronos_validation.json" && -f "$ROOT/out/vvl/libVkLayer_khronos_validation.so" ]]; then
  LAYER_PATH="$ROOT/out/vvl"
  LAYER_LIB="$ROOT/out/vvl"
elif [[ -f /opt/unreal-engine/Engine/Binaries/ThirdParty/Vulkan/Linux/VkLayer_khronos_validation.json ]]; then
  LAYER_PATH=/opt/unreal-engine/Engine/Binaries/ThirdParty/Vulkan/Linux
  LAYER_LIB=/opt/unreal-engine/Engine/Binaries/ThirdParty/Vulkan/Linux
fi

exec env -i \
  HOME="$HOME" USER="${USER:-$(id -un)}" \
  DISPLAY="${DISPLAY:-:0}" WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}" \
  XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}" \
  XDG_SESSION_TYPE="${XDG_SESSION_TYPE:-wayland}" \
  ${XA:+XAUTHORITY="$XA"} \
  PATH="/usr/bin:/bin:$ROOT/out" \
  VK_ICD_FILENAMES="$ICD" \
  ${LAYER_PATH:+VK_LAYER_PATH="$LAYER_PATH"} \
  ${LAYER_LIB:+LD_LIBRARY_PATH="$LAYER_LIB"} \
  VK_LOADER_LAYERS_DISABLE=* \
  VK_LOADER_LAYERS_ENABLE=VK_LAYER_KHRONOS_validation \
  SCR_VK_VALIDATION=1 \
  SCR_VK_DEVICE="${SCR_VK_DEVICE:-3080}" \
  SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}" \
  "$@"
