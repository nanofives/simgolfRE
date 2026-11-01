#!/bin/sh
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I21 || exit 1
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
O=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I21
~/bin/py -3.12 port/test/build.py --arch x64 > $O/build_x64.log 2>&1; echo "x64 rc=$?" >> $O/build_x64.log
~/bin/py -3.12 port/test/build.py --arch Win32 > $O/build_w32.log 2>&1; echo "w32 rc=$?" >> $O/build_w32.log
echo "[build] done $(date '+%T')" >> $O/build_x64.log
