#!/bin/sh
# P2: build the mirror port/build/P2 for one arch ($1 = x64 | Win32) with FreeType.
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/P2 || exit 1
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
~/bin/py -3.12 port/test/build.py --arch $1 > ../../../log/roadmap/P2/build_$1.log 2>&1; echo "$1 rc=$?" >> ../../../log/roadmap/P2/build_$1.log
