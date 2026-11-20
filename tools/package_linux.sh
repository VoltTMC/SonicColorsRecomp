#!/bin/bash
# Packs a Linux build into dist/SonicColorsRecomp-<version>-linux-x86_64.AppImage
# (no copyrighted game data). User data goes to $XDG_DATA_HOME/SonicColorsRecomp
# when the AppImage filesystem is read-only.
#
#   tools/package_linux.sh [-b build dir] [-c shader cache dir] [-v version]
#   tools/package_linux.sh -S                      # stage AppDir only
#   tools/package_linux.sh -A dist/AppDir -c DIR   # finish AppImage from staged dir
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=$root/build/linux-clang cache="" version="${SCR_VERSION:-${BKR_VERSION:-}}" stageOnly=0 appdir=""
while getopts "b:c:v:SA:" o; do
    case $o in b) build=$OPTARG ;; c) cache=$OPTARG ;; v) version=$OPTARG ;; S) stageOnly=1 ;; A) appdir=$OPTARG ;; *) exit 2 ;; esac
done
[ -n "$version" ] || version=$(git -C "$root" describe --tags --always --dirty 2>/dev/null || echo dev)

APP=SonicColorsRecomp

stage() {   # stage <AppDir>: everything except the shader cache
    local appdir=$1 out=$root/out
    local exe=$out/$APP bin=$1/usr/bin lic=$1/usr/share/licenses/$APP
    [ -x "$exe" ] || { echo "build first (cmake --build --preset linux-clang)"; exit 1; }
    rm -rf "$appdir"
    mkdir -p "$bin/shadercache" "$lic" "$appdir/usr/share/applications" "$appdir/usr/share/icons/hicolor/scalable/apps"
    cp "$exe" "$bin/"
    strip --strip-debug "$bin/$APP" || true
    cp -L "$out/libSDL3.so.0" "$out/libdxvk_d3d11.so.0" "$out/libdxvk_dxgi.so.0" "$bin/"
    # RecompLauncher reads /recomplauncher.json at the AppImage root (next to AppRun).
    cp "$root/recomplauncher.json" "$appdir/recomplauncher.json"
    printf '#!/bin/sh\nhere=$(dirname "$(readlink -f "$0")")\nexport LD_LIBRARY_PATH="$here/usr/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\nexec "$here/usr/bin/%s" "$@"\n' "$APP" > "$appdir/AppRun"
    chmod +x "$appdir/AppRun"
    cp "$root/data/linux/soniccolorsrecomp.desktop" "$appdir/"
    cp "$root/data/linux/soniccolorsrecomp.desktop" "$appdir/usr/share/applications/"
    mkdir -p "$appdir/usr/share/metainfo"
    cp "$root/data/linux/soniccolorsrecomp.metainfo.xml" "$appdir/usr/share/metainfo/"
    cp "$root/data/linux/soniccolorsrecomp.svg" "$appdir/soniccolorsrecomp.svg"
    cp "$root/data/linux/soniccolorsrecomp.svg" "$appdir/usr/share/icons/hicolor/scalable/apps/"
    # AppImage convention: .DirIcon next to AppRun
    cp "$root/data/linux/soniccolorsrecomp.svg" "$appdir/.DirIcon"

    # Music Library catalog next to the binary (AppImage usr/bin). manifest.toml is
    # written under the user data directory when the AppImage filesystem is read-only.
    mkdir -p "$bin/mods/music"
    if [ ! -f "$root/mods/music/catalog.json" ]; then
        echo "error: mods/music/catalog.json missing — cannot package Music Library" >&2
        exit 1
    fi
    cp -f "$root/mods/music/catalog.json" "$bin/mods/music/"
    cp -f "$root/mods/music/README.md" "$bin/mods/music/" 2>/dev/null || true

    # music_asset helper (parity with Windows ZIP). Host still needs Python +
    # cricodecs/vgmstream/ffmpeg for prepare/preview/export.
    mkdir -p "$bin/tools"
    if [ -x "$out/music_asset_launcher" ]; then
        cp -f "$out/music_asset_launcher" "$bin/tools/music_asset"
        chmod +x "$bin/tools/music_asset"
    elif [ -x "$out/music_asset" ]; then
        cp -f "$out/music_asset" "$bin/tools/music_asset"
        chmod +x "$bin/tools/music_asset"
    else
        echo "error: build music_asset (out/music_asset_launcher) before packaging" >&2
        exit 1
    fi
    if [ -d "$root/tools/music_asset_pkg" ]; then
        mkdir -p "$bin/tools/music_asset_pkg"
        cp -a "$root/tools/music_asset_pkg/." "$bin/tools/music_asset_pkg/"
        rm -f "$bin/tools/music_asset_pkg/test_music_asset.py"
        rm -rf "$bin/tools/music_asset_pkg/__pycache__"
    fi
    cp -f "$root/tools/music_deps_setup.sh" "$bin/tools/" 2>/dev/null || true
    mkdir -p "$bin/tools/bin" "$bin/data"
    cp -f "$root/tools/bin/README.md" "$bin/tools/bin/" 2>/dev/null || true
    [ -f "$root/data/music_deps_manifest.json" ] && cp -f "$root/data/music_deps_manifest.json" "$bin/data/"
    [ -f "$root/licenses/MUSIC_DEPS_RUNTIME.txt" ] && cp -f "$root/licenses/MUSIC_DEPS_RUNTIME.txt" "$lic/"

    cp "$root/licenses/BlackKnightRecomp-GPL-2.0.txt" "$lic/LICENSE"
    cp "$root/README.md" "$lic/README.md"
    [ -f "$build/_deps/sdl3-src/LICENSE.txt" ] && cp "$build/_deps/sdl3-src/LICENSE.txt" "$lic/SDL3.txt"
    [ -f "$build/_deps/imgui-src/LICENSE.txt" ] && cp "$build/_deps/imgui-src/LICENSE.txt" "$lic/imgui.txt"
    [ -f "$build/vkd3d_build-prefix/src/vkd3d_build/LICENSE" ] && cp "$build/vkd3d_build-prefix/src/vkd3d_build/LICENSE" "$lic/vkd3d-shader.txt"
    [ -f "$root/data/linux/licenses/dxvk.txt" ] && cp "$root/data/linux/licenses/dxvk.txt" "$lic/dxvk.txt"
    [ -f "$root/data/linux/licenses/nod.txt" ] && cp "$root/data/linux/licenses/nod.txt" "$lic/nod.txt"
    [ -f "$build/deps/libarchive/src/scr_libarchive/COPYING" ] && cp "$build/deps/libarchive/src/scr_libarchive/COPYING" "$lic/libarchive.txt"
    [ -f "$build/deps/zlib/src/scr_zlib/LICENSE" ] && cp "$build/deps/zlib/src/scr_zlib/LICENSE" "$lic/zlib.txt"
    [ -f "$build/deps/xz/src/scr_lzma/COPYING.0BSD" ] && cp "$build/deps/xz/src/scr_lzma/COPYING.0BSD" "$lic/liblzma.txt"
    [ -f "$build/licenses/nlohmann-json-LICENSE.MIT.txt" ] && cp "$build/licenses/nlohmann-json-LICENSE.MIT.txt" "$lic/nlohmann-json.txt"
}

if [ -z "$appdir" ]; then
    appdir=$root/dist/AppDir
    stage "$appdir"
    [ $stageOnly = 1 ] && { echo "staged $appdir"; exit 0; }
fi
bin=$appdir/usr/bin
chmod +x "$appdir/AppRun" "$bin/$APP"

# shader cache (optional — empty keys are fine for first-run compile)
if [ -n "$cache" ]; then
    cp "$cache"/*.cso "$cache"/*_keys.bin "$bin/shadercache/" 2>/dev/null || true
elif [ -d "$root/data/shadercache" ]; then
    cp "$root"/data/shadercache/*_keys.bin "$bin/shadercache/" 2>/dev/null || true
    if compgen -G "$bin/shadercache/*_keys.bin" > /dev/null; then
        "$bin/$APP" --precompile-shaders "$bin/shadercache" || true
    fi
fi

tools=$build/appimage-tools
mkdir -p "$tools"
fetch() {
    if [ ! -f "$3" ] || ! echo "$2  $3" | sha256sum -c --quiet - 2>/dev/null; then
        curl -fsSL -o "$3.part" "$1"
        echo "$2  $3.part" | sha256sum -c --quiet -
        mv "$3.part" "$3"
    fi
}
fetch https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage \
      ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0 "$tools/appimagetool"
fetch https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 \
      2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d "$tools/runtime-x86_64"
chmod +x "$tools/appimagetool"

mkdir -p "$root/dist"
target=$root/dist/$APP-$version-linux-x86_64.AppImage
rm -f "$target"
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$tools/appimagetool" --no-appstream --runtime-file "$tools/runtime-x86_64" "$appdir" "$target"
echo "packaged to $target"
