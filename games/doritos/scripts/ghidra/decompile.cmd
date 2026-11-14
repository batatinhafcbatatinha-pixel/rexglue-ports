@echo off
rem decompile.cmd 0xADDR [0xADDR ...] -> %DORITOS_GHIDRA%\out\<addr>.c
rem GHIDRA_HOME = Ghidra install folder; DORITOS_GHIDRA = folder of the local Ghidra project (outside this repository).
if "%GHIDRA_HOME%"=="" (echo set GHIDRA_HOME to the Ghidra install folder & exit /b 1)
if "%DORITOS_GHIDRA%"=="" set DORITOS_GHIDRA=D:\CodexBuilds\doritos\ghidra2
"%GHIDRA_HOME%\support\analyzeHeadless.bat" "%DORITOS_GHIDRA%" dcc -process default-pe-ghidra.bin -noanalysis -readOnly -scriptPath "%~dp0." -postScript DecompileList.java %* > "%DORITOS_GHIDRA%\run.log" 2>&1
findstr /c:"decompiled" /c:"no function" "%DORITOS_GHIDRA%\run.log"
