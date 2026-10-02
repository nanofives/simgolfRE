@echo off
rem Build simgolf_shim as winmm.dll (x86) and deploy it next to golf_clean.exe.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d %~dp0
if not exist build mkdir build
rem Matched sources compiled by the original compiler (optional; needs tools\vc6 from the user's VS6 disc).
set VC6OBJ=
set VC6DEF=
if exist ..\tools\vc6\vc98\bin\cl.exe (
  call "%~dp0build_vc6.bat" || exit /b 1
  set VC6OBJ=build\vc6\terrain.obj
  set VC6DEF=/DSG_VC6_TERRAIN
)
cl /nologo /O2 /MT /W3 /EHsc /D_CRT_SECURE_NO_WARNINGS %VC6DEF% /Fobuild\ /LD ^
   src\shim.cpp src\patches.cpp src\coverage.cpp src\re\*.cpp src\winmm_proxy.gen.cpp ^
   deps\minhook\buffer.cpp deps\minhook\hook.cpp deps\minhook\trampoline.cpp deps\minhook\hde\hde32.cpp ^
   %VC6OBJ% /link /DEF:src\winmm.def /OUT:build\winmm.dll user32.lib kernel32.lib shlwapi.lib || exit /b 1
copy /y build\winmm.dll ..\original\winmm.dll >nul || exit /b 1
echo deployed original\winmm.dll
