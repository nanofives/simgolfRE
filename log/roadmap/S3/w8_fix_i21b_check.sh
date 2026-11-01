#!/bin/bash
# Builds port/build/S3_W8 (x64, Win32) after w8_fix_i21b.py and runs the two files it edits on both backends, one w8_lock.py hold per step.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
S=log/roadmap/S3
T=port/build/S3_W8
O=$S/w8_out
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL="$(cd $T && pwd -W)/port/build/x64/Release/portsim_c.dll"
N=6; k=0
for arch in x64 Win32; do
  k=$((k+1))
  ~/bin/py -3.12 $S/w8_lock.py run S3-lead --what "w8 fix2 build $arch" -- bash -c "~/bin/py -3.12 $S/24_build.py $arch --tree S3_W8" > $O/fix2_build_$arch.log 2>&1
  echo "[$k/$N] build $arch rc=$?"
done
for be in x64 win32; do
  for f in test_gl_wgl test_gl_wgl_selfcheck; do
    k=$((k+1))
    ~/bin/py -3.12 $S/w8_lock.py run S3-lead --what "w8 fix2 $f $be" -- bash -c "cd $T && SG_BACKEND=$be ~/bin/py -3.12 -m pytest port/test/$f.py -q -p no:cacheprovider" > $O/fix2_${f}_$be.log 2>&1
    echo "[$k/$N] $f $be rc=$? :: $(grep -E 'passed|failed|error' $O/fix2_${f}_$be.log | tail -n 1)"
  done
done
echo FIX2_CHECK_DONE
