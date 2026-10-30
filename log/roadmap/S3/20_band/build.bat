@echo off
rem S3.20 band measurement: glband.exe (x64, like simgolf_port.exe)
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set CL=
set _CL_=
cd /d %~dp0
cl /nologo /O2 /W3 glband.c /Fe:glband.exe opengl32.lib gdi32.lib user32.lib
