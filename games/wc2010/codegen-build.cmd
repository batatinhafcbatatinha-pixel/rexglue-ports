@echo off
rem Codegen (ReXGlue, ~10 min) then the supervised build. Queue entry for D:\CodexBuilds\bq.sh.
cd /d %~dp0project
"%~dp0..\battlefield2\third_party\rexglue-sdk\win-amd64\bin\rexglue.exe" codegen wc2010_manifest.toml > "%~dp0analysis\codegen.log" 2>&1
if errorlevel 1 exit /b 1
call "%~dp0build-detached.cmd"
