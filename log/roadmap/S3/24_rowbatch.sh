#!/bin/bash
# S3.24: one 24_rows.py batch (run inside a lock hold by 24_turn.sh). usage: 24_rowbatch.sh <rule> <tag> <scenarios> [extra args]
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_24 || exit 1
export SG_PORTSIM_DLL="$(pwd -W)/port/build/x64/Release/portsim_c.dll"
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
R=$1; TAG=$2; SC=$3; shift 3
~/bin/py -3.12 ../../../log/roadmap/S3/24_rows.py --rule $R --tag $TAG --scenarios $SC "$@"
