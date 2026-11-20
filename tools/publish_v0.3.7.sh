#!/bin/bash
# Tag v0.3.7 and publish GitHub hotfix: AppImage + Windows Setup.exe (+ install-layout zip for updater).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

APPIMAGE=$root/dist/SonicColorsRecomp-v0.3.7-linux-x86_64.AppImage
SETUP=$root/dist/SonicColorsRecomp-Setup-v0.3.7.exe
WINZIP=$root/dist/SonicColorsRecomp-v0.3.7-windows-x86_64.zip
[[ -f $APPIMAGE && -f $SETUP && -f $WINZIP ]] || {
  echo "missing packages under dist/ (need AppImage, Setup.exe, and install-layout zip)" >&2
  exit 1
}

gh auth status
git push origin HEAD

author_name=$(git log -1 --format='%an')
author_email=$(git log -1 --format='%ae')
if git rev-parse -q --verify refs/tags/v0.3.7 >/dev/null; then
  echo "tag v0.3.7 already exists locally"
else
  git -c user.name="$author_name" -c user.email="$author_email" tag -a v0.3.7 -m "$(cat <<'EOF'
v0.3.7 — hotfix: Music preview, deps persistence, Music tab performance

EOF
)"
fi
git push origin v0.3.7

gh release create v0.3.7 \
  --title "v0.3.7 — Hotfix: Music preview + deps + performance" \
  --notes "$(cat <<'EOF'
## Summary
Hotfix over **v0.3.6**: original Music preview, Windows managed Music tools, and Music tab responsiveness.

**You still need your own Europe / PAL (SNCP8P) disc.** No game data is included.

### Fixes
- **Original preview:** use `vgmstream-cli` (decoder), not `vgmstream123` (player)
- **Windows Install Music tools:** cricodecs binary wheel verification, embed Python site-packages / Check tools, install persistence across relaunches
- **Music tab performance:** stop per-frame full WAV loads; list virtualization; cached dependency status

### Assets
| File | Platform |
|------|----------|
| `SonicColorsRecomp-Setup-v0.3.7.exe` | **Windows (primary)** — per-user installer |
| `SonicColorsRecomp-v0.3.7-windows-x86_64.zip` | Windows update / advanced (install layout) |
| `SonicColorsRecomp-v0.3.7-linux-x86_64.AppImage` | Linux |

See `docs/MUSIC_LIBRARY_PERFORMANCE.md`, `docs/WINDOWS_INSTALLER.md`, and `tools/MUSIC_ASSET_WINDOWS.txt`.
EOF
)" \
  "$SETUP" \
  "$WINZIP" \
  "$APPIMAGE"

echo "Published: https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.7"
