#!/bin/bash
# Tag v0.3.4 and publish GitHub hotfix: AppImage + Windows Setup.exe (+ install-layout zip for updater).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

APPIMAGE=$root/dist/SonicColorsRecomp-v0.3.4-linux-x86_64.AppImage
SETUP=$root/dist/SonicColorsRecomp-Setup-v0.3.4.exe
WINZIP=$root/dist/SonicColorsRecomp-v0.3.4-windows-x86_64.zip
[[ -f $APPIMAGE && -f $SETUP && -f $WINZIP ]] || {
  echo "missing packages under dist/ (need AppImage, Setup.exe, and install-layout zip)" >&2
  exit 1
}

gh auth status
git push origin HEAD

author_name=$(git log -1 --format='%an')
author_email=$(git log -1 --format='%ae')
if git rev-parse -q --verify refs/tags/v0.3.4 >/dev/null; then
  echo "tag v0.3.4 already exists locally"
else
  git -c user.name="$author_name" -c user.email="$author_email" tag -a v0.3.4 -m "$(cat <<'EOF'
v0.3.4 — hotfix: Windows installer as primary distribution

EOF
)"
fi
git push origin v0.3.4

gh release create v0.3.4 \
  --title "v0.3.4 — Hotfix: Windows installer" \
  --notes "$(cat <<'EOF'
## Summary
Hotfix over **v0.3.3**: the **primary Windows download is now the per-user installer** (`Setup.exe`), not the portable ZIP.

**You still need your own Europe / PAL (SNCP8P) disc.** No game data is included.

### Windows
- **New installs:** run `SonicColorsRecomp-Setup-v0.3.4.exe`  
  - Installs to `%LOCALAPPDATA%\Programs\Sonic Colors Recomp` (no admin required)  
  - User data → `%APPDATA%\SonicColorsRecomp`  
  - Optional import from an old portable folder (copy-only)  
  - Start Menu shortcut + Apps & Features entry + uninstaller  
- **In-app update zip** (`*-windows-x86_64.zip`): install-layout package for **Install & relaunch** (no `portable.txt`). Prefer Setup for first-time installs.
- Music drop-ins survive updates under `%APPDATA%\SonicColorsRecomp\tools\bin`.

### Also includes (from v0.3.3)
Music Library doctor/setup, `tools/bin` discovery, `libwinpthread` beside `music_asset.exe`.

### Assets
| File | Platform |
|------|----------|
| `SonicColorsRecomp-Setup-v0.3.4.exe` | **Windows (primary)** — per-user installer |
| `SonicColorsRecomp-v0.3.4-windows-x86_64.zip` | Windows update / advanced (install layout, not portable) |
| `SonicColorsRecomp-v0.3.4-linux-x86_64.AppImage` | Linux |

See `docs/WINDOWS_INSTALLER.md`.
EOF
)" \
  "$SETUP" \
  "$WINZIP" \
  "$APPIMAGE"

echo "Published: https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.4"
