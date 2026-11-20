@echo off
REM Create/update a cricodecs venv for Music Library prepare/preview.
REM Installed app: venv + drop-ins under %APPDATA%\SonicColorsRecomp (survive updates).
REM Portable ZIP: venv next to the install root; drop-ins in tools\bin.
setlocal EnableExtensions
set "TOOLS=%~dp0"
set "ROOT=%TOOLS%.."

set "USERDATA="
if exist "%ROOT%\installed.txt" (
  set "USERDATA=%APPDATA%\SonicColorsRecomp"
) else if defined SCR_USER_DIR (
  set "USERDATA=%SCR_USER_DIR%"
)

if defined USERDATA (
  set "VENV=%USERDATA%\.venv-music"
  set "BIN=%USERDATA%\tools\bin"
  echo Installed layout: user data = %USERDATA%
) else (
  set "VENV=%ROOT%\.venv-music"
  set "BIN=%TOOLS%bin"
  echo Portable layout: venv = %VENV%
)

set "PY="
where py >nul 2>&1 && set "PY=py -3"
if not defined PY where python >nul 2>&1 && set "PY=python"
if not defined PY (
  echo FAIL: Python 3 not found on PATH
  exit /b 1
)

echo == Music Library dependency setup (Windows) ==
echo Using Python: %PY%
echo Venv: %VENV%
echo Drop-in tools: %BIN%
if not exist "%BIN%" mkdir "%BIN%"
%PY% -m venv "%VENV%"
if errorlevel 1 exit /b 1
"%VENV%\Scripts\python.exe" -m pip install --upgrade pip
"%VENV%\Scripts\python.exe" -m pip install cricodecs==1.2.0
if errorlevel 1 "%VENV%\Scripts\python.exe" -m pip install cricodecs

set "PYTHONPATH=%TOOLS%;%PYTHONPATH%"
set "SCR_MUSIC_BIN=%BIN%"
echo.
echo == Verifying with music_asset doctor ==
"%VENV%\Scripts\python.exe" -m music_asset_pkg.cli doctor > "%TEMP%\scr_music_doctor.json" 2>&1
type "%TEMP%\scr_music_doctor.json"
echo.
"%VENV%\Scripts\python.exe" -c "import json;d=json.load(open(r'%TEMP%\scr_music_doctor.json',encoding='utf-8'));print('Ready:', ', '.join(d.get('ready') or []) or '(none)');print('Blocked:', ', '.join(d.get('blocked') or []) or '(none)');[print('Next:', s) for s in (d.get('setup') or {}).get('steps') or []]"
echo OK: cricodecs venv ready. Place ffmpeg.exe + vgmstream-cli.exe in:
echo   %BIN%
echo Re-check: tools\music_asset.exe doctor   or Music tab - Check tools
exit /b 0
