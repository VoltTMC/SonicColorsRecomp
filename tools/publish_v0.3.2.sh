#!/bin/bash
# Push commits, tag v0.3.2, and publish the GitHub hotfix release with packaged assets.
# Run in a normal terminal (outside the agent sandbox) after: gh auth login -h github.com
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

APPIMAGE=$root/dist/SonicColorsRecomp-v0.3.2-linux-x86_64.AppImage
WINZIP=$root/dist/SonicColorsRecomp-v0.3.2-windows-x86_64.zip
[[ -f $APPIMAGE && -f $WINZIP ]] || { echo "missing packages under dist/; rebuild first"; exit 1; }

gh auth status
git push origin HEAD

author_name=$(git log -1 --format='%an')
author_email=$(git log -1 --format='%ae')
if git rev-parse -q --verify refs/tags/v0.3.2 >/dev/null; then
  echo "tag v0.3.2 already exists locally"
else
  git -c user.name="$author_name" -c user.email="$author_email" tag -a v0.3.2 -m "$(cat <<'EOF'
v0.3.2 — hotfix: updater install + Music Library catalog

EOF
)"
fi
git push origin v0.3.2

gh release create v0.3.2 \
  --title "v0.3.2 — Hotfix: updater install & Music Library" \
  --notes "$(cat <<'EOF'
## Summary
Hotfix over **v0.3.1**: Windows auto-update install after download, and Music Library catalog shipped in redistributables.

**You still need your own Europe / PAL (SNCP8P) disc.** No game data is included.

### Fixes
- **Auto-update install (Windows):** download succeeded but install failed because `apply_update.cmd` wrote `%%ZIP%%` so Expand-Archive never saw the real path. Replaced with `apply_update.ps1` (embedded paths, PID wait, robocopy, rollback, log).
- **Music Library (Windows):** redistributable omitted `mods/music/catalog.json`, so the Music tab was empty and Prepare reported a missing manifest. Catalog (114 tracks) is now packaged; discovery prefers the exe-adjacent catalog.

Also includes the v0.3.1 launcher tab click fix if you are upgrading from v0.3.0.

### Assets
| File | Platform |
|------|----------|
| `SonicColorsRecomp-v0.3.2-linux-x86_64.AppImage` | Linux |
| `SonicColorsRecomp-v0.3.2-windows-x86_64.zip` | Windows (portable) |

### Notes
- Music prepare/export still needs Python + cricodecs + vgmstream + ffmpeg on the host (see `tools/MUSIC_ASSET_WINDOWS.txt`).
- Native Windows WASAPI/DPI/pad checklist remains **UNVERIFIED** on real hardware.
EOF
)" \
  "$APPIMAGE" \
  "$WINZIP"

echo "Published: https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.2"
