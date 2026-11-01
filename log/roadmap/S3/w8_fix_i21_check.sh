#!/bin/bash
# Runs the two files w8_fix_i21.py edits on port/build/S3_W8, x64 and Win32, one w8_lock.py hold per file.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
S=log/roadmap/S3
T=port/build/S3_W8
O=$S/w8_out
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL="$(cd $T && pwd -W)/port/build/x64/Release/portsim_c.dll"
N=4; k=0
for be in x64 win32; do
  for f in test_gfx_pixel test_gfx_reports; do
    k=$((k+1))
    if [ $be = win32 ]; then E="SG_BACKEND=win32"; else E="SG_BACKEND=x64"; fi
    ~/bin/py -3.12 $S/w8_lock.py run S3-lead --what "w8 fix $f $be" -- bash -c "cd $T && $E ~/bin/py -3.12 -m pytest port/test/$f.py -q -p no:cacheprovider" > $O/fix_${f}_$be.log 2>&1
    echo "[$k/$N] $f $be rc=$? :: $(grep -E 'passed|failed|error' $O/fix_${f}_$be.log | tail -n 1)"
  done
done
echo FIX_CHECK_DONE
