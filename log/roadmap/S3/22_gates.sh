#!/bin/bash
# S3.22 gate for W8.6 (heavy lock held by the caller): w32 test_gfx, segments x64, boot x64, one run each, --jobs 1.
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_22 || exit 1
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL="$PWD/port/build/x64/Release/portsim_c.dll"
TS="$HOME/bin/py -3.12"
echo "[1/3] w32_test_gfx"; SG_BACKEND=win32 $TS -m pytest port/test/test_gfx.py -q -p no:cacheprovider 2>&1 | tee $S/22_pytest_gfx_w32.log | tail -3
echo "[2/3] segs_x64"; $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $S/22_segs_x64.tsv 2>&1 | tee $S/22_segs_x64.log | tail -3
echo "[3/3] boot_x64"; $TS -m port.test.boot --all --jobs 1 --out $S/22_boot_x64.tsv 2>&1 | tee $S/22_boot_x64.log | tail -3
echo ALLDONE
