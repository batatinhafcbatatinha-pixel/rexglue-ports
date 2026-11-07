@echo off
rem Usage: build-doritos.cmd [codegen] [supervised]
rem Same toolchain as the other ReXGlue ports: MSVC environment + LLVM clang++ (GNU driver).
setlocal
set "ROOT=%~dp0"
set "SDK=%ROOT%..\battlefield2\third_party\rexglue-sdk\win-amd64"
if defined REXGLUE_SDK set "SDK=%REXGLUE_SDK%"
if not defined DORITOS_BUILD_ROOT set "DORITOS_BUILD_ROOT=D:/CodexBuilds/doritos/build"
if not defined LLVM_PATH set "LLVM_PATH=D:\llvm22\bin"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "PATH=%LLVM_PATH%;%PATH%"
pushd "%ROOT%project"
if /i "%~1"=="codegen" (
  "%SDK%\bin\rexglue.exe" codegen doritoscrashcourse_manifest.toml || exit /b 1
  shift
)
cmake --preset win-amd64-release "-DCMAKE_PREFIX_PATH=%SDK%" || exit /b 1
if /i "%~1"=="supervised" (
  python "%ROOT%..\ps2-perf-tools\buildsup.py" "%DORITOS_BUILD_ROOT%/win-amd64-release" doritoscrashcourse "D:\CodexBuilds\logs\doritos-build.status"
) else (
  cmake --build "%DORITOS_BUILD_ROOT%/win-amd64-release" --parallel 4
)
set "RC=%errorlevel%"
popd
exit /b %RC%
