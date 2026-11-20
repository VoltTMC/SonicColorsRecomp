#!/bin/bash
# Push commits, tag v0.3.3, and publish the GitHub hotfix release with packaged assets.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

APPIMAGE=$root/dist/SonicColorsRecomp-v0.3.3-linux-x86_64.AppImage
WINZIP=$root/dist/SonicColorsRecomp-v0.3.3-windows-x86_64.zip
[[ -f $APPIMAGE && -f $WINZIP ]] || { echo "missing packages under dist/; rebuild first"; exit 1; }

gh auth status
git push origin HEAD

author_name=$(git log -1 --format='%an')
author_email=$(git log -1 --format='%ae')
if git rev-parse -q --verify refs/tags/v0.3.3 >/dev/null; then
  echo "tag v0.3.3 already exists locally"
else
  git -c user.name="$author_name" -c user.email="$author_email" tag -a v0.3.3 -m "$(cat <<'EOF'
v0.3.3 — hotfix: Music Library deps doctor & packaging

EOF
)"
fi
git push origin v0.3.3

gh release create v0.3.3 \
  --title "v0.3.3 — Hotfix: Music Library dependencies" \
  --notes "$(cat <<'EOF'
## Summary
Hotfix over **v0.3.2**: Music Library host-tool discovery, setup helpers, and Windows `music_asset.exe` packaging so prepare/preview/export can be configured without hunting PATH issues.

**You still need your own Europe / PAL (SNCP8P) disc.** No game data is included.

### Fixes & improvements
- **Music doctor:** reports which operations work immediately vs which need Python/cricodecs/vgmstream/ffmpeg; executes drop-ins and flags `missing` vs `unusable`.
- **Setup:** `tools/music_deps_setup` (Linux/Windows) creates `.venv-music` + cricodecs and prints Ready/Blocked/Next steps.
- **Drop-ins:** place LGPL `ffmpeg` + ISC `vgmstream-cli` in `tools/bin/` (no GPL FFmpeg bundled).
- **Windows:** ship `libwinpthread-1.dll` next to `tools/music_asset.exe` so the helper loads from a clean ZIP extract.
- **UI:** Music tab → **Check tools** shows ready|blocked ops.
- Also includes v0.3.2 updater install + catalog packaging if upgrading from earlier builds.

### Assets
| File | Platform |
|------|----------|
| `SonicColorsRecomp-v0.3.3-linux-x86_64.AppImage` | Linux |
| `SonicColorsRecomp-v0.3.3-windows-x86_64.zip` | Windows (portable) |

### Notes
- Browse catalog + replacement WAV preview work without host tools.
- Prepare / original preview / export still need host Python + cricodecs + vgmstream + ffmpeg (see `tools/MUSIC_ASSET_WINDOWS.txt` / `tools/bin/README.md`).
- Native Windows WASAPI/DPI/pad checklist remains **UNVERIFIED** on real hardware.
EOF
)" \
  "$APPIMAGE" \
  "$WINZIP"

echo "Published: https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.3"
