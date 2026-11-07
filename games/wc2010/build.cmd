@echo off
rem Configure (if needed) and build 2010 FIFA World Cup. Build tree: D:\CodexBuilds\wc2010.
rem   build.cmd configure   - configure only
rem   build.cmd             - incremental build with ninja (run detached for full builds:
rem                           ..\ps2-perf-tools\buildsup.py through build-detached.cmd)
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=D:\llvm22\bin;%~dp0..\recomp-python\Scripts;%PATH%"
set "SDK=%~dp0..\battlefield2\third_party\rexglue-sdk\win-amd64"
set "BUILD=D:\CodexBuilds\wc2010\win-amd64-relwithdebinfo"
if not exist "%BUILD%\build.ninja" (
  cmake -S "%~dp0project" --preset win-amd64-relwithdebinfo -B "%BUILD%" "-DCMAKE_PREFIX_PATH=%SDK%" -DREXGLUE_RECOMP_DEBUG_INFO=none
  if errorlevel 1 exit /b 1
)
if /i "%1"=="configure" exit /b 0
ninja -C "%BUILD%" %*
exit /b %errorlevel%
