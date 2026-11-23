# Collects a playable Windows build into dist/SonicColorsRecomp (no game data).
# Run on Windows after a successful cmake build:
#   powershell -ExecutionPolicy Bypass -File tools/package.ps1
# Testers start SonicColorsRecomp.exe; the launcher installs/locates their own disc data.
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot
$dist = Join-Path $root "dist\SonicColorsRecomp"
$exe = Join-Path $root "out\SonicColorsRecomp.exe"
if (-not (Test-Path $exe)) {
    throw "build first: cmake --preset windows-clang && cmake --build --preset windows-clang"
}

New-Item -ItemType Directory -Force $dist | Out-Null
New-Item -ItemType Directory -Force (Join-Path $dist "game") | Out-Null
Copy-Item $exe $dist -Force
Copy-Item (Join-Path $root "README.md") $dist -Force
Copy-Item (Join-Path $root "recomplauncher.json") $dist -Force

# Music Library catalog (manifest.toml / tracks are created by the launcher).
$catalog = Join-Path $root "mods\music\catalog.json"
if (-not (Test-Path $catalog)) {
    throw "mods/music/catalog.json missing — cannot package Music Library"
}
$musicDst = Join-Path $dist "mods\music"
New-Item -ItemType Directory -Force $musicDst | Out-Null
Copy-Item $catalog $musicDst -Force
$musicReadme = Join-Path $root "mods\music\README.md"
if (Test-Path $musicReadme) { Copy-Item $musicReadme $musicDst -Force }

# music_asset helper + optional tools/bin drop-ins (see tools\bin\README.md)
$toolsDst = Join-Path $dist "tools"
New-Item -ItemType Directory -Force $toolsDst | Out-Null
$assetExe = Join-Path $root "out\music_asset.exe"
if (Test-Path $assetExe) { Copy-Item $assetExe $toolsDst -Force }
foreach ($f in @("music_asset.cmd", "MUSIC_ASSET_WINDOWS.txt", "music_deps_setup.cmd")) {
    $src = Join-Path $root "tools\$f"
    if (Test-Path $src) { Copy-Item $src $toolsDst -Force }
}
# MinGW (or other) runtime DLLs must sit beside music_asset.exe, not only the main exe.
Get-ChildItem (Join-Path $root "out\*.dll") -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName $dist -Force
    if (Test-Path $assetExe) { Copy-Item $_.FullName $toolsDst -Force }
}
$pthread = Join-Path $dist "libwinpthread-1.dll"
if ((Test-Path $assetExe) -and (Test-Path $pthread)) {
    Copy-Item $pthread $toolsDst -Force
}
$pkgSrc = Join-Path $root "tools\music_asset_pkg"
if (Test-Path $pkgSrc) {
    $pkgDst = Join-Path $toolsDst "music_asset_pkg"
    New-Item -ItemType Directory -Force $pkgDst | Out-Null
    Copy-Item -Recurse -Force (Join-Path $pkgSrc "*") $pkgDst
    Remove-Item -Force (Join-Path $pkgDst "test_music_asset.py") -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force (Join-Path $pkgDst "__pycache__") -ErrorAction SilentlyContinue
}
$binDst = Join-Path $toolsDst "bin"
New-Item -ItemType Directory -Force $binDst | Out-Null
$binReadme = Join-Path $root "tools\bin\README.md"
if (Test-Path $binReadme) { Copy-Item $binReadme $binDst -Force }

# GPL-2.0 and third-party notices (build copies many into out/licenses)
$licDst = Join-Path $dist "licenses"
New-Item -ItemType Directory -Force $licDst | Out-Null
$gpl = Join-Path $root "licenses\BlackKnightRecomp-GPL-2.0.txt"
if (Test-Path $gpl) { Copy-Item $gpl (Join-Path $licDst "LICENSE.txt") -Force }
if (Test-Path (Join-Path $root "out\licenses")) {
    Copy-Item -Force (Join-Path $root "out\licenses\*") $licDst
}

@"
Sonic Colors Recomp — Windows test build (no game data included)

You must own Sonic Colors (Wii) Europe / PAL SNCP8P (USA/Japan discs are not supported yet).

1. Run SonicColorsRecomp.exe (or use RecompLauncher Easy Setup)
2. Install from your own disc image (ISO / RVZ / WIA / WBFS / GCZ)
3. Play

Config, saves, and logs are written next to the executable (portable.txt).
Useful: --help, --check-updates, --save-diagnostics, --list-gamepads, --verify-data-quick
See docs\windows-native-checklist.md

Do not redistribute copyrighted game files with this package.
"@ | Set-Content (Join-Path $dist "TESTING.txt")

@"
You don't need to put anything here. Start SonicColorsRecomp.exe and select your own
disc image (ISO / extracted SNCP8P) in the launcher. Game data is installed under this
folder (or a path you choose); the image is not needed afterwards.

Optional:
  textures\...     Dolphin-format HD texture packs
  mods\<name>\...  file-replacement mods (or use Mods > Get mods in the launcher)
"@ | Set-Content (Join-Path $dist "game\ABOUT_THIS_FOLDER.txt")

# A3z: ship the full validated portable DXBC cache (keys + .cso + manifest).
# Prefer data\shadercache, then out\shadercache. Do not recompile at package time
# when a complete cache is already present — players preload automatically at startup.
$dstCache = Join-Path $dist "shadercache"
New-Item -ItemType Directory -Force $dstCache | Out-Null
Get-ChildItem $dstCache -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force
$srcCache = $null
foreach ($cand in @(
        (Join-Path $root "data\shadercache"),
        (Join-Path $root "out\shadercache"))) {
    $vs = Join-Path $cand "vs_keys.bin"
    $cso = Get-ChildItem $cand -Filter *.cso -ErrorAction SilentlyContinue
    if ((Test-Path $vs) -and $cso -and $cso.Count -gt 0) { $srcCache = $cand; break }
}
if ($srcCache) {
    Copy-Item -Force (Join-Path $srcCache "*") $dstCache
    $n = (Get-ChildItem $dstCache -Filter *.cso).Count
    Write-Host "shader cache: packaged $n .cso from $srcCache"
    if ($n -lt 1) { throw "shader cache copy produced no .cso files" }
} else {
    Write-Warning "no full shader cache (data/shadercache or out/shadercache); first play may hitch while compiling"
}

Write-Host "packaged to $dist"
Write-Host "Zip that folder to share. Do not include copyrighted game/ contents."
