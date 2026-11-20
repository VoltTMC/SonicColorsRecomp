@echo off
REM Create/update .venv-music with cricodecs for Music Library prepare/preview.
REM Place ffmpeg.exe and vgmstream-cli.exe in tools\bin\ (see tools\bin\README.md).
setlocal EnableExtensions
set "ROOT=%~dp0.."
set "VENV=%ROOT%\.venv-music"
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
echo Drop-in tools: %ROOT%\tools\bin\  (see tools\bin\README.md)
%PY% -m venv "%VENV%"
if errorlevel 1 exit /b 1
"%VENV%\Scripts\python.exe" -m pip install --upgrade pip
"%VENV%\Scripts\python.exe" -m pip install cricodecs==1.2.0
if errorlevel 1 "%VENV%\Scripts\python.exe" -m pip install cricodecs

set "PYTHONPATH=%ROOT%\tools;%PYTHONPATH%"
echo.
echo == Verifying with music_asset doctor ==
"%VENV%\Scripts\python.exe" -m music_asset_pkg.cli doctor > "%TEMP%\scr_music_doctor.json" 2>&1
type "%TEMP%\scr_music_doctor.json"
echo.
"%VENV%\Scripts\python.exe" -c "import json;d=json.load(open(r'%TEMP%\scr_music_doctor.json',encoding='utf-8'));print('Ready:', ', '.join(d.get('ready') or []) or '(none)');print('Blocked:', ', '.join(d.get('blocked') or []) or '(none)');[print('Next:', s) for s in (d.get('setup') or {}).get('steps') or []]"
echo OK: cricodecs venv ready. Place ffmpeg.exe + vgmstream-cli.exe in tools\bin\ if still blocked.
echo Re-check: tools\music_asset.exe doctor   or Music tab - Check tools
exit /b 0
