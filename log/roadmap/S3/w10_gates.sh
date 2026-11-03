#!/bin/sh
# S3 wave 10 (lead): the regression gates on the stacked tree port/build/S3_W10 (w10_assemble.py: w10_base + 31 + 30 + 32; steps from 32_gates.sh + I22 gates.sh). Each step writes log/roadmap/S3/w10_out/g_<name>.log and
# appends one line to g_gates.log. Run each step through w10_run_gates.sh (w8_lock hold + one test slot per step).
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/S3_W10
cd $M || exit 1
O=$R/log/roadmap/S3/w10_out
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
    build_x64) $PY $R/log/roadmap/S3/24_build.py x64 --tree S3_W10 ;;
    build_w32) $PY $R/log/roadmap/S3/24_build.py Win32 --tree S3_W10 ;;
    static) $TS -m pytest port/test/test_static.py -q -p no:cacheprovider ;;
    repo_static) $TS -m pytest -m static -q -p no:cacheprovider ;;
    s1_trace) $TS -m pytest tests/test_trace_format_s1.py tests/test_trace_format.py -m "" -q -p no:cacheprovider ;;
    segs_ft_x64) SG_TEXT_METRICS=freetype $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $O/segs_ft_x64.tsv ;;
    boot_ft_x64) SG_TEXT_METRICS=freetype $TS -m port.test.boot --all --jobs 1 --out $O/boot_ft_x64.tsv ;;
    live_equiv_x64) $TS -m port.test.live equiv --all --jobs 3 --out $O/equiv_x64.tsv ;;
    live_equiv_win32) $TS -m port.test.live equiv --all --jobs 3 --backend win32 --out $O/equiv_win32.tsv ;;
    chain) $TS -m port.test.integrator chain --backend x64 --all --jobs 4 --out $O/chain_x64.tsv ;;
    app_replay_x64) $TS -m port.test.app_replay gate --out $O/app_replay_x64.tsv ;;
    app_replay_win32) $TS -m port.test.app_replay gate --backend win32 --out $O/app_replay_win32.tsv ;;
    gfx_check)$TS $R/log/roadmap/S3/13_gfx_check.py x64 win32 ;;
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
