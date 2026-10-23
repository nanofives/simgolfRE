@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
set CL=
set _CL_=
cd /d %~dp0
cl /nologo /O2 glexact.c /Fe:glexact.exe opengl32.lib glu32.lib gdi32.lib user32.lib
cl /nologo /O2 glreplay.c /Fe:glreplay.exe opengl32.lib glu32.lib gdi32.lib user32.lib
