#!/bin/sh
# S3.15 heavy gates on the mirror port/build/S3_15 (ee6efc1d + wave 6 + 15_edits; = I15's tree + S3.15), after I15.
# One wave 7 heavy token per gate (w7_heavy.lock or w7_heavy2.lock), --jobs 1. Logs log/roadmap/S3/15_g_<step>.log,
# one summary line per step in 15_gates.log. Usage: sh log/roadmap/S3/15_gates.sh <step>...
#   census_gl     framegate --all, GlRaster (the default)        -> 15_census_gl.tsv
#   census_wgl    framegate --all --host-gl                       -> 15_census_wgl.tsv
#   segs_x64 / segs_win32 / boot_x64 / boot_win32                 -> 15_segs_*.tsv / 15_boot_*.tsv
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
L=$R/log/roadmap/S3
O=../../../log/roadmap/S3
cd $R/port/build/S3_15 || exit 1
M=$PWD
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY SG_GL_BACKEND SG_BACKEND
TS="/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"   # the whole script runs inside one test slot (test-slot bash -c)
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
take() {
  while :; do
    for k in w7_heavy w7_heavy2; do
      if mkdir $L/$k.lock 2>/dev/null; then TOK=$L/$k.lock; echo "S3.15 $1 $(date '+%F %T')" > $TOK/owner.txt; return; fi
    done
    echo "  tokens busy: $(cat $L/w7_heavy.lock/owner.txt 2>/dev/null) | $(cat $L/w7_heavy2.lock/owner.txt 2>/dev/null)"
    sleep 150
  done
}
for s in "$@"; do
  take $s
  echo "== $s start $(date '+%F %T') token $TOK" > $L/15_g_$s.log
  t0=$(date +%s)
  case $s in
    census_gl) $TS -m port.test.framegate --all --jobs 1 --no-probe --out $O/15_census_gl.tsv --resume ;;
    census_wgl) $TS -m port.test.framegate --all --jobs 1 --no-probe --host-gl --out $O/15_census_wgl.tsv --resume ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $O/15_segs_x64.tsv ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 $W32 --jobs 1 --out $O/15_segs_win32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 1 --out $O/15_boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 1 --out $O/15_boot_win32.tsv ;;
    *) echo "unknown step $s" ;;
  esac >> $L/15_g_$s.log 2>&1
  rc=$?
  rm -rf $TOK
  echo "[$s] rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $L/15_g_$s.log)" | tee -a $L/15_gates.log
done
echo GATES_DONE
