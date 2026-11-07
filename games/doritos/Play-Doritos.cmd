@echo off
rem Doritos Crash Course (PC port) - opens the game.
setlocal
set "BIN_DIR=D:\CodexBuilds\doritos\build\win-amd64-release"
set "DATA=D:\CodexBuilds\doritos"
if not exist "%BIN_DIR%\doritoscrashcourse.exe" (
  echo Executavel nao encontrado em %BIN_DIR%. Rode build-doritos.cmd primeiro.
  pause
  exit /b 1
)
if not exist "%DATA%\userdata" mkdir "%DATA%\userdata"
cd /d "%BIN_DIR%"
start "" doritoscrashcourse.exe "--game_data_root=%DATA%\game_files" "--user_data_root=%DATA%\userdata" %*
