#!/usr/bin/env bash
# Fast automated regression net (no long gameplay). Requires a prior linux-clang build in out/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/out"
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

[[ -x "$OUT/SonicColorsRecomp" ]] || fail "build $OUT/SonicColorsRecomp first"

run() {
  local name=$1; shift
  echo "== $name =="
  "$@"
  ok "$name"
}

run scr_nand_fs_test "$OUT/scr_nand_fs_test"
run scr_nand_backup_test "$OUT/scr_nand_backup_test"
run scr_config_test "$OUT/scr_config_test"
if [[ -x "$OUT/scr_shader_cache_test" ]]; then
  run scr_shader_cache_test "$OUT/scr_shader_cache_test"
else
  echo "building scr_shader_cache_test..."
  cmake --build --preset linux-clang --target scr_shader_cache_test -j"$(nproc 2>/dev/null || echo 4)"
  run scr_shader_cache_test "$OUT/scr_shader_cache_test"
fi
if [[ -x "$OUT/scr_a3b_camera_math_test" ]]; then
  run scr_a3b_camera_math_test "$OUT/scr_a3b_camera_math_test"
else
  echo "building scr_a3b_camera_math_test..."
  cmake --build --preset linux-clang --target scr_a3b_camera_math_test -j"$(nproc 2>/dev/null || echo 4)"
  run scr_a3b_camera_math_test "$OUT/scr_a3b_camera_math_test"
fi
if [[ -x "$OUT/scr_pose_capture_test" ]]; then
  run scr_pose_capture_test "$OUT/scr_pose_capture_test"
else
  echo "building scr_pose_capture_test..."
  cmake --build --preset linux-clang --target scr_pose_capture_test -j"$(nproc 2>/dev/null || echo 4)"
  run scr_pose_capture_test "$OUT/scr_pose_capture_test"
fi
if [[ -x "$OUT/scr_install_helpers_test" ]]; then
  run scr_install_helpers_test "$OUT/scr_install_helpers_test"
else
  echo "building scr_install_helpers_test..."
  cmake --build --preset linux-clang --target scr_install_helpers_test -j"$(nproc 2>/dev/null || echo 4)"
  run scr_install_helpers_test "$OUT/scr_install_helpers_test"
fi
if [[ -x "$OUT/scr_release_notes_test" ]]; then
  run scr_release_notes_test "$OUT/scr_release_notes_test"
else
  echo "building scr_release_notes_test..."
  cmake --build --preset linux-clang --target scr_release_notes_test -j"$(nproc 2>/dev/null || echo 4)"
  run scr_release_notes_test "$OUT/scr_release_notes_test"
fi
run install_manifest_check "$ROOT/tools/install_manifest_check.sh"
run package_smoke "$ROOT/tools/package_smoke.sh"

if [[ -f "$ROOT/game/SNCP8P/files/sonic2010_0.cpk" ]]; then
  run music_pipeline_smoke "$ROOT/tools/music_pipeline_smoke.sh"
else
  echo "SKIP: music_pipeline_smoke (no game CPK)"
fi

echo "regression_smoke OK"
