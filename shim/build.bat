@echo off
rem Build simgolf_shim as winmm.dll (x86) and deploy it next to golf_clean.exe.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d %~dp0
rem An inherited CL (Git Bash / Windows PowerShell here carry CL=/MP12) makes cl relaunch itself per source and
rem mangles the quoted /Fo path into a stray "C:\Program.obj" link input; this build sets its own flags.
set CL=
set _CL_=
rem Parallel builds (one per agent, CLAUDE.md "parallel C3"): SIMGOLF_BUILD_DIR (objects + dll), SIMGOLF_DEPLOY_DIR
rem (install copy to deploy into) and SIMGOLF_RE_BATCHES (batch list) override build, ..\original and re_batches.txt.
set BDIR=build
if defined SIMGOLF_BUILD_DIR set BDIR=%SIMGOLF_BUILD_DIR%
set DDIR=..\original
if defined SIMGOLF_DEPLOY_DIR set DDIR=%SIMGOLF_DEPLOY_DIR%
set BLIST=re_batches.txt
if defined SIMGOLF_RE_BATCHES set BLIST=%SIMGOLF_RE_BATCHES%
if not exist "%BDIR%" mkdir "%BDIR%"
rem Matched sources compiled by the original compiler (optional; needs tools\vc6 from the user's VS6 disc).
rem A parallel build reuses the main build's VC6 object instead of rebuilding it into the shared build\vc6.
set VC6OBJ=
set VC6DEF=
if exist ..\tools\vc6\vc98\bin\cl.exe (
  if not defined SIMGOLF_BUILD_DIR call "%~dp0build_vc6.bat" || exit /b 1
  set VC6OBJ=build\vc6\terrain.obj
  set VC6DEF=/DSG_VC6_TERRAIN
)
rem Reimplementations: the core files always; a parallel C3 batch (src\re\<ID>.cpp) only once it is listed in
rem re_batches.txt (or %SIMGOLF_RE_BATCHES%), so unfinished batches neither break the build nor install unverified hooks.
set REFILES=src\re\hooks.cpp src\re\Terrain.cpp src\re\golf_*.cpp
if exist "%BLIST%" for /f "usebackq eol=# tokens=*" %%f in ("%BLIST%") do call set REFILES=%%REFILES%% src\re\%%f
cl /nologo /O2 /MT /W3 /EHsc /D_CRT_SECURE_NO_WARNINGS %VC6DEF% /Fo"%BDIR%\\" /LD ^
   src\shim.cpp src\patches.cpp src\coverage.cpp %REFILES% src\winmm_proxy.gen.cpp ^
   deps\minhook\buffer.cpp deps\minhook\hook.cpp deps\minhook\trampoline.cpp deps\minhook\hde\hde32.cpp ^
   %VC6OBJ% /link /DEF:src\winmm.def /OUT:"%BDIR%\winmm.dll" user32.lib kernel32.lib shlwapi.lib || exit /b 1
copy /y "%BDIR%\winmm.dll" "%DDIR%\winmm.dll" >nul || exit /b 1
echo deployed %DDIR%\winmm.dll
