@echo off
rem Portable SimGolf: SafeDisc-free exe + simgolf_shim (winmm.dll proxy) = windowed, no registry, no install.
start "" /D "%~dp0original" golf_clean.exe
