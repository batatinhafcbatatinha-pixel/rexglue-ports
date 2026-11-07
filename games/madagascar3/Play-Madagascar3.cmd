@echo off
rem DreamWorks Madagascar 3: The Video Game — Launcher PC Port
setlocal
cd /d "%~dp0"
set "BIN_DIR=%~dp0project\out\build\win-amd64-release"
if exist "%~dp0build-location.txt" set /p BIN_DIR=<"%~dp0build-location.txt"
rem Setup.cmd (tools\build_local.py) builds into .local\build
if exist "%~dp0.local\build\madagascar3.exe" set "BIN_DIR=%~dp0.local\build"
if not exist "%BIN_DIR%\madagascar3.exe" (
  echo The game is not built yet: run Setup.cmd first.
  echo O jogo ainda nao foi montado: rode o Setup.cmd primeiro.
  pause
  exit /b 1
)
cd /d "%BIN_DIR%"
start "" madagascar3.exe "--game_data_root=%~dp0game_files" %*
