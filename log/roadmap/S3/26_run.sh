#!/bin/bash
# S3.26: run a command in the S3_26 tree with its x64 (default) or Win32 DLL: 26_run.sh [--w32] <command ...>
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_26 || exit 1
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
if [ "$1" = "--w32" ]; then shift; export SG_BACKEND=win32; export SG_PORTSIM_DLL="$PWD/port/build/Win32/Release/portsim_c.dll"
else export SG_PORTSIM_DLL="$PWD/port/build/x64/Release/portsim_c.dll"; fi
exec "$@"
