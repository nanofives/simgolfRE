@echo off
rem Portable SimGolf: SafeDisc-free exe + simgolf_shim (winmm.dll proxy) = windowed, no registry, no install.
rem The game saves to "saved games\" under its working directory; the installer created it, so do it here.
if not exist "%~dp0original\saved games" mkdir "%~dp0original\saved games"
start "" /D "%~dp0original" golf_clean.exe
