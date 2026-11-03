#!/bin/sh
# S3.32: the regression gates on port/build/S3_32 (steps from 30_gates.sh). Each step writes log/roadmap/S3/32_work/g_<name>.log and
# appends one line to g_gates.log. Run each step through 32_run_gates.sh (w8_lock hold + one test slot per step).
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/S3_32
cd $M || exit 1
O=$R/log/roadmap/S3/32_work
mkdir -p $O
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_TREE=$M
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY
PY="$HOME/bin/py -3.12"
export PROYECTOS_TEST_SLOT=off
TS="/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
NAMES=$($PY -c "import json;print(' '.join(sorted(json.load(open('re/analysis/data/scenario_refs.json'))['scenarios'])))")
batch() {   # census batch k (1-based) of 7 scenarios in scenario_refs.json order
  echo $NAMES | tr ' ' '\n' | sed -n "$(( ($1 - 1) * 7 + 1 )),$(( $1 * 7 ))p" | tr '\n' ' '
}
m=$#
n=0
for s in "$@"; do
  n=$((n + 1))
  unset SG_BACKEND
  echo "== $s start $(date '+%F %T')" > $O/g_$s.log
  t0=$(date +%s)
  case $s in
    static) $TS -m pytest port/test/test_static.py -q -p no:cacheprovider ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $O/segs_x64.tsv ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 $W32 --jobs 1 --out $O/segs_win32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 1 --out $O/boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 1 --out $O/boot_win32.tsv ;;
    census_[0-9]*) k=${s#census_}; $TS -m port.test.framegate $(batch $k) --split --jobs 1 --resume --out $O/census_split.tsv ;;
    census_full) $TS -m port.test.framegate --all --split --jobs 1 --resume --out $O/census_split.tsv ;;
    ctl14) $TS -m port.test.framegate cov_k3_camera --checkpoints y_inj3 --split --jobs 1 --negctl tick.frame=14 --out $O/ctl14.tsv ;;
    ctl0) $TS -m port.test.framegate cov_k3_camera --checkpoints y_inj3 --split --jobs 1 --out $O/ctl0.tsv ;;
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
