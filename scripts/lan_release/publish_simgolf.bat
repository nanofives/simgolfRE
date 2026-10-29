@echo off
rem Build the SimGolf port from the committed HEAD and publish it to the mini-server (http://mariano-server.local:8088/simgolf/).
rem Pass --no-build to re-publish the last build. Players then run update.bat in their SimGolf folder.
"%ProgramFiles%\Git\bin\bash.exe" "%~dp0publish.sh" %*
pause
