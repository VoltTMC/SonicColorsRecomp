#!/bin/bash
# Push commits, tag v0.3.1, and publish the GitHub hotfix release with packaged assets.
# Run in a normal terminal (outside the agent sandbox) after: gh auth login -h github.com
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

APPIMAGE=$root/dist/SonicColorsRecomp-v0.3.1-linux-x86_64.AppImage
WINZIP=$root/dist/SonicColorsRecomp-v0.3.1-windows-x86_64.zip
[[ -f $APPIMAGE && -f $WINZIP ]] || { echo "missing packages under dist/; rebuild first"; exit 1; }

gh auth status
git push origin HEAD

author_name=$(git log -1 --format='%an')
author_email=$(git log -1 --format='%ae')
if git rev-parse -q --verify refs/tags/v0.3.1 >/dev/null; then
  echo "tag v0.3.1 already exists locally"
else
  git -c user.name="$author_name" -c user.email="$author_email" tag -a v0.3.1 -m "$(cat <<'EOF'
v0.3.1 — hotfix: clickable launcher tabs

EOF
)"
fi
git push origin v0.3.1

gh release create v0.3.1 \
  --title "v0.3.1 — Hotfix: clickable launcher tabs" \
  --notes "$(cat <<'EOF'
## Summary
Hotfix over **v0.3.0**: launcher tabs ignored mouse clicks after boot.

**You still need your own Europe / PAL (SNCP8P) disc.** No game data is included.

### Fix
- Boot `forceTab` could stay asserted forever when frames never advanced, so `ImGuiTabItemFlags_SetSelected` kept cancelling mouse tab selection.
- Uses a dedicated `bootFrames` counter (same pattern as Settings sub-tabs).

### Assets
| File | Platform |
|------|----------|
| `SonicColorsRecomp-v0.3.1-linux-x86_64.AppImage` | Linux |
| `SonicColorsRecomp-v0.3.1-windows-x86_64.zip` | Windows (portable) |

### Notes
- Otherwise identical to v0.3.0 feature set.
- Native Windows WASAPI/DPI/pad checklist remains **UNVERIFIED** on real hardware.
EOF
)" \
  "$APPIMAGE" \
  "$WINZIP"

echo "Published: https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.1"
