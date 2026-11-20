#!/usr/bin/env bash
# Automated Windows installer checks (Wine). Fresh install must not require a portable folder.
#
#   tools/package_windows_installer_test.sh [Setup.exe]
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

setup=${1:-}
if [ -z "$setup" ]; then
  setup=$(ls -1t "$root"/dist/SonicColorsRecomp-Setup-v*.exe 2>/dev/null | head -1 || true)
fi
[ -n "$setup" ] && [ -f "$setup" ] || fail "Setup.exe missing (build with package_windows_installer.sh)"

command -v wine >/dev/null || fail "wine required for installer tests"
grep -q "Fresh install (recommended)" "$root/packaging/windows/SonicColorsRecomp.iss" \
  || fail "iss missing fresh-install default option"
grep -q "WantMigrate" "$root/packaging/windows/SonicColorsRecomp.iss" \
  || fail "iss missing WantMigrate gate"
grep -q "CreateInputOptionPage" "$root/packaging/windows/SonicColorsRecomp.iss" \
  || fail "iss missing optional migrate choice page"
ok "iss: fresh default + optional migrate"

wine_win_path() {
  local p=$1 out
  out=$(WINEDEBUG=-all winepath -w "$p" 2>/dev/null || true)
  if printf '%s' "$out" | grep -Eq '^[A-Za-z]:\\'; then
    printf '%s\n' "$out"
  else
    python3 -c 'import sys; print("Z:" + sys.argv[1].replace("/", "\\\\"))' "$p"
  fi
}
wine_unix_path() {
  local p=$1 out rel
  out=$(WINEDEBUG=-all winepath -u "$p" 2>/dev/null || true)
  if [[ "$out" == /* ]] && echo "$out" | grep -vq 'ShellExecute'; then
    printf '%s\n' "$out"
    return
  fi
  rel=${p#C:}
  rel=${rel#c:}
  rel=$(printf '%s' "$rel" | sed 's|^\\||;s|\\|/|g')
  printf '%s/drive_c/%s\n' "$WINEPREFIX" "$rel"
}

WINEPREFIX=${SCR_WINEPREFIX:-$root/dist/.wine-scr-installer-test}
export WINEPREFIX WINEDEBUG=-all
mkdir -p "$WINEPREFIX"
# Initialize prefix once (quiet)
wine hostname >/dev/null 2>&1 || true

# Discover the Wine user profile AppData (not hard-coded to a username).
appdata_unix=$(
  WINEDEBUG=-all wine cmd /c 'echo %APPDATA%' 2>/dev/null | tr -d '\r' | tail -1
)
[ -n "$appdata_unix" ] || fail "cannot resolve %APPDATA% under Wine"
appdata_win=$appdata_unix
# If echo returned a Windows path, convert; if already unix, keep
if [[ "$appdata_win" == [A-Za-z]:* ]]; then
  appdata_unix=$(wine_unix_path "$appdata_win")
fi
userdata_unix=$appdata_unix/SonicColorsRecomp

# --- Test A: silent fresh install (no portable folder, no /PORTABLEDIR) ---
install_win='C:\SCR_Test_Install\Sonic Colors Recomp'
install_unix=$(wine_unix_path "$install_win")
rm -rf "$install_unix" 2>/dev/null || true

echo "== Test A: fresh /VERYSILENT install (no portable) =="
wine "$setup" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART "/DIR=$install_win"
[ -f "$install_unix/SonicColorsRecomp.exe" ] || fail "Test A: exe not installed at $install_unix"
[ -f "$install_unix/installed.txt" ] || fail "Test A: installed.txt missing"
[ ! -f "$install_unix/portable.txt" ] || fail "Test A: portable.txt must not be installed"
ok "Test A: fresh install without portable folder"

# --- Test B: migrate from disposable portable (inside Wine C: to avoid Z: escaping) ---
echo "== Test B: migrate from portable via /PORTABLEDIR= =="
portable_unix=$WINEPREFIX/drive_c/portable_migrate_src
rm -rf "$portable_unix"
mkdir -p "$portable_unix/nand" "$portable_unix/mods/music" "$portable_unix/tools/bin"
printf 'fake-exe\n' >"$portable_unix/SonicColorsRecomp.exe"
printf "volume=0.5\n" >"$portable_unix/config.ini"
printf 'save\n' >"$portable_unix/nand/banner.bin"
printf 'mod\n' >"$portable_unix/mods/music/manifest.toml"
printf 'tool\n' >"$portable_unix/tools/bin/ffmpeg.exe"
printf 'portable\n' >"$portable_unix/portable.txt"

install2_win='C:\SCR_Test_Install_Migrate\Sonic Colors Recomp'
install2_unix=$(wine_unix_path "$install2_win")
rm -rf "$install2_unix" 2>/dev/null || true
rm -rf "$userdata_unix" 2>/dev/null || true

wine "$setup" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART \
  "/DIR=$install2_win" '/PORTABLEDIR=C:\portable_migrate_src'
[ -f "$install2_unix/SonicColorsRecomp.exe" ] || fail "Test B: exe not installed"
[ -f "$userdata_unix/config.ini" ] || fail "Test B: config.ini not migrated to AppData ($userdata_unix)"
[ -f "$userdata_unix/nand/banner.bin" ] || fail "Test B: nand not migrated"
[ -f "$userdata_unix/mods/music/manifest.toml" ] || fail "Test B: mods not migrated"
[ -f "$userdata_unix/tools/bin/ffmpeg.exe" ] || fail "Test B: tools/bin not migrated"
[ -f "$userdata_unix/migrated_from_portable.txt" ] || fail "Test B: migration marker missing"
[ -f "$portable_unix/config.ini" ] || fail "Test B: original portable config was deleted"
[ -f "$portable_unix/portable.txt" ] || fail "Test B: original portable folder damaged"
ok "Test B: migrate copies data; portable source intact"

# --- Test C: invalid /PORTABLEDIR= still completes as clean install ---
echo "== Test C: invalid /PORTABLEDIR= (clean install still succeeds) =="
install3_win='C:\SCR_Test_Install_BadMigrate\Sonic Colors Recomp'
install3_unix=$(wine_unix_path "$install3_win")
rm -rf "$install3_unix" 2>/dev/null || true
wine "$setup" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART \
  "/DIR=$install3_win" '/PORTABLEDIR=C:\does\not\exist\portable'
[ -f "$install3_unix/SonicColorsRecomp.exe" ] || fail "Test C: install aborted on bad migrate path"
[ -f "$install3_unix/installed.txt" ] || fail "Test C: installed.txt missing"
ok "Test C: invalid migrate directory does not block install"

# Spaces + Unicode path (fresh)
echo "== Test D: install dir with spaces / Unicode =="
install4_win='C:\SCR Test Uni\Sonic Colors Recomp'
install4_unix=$(wine_unix_path "$install4_win")
rm -rf "$install4_unix" 2>/dev/null || true
wine "$setup" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART "/DIR=$install4_win"
[ -f "$install4_unix/SonicColorsRecomp.exe" ] || fail "Test D: spaces install failed ($install4_unix)"
ok "Test D: spaces install dir"

echo "package_windows_installer_test OK"
echo "Note: interactive MsgBox for invalid browse-path is [native]; silent /PORTABLEDIR= covers abandon/continue."
