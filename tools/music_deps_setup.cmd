@echo off
REM Create/update .venv-music with cricodecs for Music Library prepare/preview.
REM Place ffmpeg.exe and vgmstream-cli.exe in tools\bin\ (see tools\bin\README.md).
setlocal
set "ROOT=%~dp0.."
set "VENV=%ROOT%\.venv-music"
set "PY="
where py >nul 2>&1 && set "PY=py -3"
if not defined PY where python >nul 2>&1 && set "PY=python"
if not defined PY (
  echo FAIL: Python 3 not found on PATH
  exit /b 1
)

echo Using Python: %PY%
echo Venv: %VENV%
%PY% -m venv "%VENV%"
"%VENV%\Scripts\python.exe" -m pip install --upgrade pip
"%VENV%\Scripts\python.exe" -m pip install cricodecs==1.2.0
if errorlevel 1 "%VENV%\Scripts\python.exe" -m pip install cricodecs

set "PYTHONPATH=%ROOT%\tools;%PYTHONPATH%"
"%VENV%\Scripts\python.exe" -m music_asset_pkg.cli doctor
echo OK: music deps venv ready. Optional: copy ffmpeg.exe + vgmstream-cli.exe into tools\bin\
exit /b 0
