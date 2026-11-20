@echo off
REM Windows launcher for music_asset_pkg (fallback when music_asset.exe is absent).
setlocal
set "ROOT=%~dp0.."
set "TOOLS=%~dp0"
set "PYTHONPATH=%TOOLS%;%PYTHONPATH%"

if exist "%ROOT%\.venv-music\Scripts\python.exe" (
  set "PY=%ROOT%\.venv-music\Scripts\python.exe"
) else if exist "%~dp0python\python.exe" (
  set "PY=%~dp0python\python.exe"
) else (
  where py >nul 2>&1 && (set "PY=py -3") || (set "PY=python")
)

%PY% -m music_asset_pkg.cli %*
exit /b %ERRORLEVEL%
