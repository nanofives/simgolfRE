@echo off
rem Compile-only check of one reimplementation file (no link, no deploy): shim\check_re.bat src\re\<file>.cpp
rem Safe to run while the game or another build is running; objects go to %TEMP%.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d %~dp0
cl /nologo /c /O2 /MT /W3 /EHsc /D_CRT_SECURE_NO_WARNINGS /Fo"%TEMP%\\" %* || exit /b 1
echo compile OK: %*
