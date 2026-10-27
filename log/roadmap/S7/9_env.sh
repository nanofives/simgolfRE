#!/bin/sh
# S7.9: source from the repo root: `. log/roadmap/S7/9_env.sh [win32]` -> cwd = the mirror, DLL / runner / app env set.
R=$(pwd)
M=$R/port/build/S7_9_head
L=$R/log/roadmap/S7
cd "$M" || return 1
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_APP_EXE=$M/port/build/x64/app/Release/simgolf_port.exe
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
if [ "$1" = win32 ]; then export SG_BACKEND=win32; else unset SG_BACKEND; fi
