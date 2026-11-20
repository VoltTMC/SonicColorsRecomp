#!/bin/bash
# Push commits, tag v0.3.0, and publish the GitHub release with packaged assets.
# Run in a normal terminal (outside the agent sandbox) after: gh auth login -h github.com
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

APPIMAGE=$root/dist/SonicColorsRecomp-v0.3.0-linux-x86_64.AppImage
WINZIP=$root/dist/SonicColorsRecomp-v0.3.0-windows-x86_64.zip
[[ -f $APPIMAGE && -f $WINZIP ]] || { echo "missing packages under dist/; rebuild first"; exit 1; }

gh auth status
git push origin HEAD

author_name=$(git log -1 --format='%an')
author_email=$(git log -1 --format='%ae')
if git rev-parse -q --verify refs/tags/v0.3.0 >/dev/null; then
  echo "tag v0.3.0 already exists locally"
else
  git -c user.name="$author_name" -c user.email="$author_email" tag -a v0.3.0 -m "$(cat <<'EOF'
v0.3.0 — launcher, updater, and PC QoL polish

EOF
)"
fi
git push origin v0.3.0

gh release create v0.3.0 \
  --title "v0.3.0 — Launcher & updater polish" \
  --notes "$(cat <<'EOF'
## Summary
PC redistributable covering the post-v0.2.0 launcher, updater, and QoL batches.

**You still need your own Europe / PAL (SNCP8P) disc.** No game data is included.

### Highlights
- **RecompLauncher** settings/actions, versioned window titles
- **Updater:** download packages, AppImage / Windows zip Install & relaunch, release notes
- **Launcher:** last played, reinstall confirm, Copy paths, crash banner, disc format discovery
- **Recovery:** `--verify-data` / `--verify-data-quick`, Clear shader cache, richer diagnostics
- **Window:** remember size + maximized; pause when minimized
- **CLI:** `--list-gamepads`, `--save-diagnostics`, `--download-update`, `--check-updates`

### Assets
| File | Platform |
|------|----------|
| `SonicColorsRecomp-v0.3.0-linux-x86_64.AppImage` | Linux |
| `SonicColorsRecomp-v0.3.0-windows-x86_64.zip` | Windows (portable) |

### Notes
- Native Windows WASAPI/DPI/pad checklist remains **UNVERIFIED** on real hardware.
- H7 black-screen and AP1 spiral remain deferred.
EOF
)" \
  "$APPIMAGE" \
  "$WINZIP"

echo "Published: https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.0"
