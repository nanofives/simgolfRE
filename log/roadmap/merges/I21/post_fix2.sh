#!/bin/sh
# I21 after both suites: apply the S3 lead's second wave 8 fix layer (log/roadmap/S3/w8_fix_i21b.py: test_gl_wgl pin 125 -> 9 px,
# the self-check's negative control through glWgl.rasterCheck(1); gl_wgl.h / gl_wgl.cpp test-only path), rebuild x64 + Win32, then
# re-run the two host-GL files on both backends and the segment gate on both (the rebuilt DLLs must still equal the gated build's
# rows). Each heavy step one S3 lock hold as step I21r. Orchestrator, 2026-10-31. Results: suite_fix/post_fix2_summary.txt.
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I21 || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I21/env.sh
O=$R/log/roadmap/merges/I21
S=$O/suite_fix/post_fix2_summary.txt
mkdir -p $O/suite_fix
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
take() { $LOCKPY take I21r --priority --what "$1" --need-gb ${MINFREE:-16} > /dev/null 2>&1; }
rel() { $LOCKPY release I21r > /dev/null 2>&1; }
echo "start $(date '+%F %T')" > $S
~/bin/py -3.12 $R/log/roadmap/S3/w8_fix_i21b.py --root . >> $S 2>&1; echo "apply rc=$?" >> $S
take "post_fix2 build"
sh $O/build.sh
rel
echo "[build] $(tail -2 $O/build_x64.log | tr '\n' ' ') / $(tail -1 $O/build_w32.log)" >> $S
for be in x64 win32; do
  for b in test_gl_wgl test_gl_wgl_selfcheck; do
    take "post_fix2 $b $be"
    if [ $be = win32 ]; then
      SG_BACKEND=win32 SG_APP_EXE=$SG_APP_EXE ~/bin/py -3.12 -m pytest port/test/$b.py -m "" -q -p no:cacheprovider > $O/suite_fix/post2_${be}_$b.txt 2>&1
    else
      SG_APP_EXE=$SG_APP_EXE ~/bin/py -3.12 -m pytest port/test/$b.py -m "" -q -p no:cacheprovider > $O/suite_fix/post2_${be}_$b.txt 2>&1
    fi
    rc=$?
    rel
    echo "$be $b rc=$rc $(tail -1 $O/suite_fix/post2_${be}_$b.txt) $(date '+%T')" >> $S
  done
done
take "post_fix2 segs_x64"
$TS -m port.test.integrator segments --backend x64 --jobs 4 --out $O/suite_fix/post2_segs_x64.tsv > $O/suite_fix/post2_segs_x64.log 2>&1
echo "segs_x64 rc=$? $(tail -1 $O/suite_fix/post2_segs_x64.log | cut -c1-200)" >> $S
rel
take "post_fix2 segs_win32"
$TS -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out $O/suite_fix/post2_segs_win32.tsv > $O/suite_fix/post2_segs_win32.log 2>&1
echo "segs_win32 rc=$? $(tail -1 $O/suite_fix/post2_segs_win32.log | cut -c1-200)" >> $S
rel
echo "[post_fix2 done] $(date '+%F %T')" >> $S
