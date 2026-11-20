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

# Optional precompiled shader cache (skips first-run HLSL compile when keys exist).
$dstCache = Join-Path $dist "shadercache"
New-Item -ItemType Directory -Force $dstCache | Out-Null
Get-ChildItem $dstCache -Filter *.cso -ErrorAction SilentlyContinue | Remove-Item
$haveKeys = $false
foreach ($k in "vs_keys.bin", "ps_keys.bin") {
    $bytes = [byte[]]@()
    foreach ($src in @(
            (Join-Path $root "data\shadercache\$k"),
            (Join-Path $root "out\shadercache\$k"),
            (Join-Path $dstCache $k))) {
        if (Test-Path $src) { $bytes += [IO.File]::ReadAllBytes($src); $haveKeys = $true }
    }
    if ($bytes.Length -gt 0) {
        [IO.File]::WriteAllBytes((Join-Path $dstCache $k), $bytes)
    }
}
if ($haveKeys) {
    & $exe --precompile-shaders $dstCache
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "shader precompilation failed (package still usable; first run may compile shaders)"
    }
} else {
    Write-Host "no shader keys found — skipping precompile (first play may hitch while compiling)"
}

Write-Host "packaged to $dist"
Write-Host "Zip that folder to share. Do not include copyrighted game/ contents."
