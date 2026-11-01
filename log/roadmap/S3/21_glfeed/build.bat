@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
set CL=
set _CL_=
cd /d %~dp0
cl /nologo /O2 glfeed.c /Fe:glfeed.exe opengl32.lib gdi32.lib user32.lib
