#!/bin/bash
# 3.7b build of the full working tree (tick + commands) in port/build/p37b, copied per build so a running test never locks it
CM="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /c/Users/maria/Desktop/Proyectos/SimGolf
touch port/src/registry.cpp
"$CM" --build port/build/p37b --config Release --target portsim_c --parallel 2>&1 | grep -E "error|warning C|-> " | head -40
if [ -f port/build/p37b/Release/portsim_c.dll ]; then
  mkdir -p port/build/p37b_run; n=$(date +%s); cp port/build/p37b/Release/portsim_c.dll port/build/p37b_run/portsim_c_$n.dll
  echo "export SG_PORTSIM_DLL=$PWD/port/build/p37b_run/portsim_c_$n.dll" > log/p3/p37b/env_full.sh
fi
