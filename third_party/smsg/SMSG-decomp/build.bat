@echo off

setlocal
set ERRORLEVEL=
set ROOT=%~dp0
if "%VC6%"=="" set VC6=%ROOT%toolchain\vc6
if "%VC6SP5%"=="" set VC6SP5=%ROOT%toolchain\vc6sp5
if "%VC6SP2LIB%"=="" set VC6SP2LIB=%ROOT%toolchain\vc6sp2\VC98\lib\lib
if "%VC6REDIST%"=="" set VC6REDIST=%ROOT%toolchain\vc6sp2\VC98\redist
if "%DXSDK%"=="" set DXSDK=%ROOT%sound\DXSDK
set BASEPATH=%PATH%
set FAILED=

if /i "%1"=="check" goto checkonly
if /i "%1"=="format" goto format

set D_GOLF=%ROOT%golf
set D_TERRAIN=%ROOT%terrain\Terrain.dll
set D_JGLD=%ROOT%jgl\jgld.dll
set D_LIBPNG=%ROOT%jgl\libpng.lib
set D_ZLIB=%ROOT%jgl\zlib.lib
set I_JPEG=%ROOT%golf\jpeg.lib\jpeg-6a
set I_LPC10=%ROOT%sound\lpc10.lib\lpc10\lpc10-1.5\lpc55-C
set I_TERRAIN=%ROOT%terrain\Terrain.dll\include
set I_SOUND=%ROOT%sound\sound.dll\include
set I_JGLD=%ROOT%jgl\jgld.dll\include
set I_BINK=%ROOT%golf\binkw32.lib
if not "%EXACT%"=="1" goto chosen
set D_GOLF=C:\SimGolf\golf
set D_TERRAIN=C:\Projects\3DTerrainLowPoly
set D_JGLD=C:\JackDev
set D_LIBPNG=C:\libpng.lib
set D_ZLIB=C:\zlib.lib
set I_JPEG=C:\SimGolf\external\jpeg-6a
set I_LPC10=C:\SimGolf\external\lpc10-1.5
set I_TERRAIN=C:\SimGolf\decompilation\terrain\Terrain.dll\include
set I_SOUND=C:\SimGolf\decompilation\sound\sound.dll\include
set I_JGLD=C:\SimGolf\decompilation\jgl\jgld.dll\include
set I_BINK=C:\SimGolf\external\bink
set DXSDK=C:\DXSDK
set VC6=C:\Program Files\Microsoft Visual Studio
call :need "%VC6%\VC98\Bin\CL.EXE"
call :need "%D_GOLF%\golf.dsw"
call :need "%D_TERRAIN%\Terrain.dsp"
call :need "%D_JGLD%\jgld.dsp"
call :need "%D_LIBPNG%\libpng.dsp"
call :need "%D_ZLIB%\zlib.dsp"
call :need "%I_JPEG%\jpeglib.h"
call :need "%I_LPC10%\lpc10.h"
call :need "%I_TERRAIN%\Terrain.h"
call :need "%I_SOUND%\sound.h"
call :need "%I_JGLD%\jgldefs.h"
call :need "%I_BINK%\binkw32.h"
call :need "%DXSDK%\include\ddraw.h"
if not "%FAILED%"=="" exit /b 1
set PROBE=.simgolf-exact-%RANDOM%%RANDOM%.probe
call :same "%D_GOLF%" golf
call :same "%D_TERRAIN%" terrain\Terrain.dll
call :same "%D_JGLD%" jgl\jgld.dll
call :same "%D_LIBPNG%" jgl\libpng.lib
call :same "%D_ZLIB%" jgl\zlib.lib
call :same "%I_JPEG%" golf\jpeg.lib\jpeg-6a
call :same "%I_LPC10%" sound\lpc10.lib\lpc10\lpc10-1.5\lpc55-C
call :same "%I_TERRAIN%" terrain\Terrain.dll\include
call :same "%I_SOUND%" sound\sound.dll\include
call :same "%I_JGLD%" jgl\jgld.dll\include
call :same "%I_BINK%" golf\binkw32.lib
if not "%FAILED%"=="" exit /b 1
:chosen

set WHAT=%1
if "%WHAT%"=="" set WHAT=all
call :formatcheck
if errorlevel 1 goto refused
if "%WHAT%"=="all" goto terrain
goto %WHAT%

:terrain
set INCLUDE=%VC6%\VC98\INCLUDE;%VC6%\VC98\MFC\INCLUDE
set LIB=%VC6SP2LIB%;%VC6%\VC98\LIB
call :msdev "%VC6%" "%D_TERRAIN%" Terrain.dsp "Terrain - Win32 Debug"
if not "%WHAT%"=="all" goto done

:jgl
set INCLUDE=%VC6%\VC98\INCLUDE;%VC6%\VC98\MFC\INCLUDE;%ROOT%jgl\libpng.lib\libpng;%ROOT%jgl\zlib.lib\zlib
set LIB=%VC6SP2LIB%;%VC6%\VC98\LIB
call :msdev "%VC6%" "%D_ZLIB%" zlib.dsp "zlib - Win32 Debug"
call :msdev "%VC6%" "%D_LIBPNG%" libpng.dsp "libpng - Win32 Debug"
call :msdev "%VC6%" "%D_JGLD%" jgld.dsp "jgld - Win32 Debug"
if not "%WHAT%"=="all" goto done

:sound
set INCLUDE=%VC6%\VC98\INCLUDE
set LIB=%VC6%\VC98\LIB
call :msdev "%VC6%" "%ROOT%sound\lpc10.lib" lpc10.dsp "lpc10 - Win32 Release"
set INCLUDE=%DXSDK%\include;%VC6%\VC98\INCLUDE;%VC6%\VC98\MFC\INCLUDE;%DXSDK%\include;%I_LPC10%
set LIB=%DXSDK%\lib;%VC6SP5%\VC98\LIB;%DXSDK%\lib
call :msdev "%VC6SP5%" "%ROOT%sound\sound.dll" sound.dsp "sound - Win32 Release"
if not "%WHAT%"=="all" goto done

:golf
if "%BINKSDK%"=="" call :bink
set INCLUDE=%DXSDK%\include;%VC6%\VC98\INCLUDE;%VC6%\VC98\MFC\INCLUDE;%DXSDK%\include;%I_JPEG%;%I_TERRAIN%;%I_SOUND%;%I_LPC10%;%I_JGLD%;%I_BINK%
set LIB=%VC6%\VC98\LIB;%D_TERRAIN%\Debug;%BINKSDK%;%DXSDK%\lib
call :msdev "%VC6%" "%D_GOLF%" golf.dsw "golf - Win32 Release"
goto done

:bink
set INCLUDE=%VC6SP5%\VC98\INCLUDE
set LIB=%VC6SP5%\VC98\LIB
call :msdev "%VC6SP5%" "%D_GOLF%\binkw32.lib" binkw32.dsp "binkw32 - Win32 Release"
set BINKSDK=%D_GOLF%\binkw32.lib\Release
if not exist "%BINKSDK%\binkw32.lib" set FAILED=%FAILED% binkw32
goto :eof

:need
if exist %1 goto :eof
echo EXACT: %~1 is missing -- see the README
set FAILED=%FAILED% %~1
goto :eof

:same
echo probe> "%ROOT%%~2\%PROBE%" 2>nul
if exist "%~1\%PROBE%" goto sameok
echo EXACT: %~1 is not this tree's %~2 -- make it a junction to %ROOT%%~2, as the README's table says.
echo        The formatting check reads this tree and MSDEV would compile what %~1 holds instead.
set FAILED=%FAILED% %~1
:sameok
if exist "%ROOT%%~2\%PROBE%" del "%ROOT%%~2\%PROBE%"
goto :eof

:msdev
set PATH=%~1\Common\MSDev98\Bin;%~1\VC98\Bin;%VC6REDIST%;%BASEPATH%
cd /d "%~2"
echo === %~2 (%~4)
MSDEV.EXE %3 /MAKE %4 /REBUILD /USEENV /OUT build.log
if errorlevel 1 set FAILED=%FAILED% %~2
"%SystemRoot%\system32\find.exe" " error(s)," build.log
goto :eof

:done
if not "%FAILED%"=="" echo FAILED:%FAILED% ^(see build.log in each^)
if not "%FAILED%"=="" exit /b 1
echo Done.
goto :eof

:checkonly
call :formatcheck
if errorlevel 1 goto refused
exit /b 0
:refused
if defined FMTBAD if exist "%FMTBAD%" del "%FMTBAD%"
if defined FMTHELD if exist "%FMTHELD%" del "%FMTHELD%"
if defined FMTDIR if exist "%FMTDIR%" rmdir "%FMTDIR%"
exit /b 1

:formatcheck
setlocal EnableDelayedExpansion
set ERRORLEVEL=
call :scratch
pushd "%ROOT%"
call :findcf
if errorlevel 1 goto formatfail
for %%x in (lst cfg out bad fmt prune spaced held badfence af rf inc incbad fl optbad one ver sha dump odd fcd hl rl rls mk.bat) do if exist "%T%.%%x" del "%T%.%%x"
for /f "usebackq eol=# tokens=*" %%p in (".clang-format-ignore") do call :addprune "%%p"
call :oddnames .
for %%f in (*) do call :fmtfile "%%f"
for /d %%s in (*) do call :fmtdir "%%s"
if exist "%T%.spaced" echo FORMAT CHECK: these names hold a character other than A-Z a-z 0-9 . _ - -- a space, or one cmd reads as syntax -- which this tree's names never do. Rename them:
if exist "%T%.spaced" type "%T%.spaced"
if exist "%T%.spaced" goto formatfail
if not exist "%T%.lst" echo FORMAT CHECK: no C or C++ source under %ROOT% -- nothing checked is not a pass.
if not exist "%T%.lst" goto formatfail
call :fmtconfigs
if errorlevel 1 goto formatfail
call :fmtoptouts
if errorlevel 1 goto formatfail
"%CF%" --verbose --dry-run -Werror --files="%T%.lst" > "%T%.out" 2>&1 <nul
set CFRC=0
if errorlevel 1 set CFRC=!ERRORLEVEL!
set NFMT=0
set NERR=0
set NVIO=0
set NREAD=0
set CUR=
set LASTV=
for /f "usebackq tokens=1,2,3 delims= " %%a in ("%T%.out") do (
  if "%%a"=="Formatting" set "CUR=%%c"
  if "%%a"=="Formatting" >>"%T%.fmt" echo %%c
  if "%%a"=="Formatting" set /a NFMT+=1
  if "%%a"=="Error" set /a NREAD+=1
  if "%%b"=="error:" set /a NERR+=1
  if "%%b"=="error:" if not "!CUR!"=="!LASTV!" >>"%T%.bad" echo !CUR!
  if "%%b"=="error:" set "LASTV=!CUR!"
)
for /f "usebackq tokens=1,2 delims=[" %%a in ("%T%.out") do if "%%b"=="-Wclang-format-violations]" set /a NVIO+=1
set RUNERR=
if not "%NREAD%"=="0" set RUNERR=1
if %NERR% GTR %NVIO% set RUNERR=1
if not "%CFRC%"=="0" if "%NVIO%"=="0" set RUNERR=1
if defined RUNERR echo.
if defined RUNERR echo FORMAT CHECK: clang-format COULD NOT CHECK THIS TREE -- it exited %CFRC% -- so nothing was checked and nothing compiled. Its output:
if defined RUNERR type "%T%.out"
if defined RUNERR goto formatfail
set CENSUSBAD=
if %NFMT% LSS %CENSUS% set CENSUSBAD=1
if "%NFMT%"=="0" echo FORMAT CHECK: clang-format formatted no file -- nothing checked is not a pass.
if "%NFMT%"=="0" goto formatfail
for /f "usebackq delims=" %%f in ("%T%.fmt") do call :scanfile %%f >nul 2>&1
if exist "%T%.mk.bat" del "%T%.mk.bat"
if exist "smsg\line-macros.txt" for /f "usebackq tokens=1,2" %%a in ("smsg\line-macros.txt") do if "%%a"=="macro" call :genlm %%b
if exist "%T%.bad" for /f "usebackq delims=" %%f in ("%T%.bad") do call :markheld %%f
call :fencecompare
echo format: %CFDESC%; %NFMT% source(s) formatted (census %CENSUS%)
set POLICY=
if exist "%T%.badfence" set POLICY=1
if exist "%T%.incbad" set POLICY=1
if exist "%T%.optbad" set POLICY=1
if defined CENSUSBAD set POLICY=1
if not exist "%T%.bad" if not defined POLICY goto formatok
echo.
if exist "%T%.badfence" echo FENCE VIOLATION: a fence is exactly "// clang-format off: " and its reason, and "// clang-format on", each a line of its own, in pairs. Fix it in:
if exist "%T%.badfence" type "%T%.badfence"
if exist "%T%.incbad" echo INCLUDE VIOLATION: these sources include a file of an extension the check does not read; rename it to a C or C++ extension:
if exist "%T%.incbad" type "%T%.incbad"
if exist "%T%.optbad" echo OPT-OUT VIOLATION: the tree's opt-outs differ from smsg\format-optouts.txt; add an opt-out there, visibly, and set that file's sha256 in smsg\lock.json, under pins.records, in the same commit, whose message ends with a Lock-Update: trailer:
if exist "%T%.optbad" type "%T%.optbad"
if defined CENSUSBAD echo CENSUS VIOLATION: clang-format formatted %NFMT% files and the census in smsg\format-optouts.txt says %CENSUS%. A clang-format that skips files, or an ignore pattern that names them, is not a pass; if sources were removed on purpose, lower the census there and set that file's sha256 in smsg\lock.json, under pins.records, in the same commit, whose message ends with a Lock-Update: trailer.
if not exist "%T%.bad" goto formatpolicy
set SHOWN=0
for /f "usebackq delims=" %%f in ("%T%.bad") do call :fmtshow %%f
if %SHOWN% GTR 10 echo   The first ten files are shown; all %SHOWN% are named below.
:formatpolicy
echo.
echo FORMAT CHECK REFUSED THIS TREE -- NOTHING WAS COMPILED.
if not exist "%T%.bad" goto formatnorm
set FIXABLE=
set HANDS=
for /f "usebackq delims=" %%f in ("%T%.bad") do call :fixkind %%f
echo.
echo   These sources are not as clang-format 23.1.3 writes them:
for /f "usebackq delims=" %%f in ("%T%.bad") do echo     %%f
if defined FIXABLE echo.
if defined FIXABLE echo   FIX THEM WITH ONE COMMAND, then build again:
if defined FIXABLE echo.
if defined FIXABLE if not defined SIMGOLF_FORMAT_FIX echo       "%~f0" format
if defined FIXABLE if defined SIMGOLF_FORMAT_FIX echo       %SIMGOLF_FORMAT_FIX%
if defined HANDS echo.
if defined HANDS echo   FIX THESE BY HAND -- each holds a fence, a #line or a line position: an assertion, a __LINE__, a debug
if defined HANDS echo   report or an __asm block. Formatting one can move a compiled byte, so "build.bat format" leaves them
if defined HANDS echo   alone, and an EXACT=1 build checked against smsg\lock.json shows whether one moved. Make these lines as
if defined HANDS echo   clang-format writes them, keeping each line position on the line it is on:
if defined HANDS for /f "usebackq delims=" %%f in ("%T%.held") do call :handlines %%f
:formatnorm
echo.
echo   THE RULE AND WHAT IT PROTECTS: CONTRIBUTING.md, "Formatting"
goto formatfail
:formatok
echo format ok: every formatted source is as clang-format 23.1.3 writes it
for %%x in (lst cfg out fmt prune held af rf inc fl one ver sha dump odd fcd hl rl rls mk.bat) do if exist "%T%.%%x" del "%T%.%%x"
popd
if exist "%T%.d" rmdir "%T%.d"
endlocal & set FMTBAD=
exit /b 0
:formatfail
for %%x in (lst cfg out fmt prune spaced af rf inc fl optbad incbad badfence one ver sha dump odd fcd hl rl rls mk.bat) do if exist "%T%.%%x" del "%T%.%%x"
popd
endlocal & set "FMTBAD=%T%.bad" & set "FMTHELD=%T%.held" & set "FMTDIR=%T%.d"
exit /b 1

:fmtshow
set /a SHOWN+=1
if %SHOWN% GTR 10 goto :eof
echo.
echo FORMAT VIOLATION  %~1   (clang-format's first five places)
"%CF%" --dry-run -Werror --ferror-limit=5 "%~1" > "%T%.one" 2>&1 <nul
type "%T%.one"
set FL=
set FC=
for /f "usebackq tokens=1,2,3 delims=:" %%a in ("%T%.one") do if /i "%%a"=="%~1" if not defined FL set FL=%%b
for /f "usebackq tokens=1,2,3 delims=:" %%a in ("%T%.one") do if /i "%%a"=="%~1" if "%%b"=="%FL%" if not defined FC set FC=%%c
if defined FC call :realline "%~1" %FL% %FC%
if defined FC echo   clang-format marks where each change STARTS: the first line to change is line %RL%
del "%T%.one"
goto :eof

:fixkind
set KIND=FIXABLE
if exist "%T%.held" for /f "usebackq delims=" %%h in ("%T%.held") do if /i "%%h"=="%~1" set KIND=HANDS
set %KIND%=1
goto :eof

:handlines
set HL=0
for /f "usebackq delims=" %%b in ("%T%.bad") do if /i "%%b"=="%~1" set HL=1
if "%HL%"=="0" goto :eof
"%CF%" --dry-run -Werror --ferror-limit=20 "%~1" > "%T%.hl" 2>&1 <nul
set LINES=
if exist "%T%.rls" del "%T%.rls"
for /f "usebackq tokens=1,2,3 delims=:" %%a in ("%T%.hl") do if /i "%%a"=="%~1" call :handone "%~1" %%b %%c
echo     %~1: line(s)%LINES%
del "%T%.hl"
if exist "%T%.rls" del "%T%.rls"
goto :eof
:handone
call :realline %1 %2 %3
call :addline
goto :eof

:oddnames
setlocal DisableDelayedExpansion
dir /b /a "%~1\* *" "%~1\*&*" "%~1\*!*" "%~1\*^*" "%~1\*%%*" "%~1\*(*" "%~1\*)*" "%~1\*;*" "%~1\*,*" "%~1\*=*" "%~1\*+*" "%~1\*[*" "%~1\*]*" "%~1\*{*" "%~1\*}*" "%~1\*'*" "%~1\*`*" "%~1\*@*" "%~1\*#*" "%~1\*$*" > "%T%.odd" 2>nul
set ODD=
for /f "usebackq eol=| delims=" %%z in ("%T%.odd") do if not "%%z"=="" set ODD=1
if defined ODD >>"%T%.spaced" echo     in %~1:
if defined ODD type "%T%.odd" >> "%T%.spaced"
if exist "%T%.odd" del "%T%.odd"
endlocal
goto :eof

:realline
set RL=%~2
set /a LO=%~2+1
set /a HI=%~2+64
call :rlhit %1 %LO% %HI% %2 %3
if not defined HIT goto :eof
:realbisect
if %LO% GEQ %HI% goto realfound
set /a MID=(LO+HI)/2
call :rlhit %1 %LO% %MID% %2 %3
if defined HIT set HI=%MID%
if not defined HIT set /a LO=MID+1
goto realbisect
:realfound
set RL=%LO%
goto :eof
:rlhit
"%CF%" --dry-run -Werror --lines=%~2:%~3 "%~1" > "%T%.rl" 2>&1 <nul
set HIT=
for /f "usebackq tokens=1,2,3 delims=:" %%a in ("%T%.rl") do if /i "%%a"=="%~1" if "%%b"=="%~4" if "%%c"=="%~5" set HIT=1
if exist "%T%.rl" del "%T%.rl"
goto :eof

:markheld
set SEEN=
if exist "%T%.held" for /f "usebackq delims=" %%h in ("%T%.held") do if /i "%%h"=="%~1" set SEEN=1
if defined SEEN goto :eof
call :markers "%~1"
if defined MARKED >>"%T%.held" echo %~1
goto :eof

:addline
set SEEN=
if exist "%T%.rls" for /f "usebackq" %%r in ("%T%.rls") do if "%%r"=="%RL%" set SEEN=1
if defined SEEN goto :eof
>>"%T%.rls" echo %RL%
set "LINES=%LINES% %RL%"
goto :eof

:markers
set MARKED=
for /f "usebackq delims=" %%l in ("%~1") do (
  set "L=%%l"
  call :mkline
)
goto :eof
:mkline
set "L=%L:"=%"
if /i not "%L:__LINE__=%"=="%L%" set MARKED=1
if /i not "%L:assert=%"=="%L%" set MARKED=1
if /i not "%L:VERIFY=%"=="%L%" set MARKED=1
if /i not "%L:_RPT=%"=="%L%" set MARKED=1
if /i not "%L:DXTRACE=%"=="%L%" set MARKED=1
if /i not "%L:#line=%"=="%L%" set MARKED=1
if /i not "%L:clang-format=%"=="%L%" set MARKED=1
if /i not "%L:__asm=%"=="%L%" set MARKED=1
if exist "%T%.mk.bat" call "%T%.mk.bat"
goto :eof

:genlm
set "Q=%~1"
set "Q=%Q:a=%"
set "Q=%Q:b=%"
set "Q=%Q:c=%"
set "Q=%Q:d=%"
set "Q=%Q:e=%"
set "Q=%Q:f=%"
set "Q=%Q:g=%"
set "Q=%Q:h=%"
set "Q=%Q:i=%"
set "Q=%Q:j=%"
set "Q=%Q:k=%"
set "Q=%Q:l=%"
set "Q=%Q:m=%"
set "Q=%Q:n=%"
set "Q=%Q:o=%"
set "Q=%Q:p=%"
set "Q=%Q:q=%"
set "Q=%Q:r=%"
set "Q=%Q:s=%"
set "Q=%Q:t=%"
set "Q=%Q:u=%"
set "Q=%Q:v=%"
set "Q=%Q:w=%"
set "Q=%Q:x=%"
set "Q=%Q:y=%"
set "Q=%Q:z=%"
set "Q=%Q:0=%"
set "Q=%Q:1=%"
set "Q=%Q:2=%"
set "Q=%Q:3=%"
set "Q=%Q:4=%"
set "Q=%Q:5=%"
set "Q=%Q:6=%"
set "Q=%Q:7=%"
set "Q=%Q:8=%"
set "Q=%Q:9=%"
set "Q=%Q:_=%"
if defined Q echo FORMAT CHECK: smsg\line-macros.txt names `%~1`, which is no identifier -- skipped.
if defined Q goto :eof
>>"%T%.mk.bat" echo @if /i not "%%L:%~1=%%"=="%%L%%" set MARKED=1
goto :eof

:addprune
if "%~1"=="" goto :eof
set "PR=%~1"
if not "%PR:~-3%"=="/**" goto :eof
set "PR=%PR:~0,-3%"
set "PR=%PR:/=\%"
>>"%T%.prune" echo %PR%
goto :eof

:fmtdir
set "D=%~1"
if not "%D: =%"=="%D%" >>"%T%.spaced" echo     %~1
if not "%D: =%"=="%D%" goto :eof
if /i "%~nx1"==".git" goto :eof
set PRUNED=
if exist "%T%.prune" for /f "usebackq delims=" %%q in ("%T%.prune") do if /i "%~1"=="%%q" set PRUNED=1
if defined PRUNED goto :eof
call :oddnames "%~1"
for %%f in (%~1\*) do call :fmtfile "%%f"
for /d %%s in (%~1\*) do call :fmtdir "%%s"
goto :eof

:fmtfile
set "N=%~1"
if not "%N: =%"=="%N%" >>"%T%.spaced" echo     %~1
if not "%N: =%"=="%N%" goto :eof
for %%c in (.clang-format _clang-format .clang-format-ignore) do if /i "%~nx1"=="%%c" >>"%T%.cfg" echo %~1
for %%e in (c cc cp cpp cxx c++ h hh hp hpp hxx h++ inl inc ipp ixx tcc tpp txx) do if /i "%~x1"==".%%e" >>"%T%.lst" echo %~1
goto :eof

:scanfile
if exist "%T%.fl" del "%T%.fl"
for /f "usebackq tokens=1-9" %%a in ("%~1") do (
  if "%%a"=="//" if "%%b"=="clang-format" >>"%T%.fl" echo fence %%c %%d
  if "%%a"=="/*" if "%%b"=="clang-format" >>"%T%.fl" echo bad
  if "%%b"=="//" if "%%c"=="clang-format" >>"%T%.fl" echo bad
  if "%%b"=="/*" if "%%c"=="clang-format" >>"%T%.fl" echo bad
  if "%%c"=="//" if "%%d"=="clang-format" >>"%T%.fl" echo bad
  if "%%c"=="/*" if "%%d"=="clang-format" >>"%T%.fl" echo bad
  if "%%d"=="//" if "%%e"=="clang-format" >>"%T%.fl" echo bad
  if "%%d"=="/*" if "%%e"=="clang-format" >>"%T%.fl" echo bad
  if "%%e"=="//" if "%%f"=="clang-format" >>"%T%.fl" echo bad
  if "%%e"=="/*" if "%%f"=="clang-format" >>"%T%.fl" echo bad
  if "%%f"=="//" if "%%g"=="clang-format" >>"%T%.fl" echo bad
  if "%%f"=="/*" if "%%g"=="clang-format" >>"%T%.fl" echo bad
  if "%%g"=="//" if "%%h"=="clang-format" >>"%T%.fl" echo bad
  if "%%g"=="/*" if "%%h"=="clang-format" >>"%T%.fl" echo bad
  if "%%h"=="//" if "%%i"=="clang-format" >>"%T%.fl" echo bad
  if "%%h"=="/*" if "%%i"=="clang-format" >>"%T%.fl" echo bad
  if /i "%%a"=="//clang-format" >>"%T%.fl" echo bad
  if /i "%%a"=="/*clang-format" >>"%T%.fl" echo bad
  if /i "%%b"=="//clang-format" >>"%T%.fl" echo bad
  if /i "%%b"=="/*clang-format" >>"%T%.fl" echo bad
  if /i "%%c"=="//clang-format" >>"%T%.fl" echo bad
  if /i "%%c"=="/*clang-format" >>"%T%.fl" echo bad
  if /i "%%d"=="//clang-format" >>"%T%.fl" echo bad
  if /i "%%d"=="/*clang-format" >>"%T%.fl" echo bad
  if /i "%%e"=="//clang-format" >>"%T%.fl" echo bad
  if /i "%%e"=="/*clang-format" >>"%T%.fl" echo bad
  if /i "%%f"=="//clang-format" >>"%T%.fl" echo bad
  if /i "%%f"=="/*clang-format" >>"%T%.fl" echo bad
  if /i "%%g"=="//clang-format" >>"%T%.fl" echo bad
  if /i "%%g"=="/*clang-format" >>"%T%.fl" echo bad
  if /i "%%h"=="//clang-format" >>"%T%.fl" echo bad
  if /i "%%h"=="/*clang-format" >>"%T%.fl" echo bad
  if /i "%%i"=="//clang-format" >>"%T%.fl" echo bad
  if /i "%%i"=="/*clang-format" >>"%T%.fl" echo bad
  if "%%a"=="#line" >>"%T%.fl" echo line
  if "%%a"=="#" if "%%b"=="line" >>"%T%.fl" echo line
  if "%%a"=="#include" if not "%%~b"=="%%b" >>"%T%.inc" echo %~1 [%%~xb]
  if "%%a"=="#" if "%%b"=="include" if not "%%~c"=="%%c" >>"%T%.inc" echo %~1 [%%~xc]
)
if not exist "%T%.fl" goto :eof
set OPENS=0
set CLOSES=0
set HELDF=
set BADF=
for /f "usebackq tokens=1,2,3" %%x in ("%T%.fl") do (
  if "%%x"=="line" set HELDF=1
  if "%%x"=="bad" set BADF=1
  if "%%x"=="fence" if "%%y"=="off:" if not "%%z"=="" set /a OPENS+=1
  if "%%x"=="fence" if "%%y"=="off:" if not "%%z"=="" set HELDF=1
  if "%%x"=="fence" if "%%y"=="off:" if "%%z"=="" set BADF=1
  if "%%x"=="fence" if "%%y"=="on" if not "%%z"=="" set BADF=1
  if "%%x"=="fence" if "%%y"=="on" if "%%z"=="" set /a CLOSES+=1
  if "%%x"=="fence" if not "%%y"=="off:" if not "%%y"=="on" set BADF=1
)
if not "%OPENS%"=="%CLOSES%" set BADF=1
if defined BADF >>"%T%.badfence" echo     %~1
if defined HELDF >>"%T%.held" echo %~1
for /l %%n in (1,1,%OPENS%) do >>"%T%.af" echo %~1
del "%T%.fl"
goto :eof

:fencecompare
if exist "%T%.inc" for /f "usebackq tokens=1,2" %%a in ("%T%.inc") do call :incext %%a %%b
for /f "usebackq tokens=1,2" %%a in ("smsg\format-optouts.txt") do if "%%a"=="fence" call :recfence %%b
if exist "%T%.af" for /f "usebackq delims=" %%f in ("%T%.af") do call :fencecount %%f
if exist "%T%.rf" for /f "usebackq delims=" %%f in ("%T%.rf") do call :fencecount %%f
goto :eof

:incext
set OKX=
for %%e in (c cc cp cpp cxx c++ h hh hp hpp hxx h++ inl inc ipp ixx tcc tpp txx) do if /i "%~2"=="[.%%e]" set OKX=1
if defined OKX goto :eof
if "%~2"=="[]" >>"%T%.incbad" echo     %~1 includes a file with no extension
if not "%~2"=="[]" >>"%T%.incbad" echo     %~1 includes a file of extension %~2
goto :eof

:recfence
set "RF=%~1"
set "RF=%RF:/=\%"
>>"%T%.rf" echo %RF%
goto :eof

:fencecount
set SEEN=
if exist "%T%.fcd" for /f "usebackq delims=" %%g in ("%T%.fcd") do if /i "%%g"=="%~1" set SEEN=1
if defined SEEN goto :eof
>>"%T%.fcd" echo %~1
set NA=0
set NR=0
if exist "%T%.af" for /f "usebackq delims=" %%g in ("%T%.af") do if /i "%%g"=="%~1" set /a NA+=1
if exist "%T%.rf" for /f "usebackq delims=" %%g in ("%T%.rf") do if /i "%%g"=="%~1" set /a NR+=1
if not "%NA%"=="%NR%" >>"%T%.optbad" echo     %~1 holds %NA% fence^(s^) and the record says %NR%
goto :eof

:fmtconfigs
set CFGBAD=
if exist "%T%.cfg" for /f "usebackq delims=" %%f in ("%T%.cfg") do call :fmtknown %%f
for %%k in (.clang-format golf\.clang-format jgl\.clang-format sound\.clang-format terrain\.clang-format) do if exist %%k call :fmtdisable %%k
for %%d in (. golf jgl sound terrain) do call :fmtdump %%d
if defined CFGBAD exit /b 1
exit /b 0

:fmtknown
for %%k in (.clang-format .clang-format-ignore golf\.clang-format jgl\.clang-format sound\.clang-format terrain\.clang-format) do if /i "%~1"=="%%k" goto :eof
echo FORMAT CHECK: %~1 is a clang-format configuration this tree does not have. clang-format reads it -- it restyles, disables or ignores the files under it -- so it would change what is checked without saying so. Remove it, or put its settings in the binary's own .clang-format.
set CFGBAD=1
goto :eof

:fmtdisable
for /f "usebackq delims=" %%l in ("%~1") do (
  set "L=%%l"
  call :dfline
)
if defined DISABLED echo FORMAT CHECK: %~1 names DisableFormat, which turns the check off for every file it covers.
if defined DISABLED set CFGBAD=1
set DISABLED=
goto :eof

:dfline
set "L=%L:"=%"
if /i not "%L:DisableFormat=%"=="%L%" set DISABLED=1
goto :eof

:fmtdump
"%CF%" --dump-config --assume-filename=%~1\smsg-format-probe.cpp > "%T%.dump" 2>&1 <nul
set DUMPED=
set DISABLED=
call :dumpscan >nul 2>&1
if not defined DUMPED echo FORMAT CHECK: clang-format could not read the configuration of a file in %~1. It said:
if not defined DUMPED type "%T%.dump"
if not defined DUMPED set CFGBAD=1
if defined DISABLED echo FORMAT CHECK: clang-format reads DisableFormat for a file in %~1, so nothing there would be checked.
if defined DISABLED set CFGBAD=1
set DISABLED=
goto :eof

:dumpscan
for /f "usebackq tokens=1,2" %%a in ("%T%.dump") do if "%%a"=="DisableFormat:" set DUMPED=1
for /f "usebackq tokens=1,2" %%a in ("%T%.dump") do if "%%a"=="DisableFormat:" if /i not "%%b"=="false" set DISABLED=1
goto :eof

:fmtoptouts
set CENSUS=
if not exist "smsg\line-macros.txt" echo FORMAT CHECK: no smsg\line-macros.txt, the record of the product's line macros that build.bat format must not move. It is part of this repository: restore it with git checkout HEAD -- smsg/line-macros.txt
if not exist "smsg\line-macros.txt" exit /b 1
if not exist "smsg\format-optouts.txt" echo FORMAT CHECK: no smsg\format-optouts.txt, the record of this tree's formatting opt-outs.
if not exist "smsg\format-optouts.txt" exit /b 1
for /f "usebackq tokens=1,2" %%a in ("smsg\format-optouts.txt") do if "%%a"=="census" set CENSUS=%%b
if "%CENSUS%"=="" echo FORMAT CHECK: smsg\format-optouts.txt has no "census N" line, so a run that formats too few files could not be told from a clean one.
if "%CENSUS%"=="" exit /b 1
for /f "usebackq eol=# tokens=*" %%p in (".clang-format-ignore") do call :ignrec "%%p"
for /f "usebackq tokens=1,2" %%a in ("smsg\format-optouts.txt") do if "%%a"=="ignore" call :ignhas "%%b"
for /f "usebackq tokens=1,2,3" %%a in ("smsg\format-optouts.txt") do if "%%a"=="ignore" if "%%c"=="" >>"%T%.optbad" echo     smsg\format-optouts.txt records ignore %%b without its reason: write it after the pattern, on the same line - what formatting it would move, measured, or why it is never formatted
exit /b 0

:ignrec
if "%~1"=="" goto :eof
set FOUND=
for /f "usebackq tokens=1,2" %%a in ("smsg\format-optouts.txt") do if "%%a"=="ignore" if "%%b"=="%~1" set FOUND=1
if not defined FOUND >>"%T%.optbad" echo     .clang-format-ignore names %~1, which smsg\format-optouts.txt does not record
goto :eof

:ignhas
set FOUND=
for /f "usebackq eol=# tokens=*" %%p in (".clang-format-ignore") do if "%%p"=="%~1" set FOUND=1
if not defined FOUND >>"%T%.optbad" echo     smsg\format-optouts.txt records ignore %~1, which .clang-format-ignore does not name
goto :eof

:findcf
set CF=
if defined CLANGFORMAT set "CF=%CLANGFORMAT:"=%"
if "%CF%"=="" for %%i in (clang-format.exe) do set "CF=%%~$PATH:i"
if "%CF%"=="" goto nocf
"%CF%" --version > "%T%.ver" 2>&1 <nul
if errorlevel 9009 goto nocf
set CFV=
for /f "usebackq delims=" %%v in ("%T%.ver") do if not defined CFV set "CFV=%%v"
if exist "%T%.ver" del "%T%.ver"
if "%CFV%"=="" goto nocf
set "CFT=%CFV:*clang-format version =%"
if "%CFT%"=="%CFV%" goto wrongcf
set CFN=
for /f "tokens=1" %%t in ("%CFT%") do set CFN=%%t
if not "%CFN%"=="23.1.3" goto wrongcf
set CFSHA=
certutil -hashfile "%CF%" SHA256 > "%T%.sha" 2>&1 <nul
if not errorlevel 1 for /f "usebackq skip=1 delims=" %%h in ("%T%.sha") do if not defined CFSHA set "CFSHA=%%h"
if defined CFSHA set "CFSHA=%CFSHA: =%"
if not defined CFSHA set "CFSHA=not computed: no certutil here"
if exist "%T%.sha" del "%T%.sha"
set "CFDESC=clang-format 23.1.3 at %CF% (sha256 %CFSHA%)"
exit /b 0
:nocf
echo FORMAT CHECK: NO CLANG-FORMAT, and this tree does not build without it -- its sources are formatted with
echo   clang-format 23.1.3, and every build checks them before its first compile.
if defined CLANGFORMAT echo   LOOKED AT:   CLANGFORMAT=%CLANGFORMAT% -- set, so the PATH is not searched
if not defined CLANGFORMAT echo   LOOKED AT:   clang-format.exe on the PATH -- none there
echo   INSTALL IT:  the LLVM 23.1.3 release (LLVM-23.1.3-win64.msi, from
echo                https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.3), or
echo                pip install clang-format==23.1.3; or set CLANGFORMAT to a clang-format.exe of that release
echo   THE RULE:    CONTRIBUTING.md, "Formatting"
exit /b 1
:wrongcf
echo FORMAT CHECK: WRONG CLANG-FORMAT. Required 23.1.3 exactly; %CF% says: %CFV%
echo   Another release -- another PATCH release included -- formats these sources differently, so it would
echo   rewrite a tree that is already formatted. That is why the version is pinned by number.
echo   INSTALL IT:  the LLVM 23.1.3 release (LLVM-23.1.3-win64.msi, from
echo                https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.3), or
echo                pip install clang-format==23.1.3; or set CLANGFORMAT to a clang-format.exe of that release
echo   THE RULE:    CONTRIBUTING.md, "Formatting"
exit /b 1

:format
call :formatcheck
if not errorlevel 1 exit /b 0
if not defined FMTBAD exit /b 1
if not exist "%FMTBAD%" exit /b 1
call :scratch
call :findcf
if errorlevel 1 exit /b 1
set HELD=
pushd "%ROOT%"
for /f "usebackq delims=" %%f in ("%FMTBAD%") do call :fmtfix %%f
popd
del "%FMTBAD%"
if defined FMTHELD if exist "%FMTHELD%" del "%FMTHELD%"
if defined FMTDIR if exist "%FMTDIR%" rmdir "%FMTDIR%"
if exist "%T%.d" rmdir "%T%.d"
echo.
echo Checking again:
call :formatcheck
set FMTRC=0
if errorlevel 1 set FMTRC=1
if defined FMTBAD if exist "%FMTBAD%" del "%FMTBAD%"
if defined FMTHELD if exist "%FMTHELD%" del "%FMTHELD%"
if defined FMTDIR if exist "%FMTDIR%" rmdir "%FMTDIR%"
if not defined HELD exit /b %FMTRC%
echo.
echo   NOT FORMATTED BY build.bat format:%HELD%
echo   Each holds a fence, a #line or a line position, where formatting can move a compiled byte: fix the lines
echo   named above by hand, then check that an EXACT=1 build still matches smsg\lock.json.
echo   CONTRIBUTING.md, "Formatting" and "The Build Lock".
exit /b 1

:scratch
set "TS=%TIME: =0%"
set "TS=%TS::=%"
set "TS=%TS:.=%"
set "TS=%TS:,=%"
set SEQ=0
:scratchnext
set /a SEQ+=1
set "T=%TEMP%\simgolf-format-%RANDOM%-%TS%-%SEQ%"
if %SEQ% GTR 200 goto :eof
mkdir "%T%.d" 2>nul
if errorlevel 1 goto scratchnext
goto :eof

:fmtfix
set PINNED=
if defined FMTHELD if exist "%FMTHELD%" for /f "usebackq delims=" %%h in ("%FMTHELD%") do if /i "%%h"=="%~1" set PINNED=1
if defined PINNED set HELD=%HELD% %~1
if defined PINNED goto :eof
echo formatting %~1
"%CF%" -i "%~1" <nul
goto :eof
