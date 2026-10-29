@echo off
rem SimGolf port: start the game on this folder's game files. Saves go to %LOCALAPPDATA%\SimGolfPort.
start "" /D "%~dp0." "%~dp0simgolf_port.exe" --install "%~dp0game"
