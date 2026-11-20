#!/bin/bash
# Package a MinGW cross-built Windows binary into a redistributable ZIP from Linux.
#
#   tools/package_windows_cross.sh [-o out-windows] [-v version]
#
# Produces: dist/SonicColorsRecomp-<version>-windows-x86_64.zip
# Does not include copyrighted game data. Optional shader keys are copied when present.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=$root/out-windows
version="${SCR_VERSION:-}"
while getopts "o:v:" o; do
    case $o in o) out=$OPTARG ;; v) version=$OPTARG ;; *) exit 2 ;; esac
done
[ -n "$version" ] || version=$(git -C "$root" describe --tags --always --dirty 2>/dev/null || echo dev)

APP=SonicColorsRecomp
exe=$out/$APP.exe
[ -f "$exe" ] || { echo "build first: cmake --preset windows-mingw && cmake --build --preset windows-mingw"; exit 1; }

stage=$root/dist/$APP
rm -rf "$stage"
mkdir -p "$stage/game" "$stage/licenses" "$stage/shadercache"

cp "$exe" "$stage/"
# Keep config/saves next to the exe for the redistributable ZIP (see Platform_UserDir).
: > "$stage/portable.txt"
cp "$root/README.md" "$stage/" 2>/dev/null || true
cp "$root/recomplauncher.json" "$stage/"
if [ -f "$root/docs/WINDOWS_FEATURE_PARITY.md" ]; then
    mkdir -p "$stage/docs"
    cp "$root/docs/WINDOWS_FEATURE_PARITY.md" "$stage/docs/"
fi
[ -f "$root/licenses/BlackKnightRecomp-GPL-2.0.txt" ] && \
    cp "$root/licenses/BlackKnightRecomp-GPL-2.0.txt" "$stage/licenses/LICENSE.txt"
if [ -d "$out/licenses" ]; then
    cp -f "$out/licenses/"* "$stage/licenses/" 2>/dev/null || true
fi

# Music Library catalog (required at runtime; manifest.toml/tracks are user-generated).
mkdir -p "$stage/mods/music"
if [ ! -f "$root/mods/music/catalog.json" ]; then
    echo "error: mods/music/catalog.json missing — cannot package Music Library" >&2
    exit 1
fi
cp -f "$root/mods/music/catalog.json" "$stage/mods/music/"
cp -f "$root/mods/music/README.md" "$stage/mods/music/" 2>/dev/null || true

# music_asset helper (Python package + Windows launcher). Requires host Python/cricodecs/vgmstream.
mkdir -p "$stage/tools"
if [ -f "$out/music_asset.exe" ]; then
    cp -f "$out/music_asset.exe" "$stage/tools/"
fi
cp -f "$root/tools/music_asset.cmd" "$stage/tools/" 2>/dev/null || true
cp -f "$root/tools/MUSIC_ASSET_WINDOWS.txt" "$stage/tools/" 2>/dev/null || true
cp -f "$root/tools/music_deps_setup.cmd" "$stage/tools/" 2>/dev/null || true
mkdir -p "$stage/tools/bin"
cp -f "$root/tools/bin/README.md" "$stage/tools/bin/" 2>/dev/null || true
if [ -d "$root/tools/music_asset_pkg" ]; then
    mkdir -p "$stage/tools/music_asset_pkg"
    cp -a "$root/tools/music_asset_pkg/." "$stage/tools/music_asset_pkg/"
    # Do not ship unit tests or bytecode in the redistributable.
    rm -f "$stage/tools/music_asset_pkg/test_music_asset.py"
    rm -rf "$stage/tools/music_asset_pkg/__pycache__"
fi
if [ -f "$root/docs/windows-native-checklist.md" ]; then
    mkdir -p "$stage/docs"
    cp -f "$root/docs/windows-native-checklist.md" "$stage/docs/"
fi

# Shader keys (precompile on Windows; Wine may not drive D3D11 shader compile reliably)
for k in vs_keys.bin ps_keys.bin; do
    for src in "$root/data/shadercache/$k" "$out/shadercache/$k"; do
        [ -f "$src" ] && cp -f "$src" "$stage/shadercache/" && break
    done
done

# Runtime DLLs next to the exe (nod.dll required; MinGW winpthread / MSVC CRT for MSVC nod).
shopt -s nullglob
for dll in "$out"/*.dll; do
    cp -f "$dll" "$stage/"
done
shopt -u nullglob
# Fallback: ship winpthread if the PE still imports it
if x86_64-w64-mingw32-objdump -p "$exe" 2>/dev/null | grep -qi 'libwinpthread'; then
    for cand in /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll "$out/libwinpthread-1.dll"; do
        if [ -f "$cand" ]; then cp -f "$cand" "$stage/"; break; fi
    done
fi
# music_asset.exe is under tools/; Windows loads DLLs from that directory first — copy
# MinGW runtimes next to it as well (parent-folder DLL is not searched automatically).
if [ -f "$stage/tools/music_asset.exe" ]; then
    for cand in "$stage/libwinpthread-1.dll" /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll "$out/libwinpthread-1.dll"; do
        if [ -f "$cand" ]; then
            cp -f "$cand" "$stage/tools/libwinpthread-1.dll"
            break
        fi
    done
    [ -f "$stage/tools/libwinpthread-1.dll" ] \
        || echo "WARN: libwinpthread-1.dll not copied next to tools/music_asset.exe" >&2
fi
# Prebuilt nod.dll is MSVC-built and needs VCRUNTIME140.dll (Wine does not ship it by default).
if [ -f "$stage/nod.dll" ] || [ -f "$out/nod.dll" ]; then
    if ! ls "$stage"/[Vv][Cc][Rr][Uu][Nn][Tt][Ii][Mm][Ee]140.dll >/dev/null 2>&1; then
        for cand in \
            "$out/VCRUNTIME140.dll" "$out/vcruntime140.dll" \
            /usr/lib/wine/x86_64-windows/vcruntime140.dll \
            /usr/share/steam/compatibilitytools.d/proton-cachyos-native/files/lib/wine/x86_64-windows/vcruntime140.dll
        do
            if [ -f "$cand" ]; then
                cp -f "$cand" "$stage/VCRUNTIME140.dll"
                break
            fi
        done
    fi
fi

# Report linked DLLs for the package notes (host objdump)
deps_txt=$stage/RUNTIME_DEPS.txt
{
    echo "PE imports (objdump -p). System DLLs are expected on Windows; do not ship them."
    echo
    x86_64-w64-mingw32-objdump -p "$exe" 2>/dev/null | awk '/DLL Name:/{print $3}' | sort -u
} > "$deps_txt"

cat > "$stage/TESTING.txt" <<'EOF'
Sonic Colors Recomp — Windows build (no game data included)

You must own Sonic Colors (Wii) Europe / PAL SNCP8P (USA/Japan discs are not supported yet).

1. Run SonicColorsRecomp.exe (or use RecompLauncher Easy Setup)
2. Install from your own disc image (ISO / RVZ / WIA / WBFS / GCZ)
3. Play

This ZIP includes portable.txt, so config, saves, and logs are written next to the
executable. For an installed layout without portable.txt, user data goes under
%APPDATA%\\SonicColorsRecomp (or set SCR_USER_DIR / pass --portable).

Music Library: mods\\music\\catalog.json is included (114 SNCP8P tracks). Assign
WAV replacements in the Music tab; prepare needs Python + cricodecs + vgmstream
+ ffmpeg (see tools\\MUSIC_ASSET_WINDOWS.txt).

Useful:
  SonicColorsRecomp.exe --help
  SonicColorsRecomp.exe --check-updates
  SonicColorsRecomp.exe --save-diagnostics
  SonicColorsRecomp.exe --list-gamepads
  SonicColorsRecomp.exe --verify-data-quick
  docs\\windows-native-checklist.md

Do not redistribute copyrighted game files with this package.
EOF

cat > "$stage/game/ABOUT_THIS_FOLDER.txt" <<'EOF'
You don't need to put anything here. Start SonicColorsRecomp.exe and select your own
disc image (ISO / extracted SNCP8P) in the launcher. Game data is installed under this
folder (or a path you choose); the image is not needed afterwards.

Optional:
  textures\...     Dolphin-format HD texture packs
  mods\<name>\...  file-replacement mods (or use Mods > Get mods in the launcher)
EOF

mkdir -p "$root/dist"
zipfile=$root/dist/$APP-$version-windows-x86_64.zip
rm -f "$zipfile"
( cd "$root/dist" && zip -r -q "$(basename "$zipfile")" "$APP" )
echo "packaged to $zipfile"
echo "staged at $stage"
