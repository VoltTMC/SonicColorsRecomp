#!/usr/bin/env bash
# Stage a Windows *installed* layout (no portable.txt) and build an Inno Setup installer.
#
#   tools/package_windows_installer.sh [-o out-windows] [-v version]
#
# Produces:
#   dist/SonicColorsRecomp-install/          (staged tree)
#   dist/SonicColorsRecomp-Setup-v<ver>.exe  (when ISCC / Wine+Inno available)
#
# The optional portable ZIP remains tools/package_windows_cross.sh.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=$root/out-windows
version="${SCR_VERSION:-}"
while getopts "o:v:" o; do
    case $o in o) out=$OPTARG ;; v) version=$OPTARG ;; *) exit 2 ;; esac
done
[ -n "$version" ] || version=$(git -C "$root" describe --tags --always --dirty 2>/dev/null || echo dev)
version=${version#v}
# Inno VersionInfoVersion needs dotted numerics (drop -dirty / git sha suffixes).
version_numeric=$(echo "$version" | sed -E 's/[^0-9.].*$//;s/\.$//')
[ -n "$version_numeric" ] || version_numeric=0.0.0

APP=SonicColorsRecomp
exe=$out/$APP.exe
[ -f "$exe" ] || { echo "build first: cmake --preset windows-mingw && cmake --build --preset windows-mingw"; exit 1; }

stage=$root/dist/$APP-install
rm -rf "$stage"
mkdir -p "$stage/game" "$stage/licenses" "$stage/shadercache"

cp "$exe" "$stage/"
# Installed layout marker — Platform_UserDir uses %APPDATA%\SonicColorsRecomp.
printf 'Sonic Colors Recomp installed layout\nversion=%s\n' "$version" > "$stage/installed.txt"
# Explicitly do NOT create portable.txt.

cp "$root/README.md" "$stage/" 2>/dev/null || true
cp "$root/recomplauncher.json" "$stage/"
mkdir -p "$stage/data" "$stage/licenses"
[ -f "$root/data/music_deps_manifest.json" ] && cp -f "$root/data/music_deps_manifest.json" "$stage/data/"
[ -f "$root/licenses/MUSIC_DEPS_RUNTIME.txt" ] && cp -f "$root/licenses/MUSIC_DEPS_RUNTIME.txt" "$stage/licenses/"
if [ -f "$root/docs/WINDOWS_FEATURE_PARITY.md" ]; then
    mkdir -p "$stage/docs"
    cp "$root/docs/WINDOWS_FEATURE_PARITY.md" "$stage/docs/"
fi
if [ -f "$root/docs/WINDOWS_INSTALLER.md" ]; then
    mkdir -p "$stage/docs"
    cp "$root/docs/WINDOWS_INSTALLER.md" "$stage/docs/"
fi
[ -f "$root/licenses/BlackKnightRecomp-GPL-2.0.txt" ] && \
    cp "$root/licenses/BlackKnightRecomp-GPL-2.0.txt" "$stage/licenses/LICENSE.txt"
if [ -d "$out/licenses" ]; then
    cp -f "$out/licenses/"* "$stage/licenses/" 2>/dev/null || true
fi

mkdir -p "$stage/mods/music"
[ -f "$root/mods/music/catalog.json" ] || { echo "error: mods/music/catalog.json missing" >&2; exit 1; }
cp -f "$root/mods/music/catalog.json" "$stage/mods/music/"
cp -f "$root/mods/music/README.md" "$stage/mods/music/" 2>/dev/null || true

mkdir -p "$stage/tools"
[ -f "$out/music_asset.exe" ] && cp -f "$out/music_asset.exe" "$stage/tools/"
cp -f "$root/tools/music_asset.cmd" "$stage/tools/" 2>/dev/null || true
cp -f "$root/tools/MUSIC_ASSET_WINDOWS.txt" "$stage/tools/" 2>/dev/null || true
cp -f "$root/tools/music_deps_setup.cmd" "$stage/tools/" 2>/dev/null || true
mkdir -p "$stage/tools/bin"
cp -f "$root/tools/bin/README.md" "$stage/tools/bin/" 2>/dev/null || true
if [ -d "$root/tools/music_asset_pkg" ]; then
    mkdir -p "$stage/tools/music_asset_pkg"
    cp -a "$root/tools/music_asset_pkg/." "$stage/tools/music_asset_pkg/"
    rm -f "$stage/tools/music_asset_pkg/test_music_asset.py"
    rm -rf "$stage/tools/music_asset_pkg/__pycache__"
fi
if [ -f "$root/docs/windows-native-checklist.md" ]; then
    mkdir -p "$stage/docs"
    cp -f "$root/docs/windows-native-checklist.md" "$stage/docs/"
fi

# A3z: ship full portable DXBC cache (keys + .cso + manifest).
pick_cache() {
    if [ -f "$root/data/shadercache/vs_keys.bin" ] && compgen -G "$root/data/shadercache/*.cso" > /dev/null; then
        echo "$root/data/shadercache"; return
    fi
    if [ -f "$out/shadercache/vs_keys.bin" ] && compgen -G "$out/shadercache/*.cso" > /dev/null; then
        echo "$out/shadercache"; return
    fi
    echo ""
}
src_cache=$(pick_cache)
if [ -n "$src_cache" ]; then
    cp -f "$src_cache"/*.cso "$src_cache"/*_keys.bin "$src_cache"/manifest.json "$stage/shadercache/" 2>/dev/null || true
    n_cso=$(find "$stage/shadercache" -maxdepth 1 -name '*.cso' | wc -l)
    echo "shader cache: packaged $n_cso .cso from $src_cache → $stage/shadercache"
    if [ "$n_cso" -lt 1 ]; then
        echo "error: shader cache copy produced no .cso files" >&2
        exit 1
    fi
else
    echo "warning: no full shader cache (data/shadercache or out/shadercache); first play may hitch" >&2
fi

shopt -s nullglob
for dll in "$out"/*.dll; do
    cp -f "$dll" "$stage/"
done
shopt -u nullglob
if x86_64-w64-mingw32-objdump -p "$exe" 2>/dev/null | grep -qi 'libwinpthread'; then
    for cand in /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll "$out/libwinpthread-1.dll"; do
        if [ -f "$cand" ]; then cp -f "$cand" "$stage/"; break; fi
    done
fi
if [ -f "$stage/tools/music_asset.exe" ]; then
    for cand in "$stage/libwinpthread-1.dll" /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll "$out/libwinpthread-1.dll"; do
        if [ -f "$cand" ]; then
            cp -f "$cand" "$stage/tools/libwinpthread-1.dll"
            break
        fi
    done
fi

cat > "$stage/TESTING.txt" <<EOF
Sonic Colors Recomp — Windows installed build (no game data included)

Installed via Setup into %LOCALAPPDATA%\\Programs\\Sonic Colors Recomp.
User data (config, saves, mods, Music Library) lives under:
  %APPDATA%\\SonicColorsRecomp

Music drop-in tools (ffmpeg / vgmstream): %APPDATA%\\SonicColorsRecomp\\tools\\bin
Setup: tools\\music_deps_setup.cmd then Check tools in the Music tab.

Do not redistribute copyrighted game files with this package.
EOF

cat > "$stage/game/ABOUT_THIS_FOLDER.txt" <<'EOF'
Game data is installed under your user-data folder (%APPDATA%\SonicColorsRecomp\game)
when you use the launcher Install flow — not into Program Files / the install directory.
EOF

# Guard: portable marker must never ship in the installer stage.
[ ! -f "$stage/portable.txt" ] || { echo "error: portable.txt must not be in install stage" >&2; exit 1; }
[ -f "$stage/installed.txt" ] || { echo "error: installed.txt missing" >&2; exit 1; }

echo "staged install tree at $stage"

# --- Inno Setup compile ---
iss=$root/packaging/windows/SonicColorsRecomp.iss
vendor=$root/tools/vendor/innosetup
iscc=""
if command -v ISCC.exe >/dev/null 2>&1; then
    iscc=$(command -v ISCC.exe)
elif command -v iscc >/dev/null 2>&1; then
    iscc=$(command -v iscc)
elif [ -x "$vendor/Installed/ISCC.exe" ]; then
    iscc=$vendor/Installed/ISCC.exe
elif [ -f "$vendor/Installed/ISCC.exe" ]; then
    iscc=$vendor/Installed/ISCC.exe
fi

ensure_inno() {
    if [ -n "$iscc" ] && { [ -x "$iscc" ] || [ -f "$iscc" ]; }; then return 0; fi
    command -v wine >/dev/null || { echo "WARN: wine not found; cannot bootstrap Inno Setup" >&2; return 1; }
    mkdir -p "$vendor"
    local setup=$vendor/innosetup-7.1.1-x64.exe
    if [ ! -f "$setup" ]; then
        echo "Downloading Inno Setup 7.1.1…"
        curl -fsSL -o "$setup" \
            "https://github.com/jrsoftware/issrc/releases/download/is-7_1_1/innosetup-7.1.1-x64.exe"
    fi
    echo "Installing Inno Setup under Wine (silent)…"
    # /DIR= and /VERYSILENT are Inno's own installer flags
    local windir
    windir=$(winepath -w "$vendor/Installed" 2>/dev/null || echo "Z:$(echo "$vendor/Installed" | sed 's|/|\\|g')")
    WINEDEBUG=-all wine "$setup" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART "/DIR=$windir" || true
    if [ -f "$vendor/Installed/ISCC.exe" ]; then
        iscc=$vendor/Installed/ISCC.exe
        return 0
    fi
    # Fallback common Wine Program Files path
    for cand in \
        "$HOME/.wine/drive_c/Program Files/Inno Setup 7/ISCC.exe" \
        "$HOME/.wine/drive_c/Program Files (x86)/Inno Setup 7/ISCC.exe" \
        "$HOME/.wine/drive_c/Program Files/Inno Setup 6/ISCC.exe"
    do
        if [ -f "$cand" ]; then iscc=$cand; return 0; fi
    done
    return 1
}

if ! ensure_inno; then
    echo "WARN: ISCC unavailable — staged tree only. Install Inno Setup and re-run to produce Setup.exe." >&2
    echo "Portable ZIP (optional): tools/package_windows_cross.sh -v $version"
    exit 0
fi

# Map a host path to a Wine Z: path. Prefer winepath when it returns a real drive path.
wine_win_path() {
    local p=$1 out
    out=$(WINEDEBUG=-all winepath -w "$p" 2>/dev/null || true)
    if printf '%s' "$out" | grep -Eq '^[A-Za-z]:\\'; then
        printf '%s\n' "$out"
    else
        # Avoid echo -e eating \v in paths like /home/volt/...
        python3 -c 'import sys; print("Z:" + sys.argv[1].replace("/", "\\\\"))' "$p"
    fi
}
stage_win=$(wine_win_path "$stage")
out_win=$(wine_win_path "$root/dist")
iss_win=$(wine_win_path "$iss")

# Write a tiny wrapper .iss that #defines paths (avoids Wine eating /D flags).
wrap=$root/dist/_scr_setup_wrap.iss
cat >"$wrap" <<EOF
#define MyAppVersion "$version_numeric"
#define StageDir "$stage_win"
#define OutputDir "$out_win"
#define OutputBase "SonicColorsRecomp-Setup"
#include "$iss_win"
EOF
wrap_win=$(wine_win_path "$wrap")

echo "Compiling installer with $iscc …"
set +e
WINEDEBUG=-all wine "$iscc" "$wrap_win"
iscc_rc=$?
set -e
rm -f "$wrap"
[ "$iscc_rc" -eq 0 ] || { echo "FAIL: ISCC exited $iscc_rc" >&2; exit 1; }

setup=$(ls -1t "$root"/dist/SonicColorsRecomp-Setup-v*.exe 2>/dev/null | head -1 || true)
[ -n "$setup" ] && [ -f "$setup" ] || { echo "FAIL: Setup.exe not produced" >&2; exit 1; }
echo "installer: $setup"
