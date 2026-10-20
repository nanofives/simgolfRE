#!/bin/sh
# 38g private build: mirror (working tree + routing) -> port/build/m38g (x64), copied to port/build/m38g_run (tests load that copy)
set -e
cd "$(dirname "$0")/../.."
CM="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
~/bin/py -3.12 log/p38/38g_mirror.py
[ -f port/build/m38g/CMakeCache.txt ] || "$CM" -S port/build/m38g_root/port -B port/build/m38g -A x64 > log/p38/38g_cmake.log 2>&1
touch port/build/m38g_root/port/src/registry.cpp
"$CM" --build port/build/m38g --config Release --target portsim_c -- -m -v:m > log/p38/38g_build.log 2>&1 || { grep -E "error|Error" log/p38/38g_build.log | head -40; exit 1; }
mkdir -p port/build/m38g_run
cp port/build/m38g/Release/portsim_c.dll port/build/m38g_run/portsim_c.dll
echo "built port/build/m38g_run/portsim_c.dll"
