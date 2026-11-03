#!/bin/sh
# S3.30: the regression gates on port/build/S3_30 (copy of log/roadmap/merges/I22/gates.sh's steps; one w8_lock hold per step,
# --jobs 1 as W10_COMMON.md asks). Each step writes log/roadmap/S3/30_work/g_<name>.log and appends a line to g_gates.log.
# Usage (lock first, then one test slot per step):
#   for s in <steps>; do ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S3.30 --what "30_gates $s" -- bash -c "test-slot sh log/roadmap/S3/30_gates.sh $s"; done
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/S3_30
cd $M || exit 1
O=$R/log/roadmap/S3/30_work
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_TREE=$M
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY
PY="$HOME/bin/py -3.12"
# run the whole script inside ONE test slot (test-slot sh log/roadmap/S3/30_gates.sh ...): the steps then take no second one
export PROYECTOS_TEST_SLOT=off
TS="/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
LOCKPY="$PY $R/log/roadmap/S3/w8_lock.py"
m=$#
n=0
for s in "$@"; do
  n=$((n + 1))
  unset SG_BACKEND
  echo "== $s start $(date '+%F %T')" > $O/g_$s.log
  t0=$(date +%s)
  case $s in
    static_trace) $PY -m pytest tests/test_trace_format.py tests/test_segment_corpus.py -m static -q -p no:cacheprovider ;;
    s1_trace) $PY -m pytest tests/test_trace_format_s1.py -m "" -q -p no:cacheprovider ;;
    validate_all)$PY $R/log/roadmap/S3/30_validate_all.py ;;
    validate_segs) $PY $R/log/roadmap/S3/30_validate_all.py --segments ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $O/segs_x64.tsv ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 $W32 --jobs 1 --out $O/segs_win32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 1 --out $O/boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 1 --out $O/boot_win32.tsv ;;
    live_equiv_x64) $TS -m port.test.live equiv --all --jobs 3 --out $O/equiv_x64.tsv ;;
    live_equiv_x64p) $TS $R/log/roadmap/S3/30_live_equiv.py $O/equiv_x64.tsv ;;
    controls_x64)$TS $R/log/roadmap/S3/30_controls.py ;;
    controls_win32) SG_BACKEND=win32 $TS $R/log/roadmap/S3/30_controls.py ;;
    census_ge)$TS -m port.test.framegate golfer_events --split --jobs 1 --resume --out $O/census_split_golfer_events.tsv ;;
    t_*) f=${s#t_}; $TS -m pytest port/test/$f.py -m "" -q -p no:cacheprovider ;;
    w32_*) f=${s#w32_}; SG_BACKEND=win32 $TS -m pytest port/test/$f.py -m "" -q -p no:cacheprovider ;;
    *) echo "unknown step $s"; false ;;
  esac >> $O/g_$s.log 2>&1
  rc=$?
  line="[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $O/g_$s.log | cut -c1-160)"
  echo "$line" >> $O/g_gates.log
  echo "$line"
done
echo "[done] gates $*"
