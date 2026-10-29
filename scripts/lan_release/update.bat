@echo off
rem SimGolf port: download the latest build from the mini-server into THIS folder (only changed files).
rem First time: put update.bat in an empty folder and run it; it fetches simgolf_update.ps1 and the game.
if not exist "%~dp0simgolf_update.ps1" (
  echo Fetching simgolf_update.ps1 from the mini-server ...
  powershell -NoProfile -ExecutionPolicy Bypass -Command "try { Invoke-WebRequest -UseBasicParsing -Uri 'http://mariano-server.local:8088/simgolf/simgolf_update.ps1' -OutFile '%~dp0simgolf_update.ps1' } catch { Invoke-WebRequest -UseBasicParsing -Uri 'http://192.168.68.112:8088/simgolf/simgolf_update.ps1' -OutFile '%~dp0simgolf_update.ps1' }"
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0simgolf_update.ps1" -InstallRoot "%~dp0." %*
pause
