@echo off
rem SimGolf port: download the latest build from the mini-server into THIS folder (only changed files).
rem First time: put update.bat in an empty folder and run it; it fetches simgolf_update.ps1 and the game.
rem Every run fetches the newest simgolf_update.ps1 first, so a change to the updater applies on the same run.
rem If the server is unreachable the copy already here is used.
echo Fetching the latest simgolf_update.ps1 from the mini-server ...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$o='%~dp0simgolf_update.ps1.dl_tmp'; foreach ($u in 'http://mariano-server.local:8088/simgolf/simgolf_update.ps1','http://192.168.68.112:8088/simgolf/simgolf_update.ps1') { try { Invoke-WebRequest -UseBasicParsing -TimeoutSec 30 -Uri $u -OutFile $o; Move-Item -LiteralPath $o -Destination '%~dp0simgolf_update.ps1' -Force; exit 0 } catch { } }; Remove-Item -LiteralPath $o -Force -ErrorAction SilentlyContinue; exit 1"
if not exist "%~dp0simgolf_update.ps1" (
  echo ERROR: could not fetch simgolf_update.ps1 from the mini-server.
  pause
  exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0simgolf_update.ps1" -InstallRoot "%~dp0." %*
pause
