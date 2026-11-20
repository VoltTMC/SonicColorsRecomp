#!/bin/bash
# Tag v0.3.6 and publish GitHub hotfix: AppImage + Windows Setup.exe (+ install-layout zip for updater).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

APPIMAGE=$root/dist/SonicColorsRecomp-v0.3.6-linux-x86_64.AppImage
SETUP=$root/dist/SonicColorsRecomp-Setup-v0.3.6.exe
WINZIP=$root/dist/SonicColorsRecomp-v0.3.6-windows-x86_64.zip
[[ -f $APPIMAGE && -f $SETUP && -f $WINZIP ]] || {
  echo "missing packages under dist/ (need AppImage, Setup.exe, and install-layout zip)" >&2
  exit 1
}

gh auth status
git push origin HEAD

author_name=$(git log -1 --format='%an')
author_email=$(git log -1 --format='%ae')
if git rev-parse -q --verify refs/tags/v0.3.6 >/dev/null; then
  echo "tag v0.3.6 already exists locally"
else
  git -c user.name="$author_name" -c user.email="$author_email" tag -a v0.3.6 -m "$(cat <<'EOF'
v0.3.6 — hotfix: Windows Music Install Music tools

EOF
)"
fi
git push origin v0.3.6

gh release create v0.3.6 \
  --title "v0.3.6 — Hotfix: Install Music tools from the launcher" \
  --notes "$(cat <<'EOF'
## Summary
Hotfix over **v0.3.5**: Windows can install Music Library host tools from the launcher with little manual setup.

**You still need your own Europe / PAL (SNCP8P) disc.** No game data is included.

### Windows — Install Music tools
On the Music tab, use **Install Music tools** (also auto-starts when tools are missing). The launcher downloads pinned:
- Python embed 3.12.8
- cricodecs 1.2.0
- vgmstream r2168
- LGPL FFmpeg (BtbN shared build)

Into `%LOCALAPPDATA%\SonicColorsRecomp\dependencies` (no admin, no PATH edits). User drop-ins in `%APPDATA%\SonicColorsRecomp\tools\bin` still override managed copies.

Also includes from **v0.3.5**: CreateProcess 87 / prepare rc=127 fix and vgmstream discovery (`SCR_MUSIC_BIN` via Win32 env).

### Assets
| File | Platform |
|------|----------|
| `SonicColorsRecomp-Setup-v0.3.6.exe` | **Windows (primary)** — per-user installer |
| `SonicColorsRecomp-v0.3.6-windows-x86_64.zip` | Windows update / advanced (install layout) |
| `SonicColorsRecomp-v0.3.6-linux-x86_64.AppImage` | Linux |

See `docs/WINDOWS_INSTALLER.md`, `tools/MUSIC_ASSET_WINDOWS.txt`, and `licenses/MUSIC_DEPS_RUNTIME.txt`.
EOF
)" \
  "$SETUP" \
  "$WINZIP" \
  "$APPIMAGE"

echo "Published: https://github.com/VoltTMC/SonicColorsRecomp/releases/tag/v0.3.6"
