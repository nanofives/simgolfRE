@echo off
rem Compile the matched sources (re\match\*.cpp listed below) with the ORIGINAL compiler (VC6 cl 12.00.8168)
rem into objects the MSVC 2022 shim links. /Zl: no VC6 default-library directives (the shim uses the
rem modern static CRT). Release flags = the flags that matched golf_clean.exe.
setlocal
cd /d %~dp0
set VC=%~dp0..\tools\vc6\vc98
if not exist "%VC%\bin\cl.exe" (echo VC6 not extracted to tools\vc6 & exit /b 1)
set PATH=%VC%\bin;%PATH%
set INCLUDE=%VC%\include
rem A machine-wide CL=/MP12 (modern MSVC option) would make VC6 warn D4002; VC6 must not see it.
set CL=
if not exist build\vc6 mkdir build\vc6
"%VC%\bin\cl.exe" /nologo /c /O2 /Zl /Gy /Fobuild\vc6\terrain.obj ..\re\match\terrain.cpp || exit /b 1
echo built build\vc6\terrain.obj with VC6
