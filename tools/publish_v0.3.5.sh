#!/bin/bash
# Tag v0.3.5 and publish GitHub hotfix: AppImage + Windows Setup.exe (+ install-layout zip for updater).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

APPIMAGE=$root/dist/SonicColorsRecomp-v0.3.5-linux-x86_64.AppImage
SETUP=$root/dist/SonicColorsRecomp-Setup-v0.3.5.exe
WINZIP=$root/dist/SonicColorsRecomp-v0.3.5-windows-x86_64.zip
[[ -f $APPIMAGE && -f $SETUP && -f $WINZIP ]] || {
  echo "missing packages under dist/ (need AppImage, Setup.exe, and install-layout zip)" >&2
  exit 1
}

gh auth status
git push origin HEAD

author_name=$(git log -1 --format='%an')
author_email=$(git log -1 --format='%ae')
if git rev-parse -q --verify refs/tags/v0.3.5 >/dev/null; then
  echo "tag v0.3.5 already exists locally"
else
  git -c user.name="$author_name" -c user.email="$author_email" tag -a v0.3.5 -m "$(cat <<'EOF'
v0.3.5 — hotfix: Windows Music Library CreateProcess 87

EOF
)"
fi
git push origin v0.3.5

gh release create v0.3.5 \
  --title "v0.3.5 — Hotfix: Windows Music Library process launch" \
  --notes "$(cat <<'EOF'
## Summary
Hotfix over **v0.3.4**: Windows Music Library can launch helper tools again.

**You still need your own Europe / PAL (SNCP8P) disc.** No game data is included.

### Fixed
- **`CreateProcess failed (87)`** when `music_asset.exe` started Python (Unicode environment block without `CREATE_UNICODE_ENVIRONMENT`)
- Ambiguous **`PREPARE FAILED (rc=127)`** conflating “Python not found” with process-creation failure (now **127** vs **126**)
- GUI `Platform_RunProcess` redirects using invalid std handles (`STARTF_USESTDHANDLES` + `GetStdHandle`)

### Windows
- **New installs:** run `SonicColorsRecomp-Setup-v0.3.5.exe` (per-user, no admin)
- **In-app update zip** (`*-windows-x86_64.zip`): install-layout package for **Install & relaunch**
- After install/update, run `tools\music_deps_setup.cmd` if Music preview/prepare still need Python/cricodecs

### Assets
| File | Platform |
|------|----------|
| `SonicColorsRecomp-Setup-v0.3.5.exe` | **Windows (primary)** — per-user installer |
| `SonicColorsRecomp-v0.3.5-windows-x86_64.zip` | Windows update / advanced (install layout) |
| `SonicColorsRecomp-v0.3.5-linux-x86_64.AppImage` | Linux |

See `docs/WINDOWS_INSTALLER.md` and `tools/MUSIC_ASSET_WINDOWS.txt`.
EOF
)" \
  "$SETUP" \
  "$WINZIP" \
  "$APPIMAGE"

echo "Published: https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.5"
