@echo off
rem decompile.cmd 0xADDR [0xADDR ...] -> D:\CodexBuilds\doritos\ghidra2\out\<addr>.c
"C:\Users\dudua\Downloads\ghidra_12.1.4_PUBLIC_20260921\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" D:\CodexBuilds\doritos\ghidra2 dcc -process default-pe-ghidra.bin -noanalysis -readOnly -scriptPath "%~dp0." -postScript DecompileList.java %* > D:\CodexBuilds\doritos\ghidra2\run.log 2>&1
findstr /c:"decompiled" /c:"no function" D:\CodexBuilds\doritos\ghidra2\run.log
