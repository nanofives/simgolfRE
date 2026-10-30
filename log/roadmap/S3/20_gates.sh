#!/bin/sh
# S3.20: the gates on port/build/S3_20 (w7_assemble.py + 20_edits.py), copied from 9_gates.sh (S3.9). Same commands as 17_gates.sh / I15's gates.sh,
# with --jobs ${JOBS:-1} (W7_HEAVY_RULE.md: one token = one process) and one w7 heavy token taken per step and released after it.
# Each step tees its full output to log/roadmap/S3/20_out/$TREE/g_<step>.log and prints "[N/M] step rc=.. s :: last line".
# Usage: sh log/roadmap/S3/20_gates.sh <step>...   (steps: see the case table)
trap '' PIPE
T=${TREE:-S3_20}
R=/c/Users/maria/Desktop/Proyectos/SimGolf
L=$R/log/roadmap/S3
cd $R/port/build/$T || exit 1
TREE=$T . $L/17_env.sh
unset SG_GL_BACKEND SG_BACKEND
export SG_APP_FACTS_DIR=$L/20_out/facts_$T
mkdir -p $SG_APP_FACTS_DIR
S=$L/20_out/$T
mkdir -p $S
J=${JOBS:-1}
PY="$HOME/bin/py -3.12"
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
NEWT="port/test/test_gfx_s3_17.py port/test/test_gfx_s3_18.py port/test/test_gfx_s3_19.py port/test/test_gfx_info_panel.py port/test/test_gl_wgl.py"
take() {
  while :; do
    for k in w7_heavy w7_heavy2; do
      if mkdir $L/$k.lock 2>/dev/null; then TOK=$L/$k.lock; echo "S3.20 $1 $T $(date '+%F %T')" > $TOK/owner.txt; return; fi
    done
    sleep 120
  done
}
release() {   # only our own token
  case "$(cat $TOK/owner.txt 2>/dev/null)" in "S3.20 "*) rm -rf $TOK ;; *) echo "token $TOK not ours, left" ;; esac
}
suite() {   # $1 = out dir, $2 = file list; one file per process (log/p38 suite_by_file.sh pattern)
  mkdir -p $1; m=$(echo $2 | wc -w); n=0
  for f in $2; do
    n=$((n + 1)); b=$(basename $f .py); t0=$(date +%s)
    $PY -m pytest $f -m "" -q -p no:cacheprovider > $1/$b.txt 2>&1; rc=$?
    [ $rc -eq 0 ] || [ $rc -eq 5 ] && st=PASS || st=FAIL
    echo "[$n/$m] $b $st $(( $(date +%s) - t0 )) s ($(tail -1 $1/$b.txt))"
  done
}
m=$#
n=0
for s in "$@"; do
  n=$((n + 1))
  waitmem
  take $s
  G=$S/g_$s.log
  echo "== $s start $(date '+%F %T') free=${FREE}GB tree=$T jobs=$J token=$TOK" > $G
  t0=$(date +%s)
  case $s in
    build_x64) $PY port/test/build.py --arch x64 ;;
    build_w32) $PY port/test/build.py --arch Win32 ;;
    gfx_check) $TS $L/13_gfx_check.py x64 win32 ;;
    static) $PY -m pytest -m static -q -p no:cacheprovider ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs $J --out $S/segs_x64.tsv ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 $W32 --jobs $J --out $S/segs_win32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs $J --out $S/boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs $J --out $S/boot_win32.tsv ;;
    live_equiv_x64) $TS -m port.test.live equiv --all --jobs $J --out $S/equiv_x64.tsv ;;
    live_equiv_win32) $TS -m port.test.live equiv --all --jobs $J --backend win32 --out $S/equiv_win32.tsv ;;
    app_replay_x64) $TS -m port.test.app_replay gate --out $S/app_replay_x64.tsv ;;
    app_replay_win32) $TS -m port.test.app_replay gate --backend win32 --out $S/app_replay_win32.tsv ;;
    k_raster_app) $TS -m pytest port/test/test_app_ingame.py -k test_app_gl_raster_end_to_end -v -p no:cacheprovider ;;
    t_*) f=${s#t_}; $TS -m pytest port/test/$f.py -q -p no:cacheprovider ;;
    w32_*) f=${s#w32_}; SG_BACKEND=win32 $TS -m pytest port/test/$f.py -q -p no:cacheprovider ;;
    ctl13) $TS -m port.test.framegate cov_a_champ --checkpoints n_yes,end --no-probe --negctl tick.frame=13 --out $S/ctl13.tsv ;;
    census) $TS -m port.test.framegate --all --jobs $J --no-probe --out $S/census.tsv --resume ;;
    census_raster) $TS -m port.test.framegate --all --jobs $J --no-probe --gl raster --out $S/census_raster.tsv --resume ;;
    fg_w32) SG_BACKEND=win32 $TS -m port.test.framegate sandbox_basic cov_k3_camera2 cov_k1_river --jobs $J --no-probe --out $S/fg_w32.tsv ;;
    fg_hostgl) $TS -m port.test.framegate championship cov_a_build2 cov_a_build3 cov_a_champ cov_a_flags cov_a_golfer cov_a_keys cov_a_keys2 cov_a_keys3 cov_a_play cov_b_decor cov_b_holes18 cov_k1_paths cov_k1_river cov_k1_routing cov_k2_build cov_l_bridge cov_l_camera cov_r_bio cov_r_palette_hot ui_tour ui_tour_v --jobs $J --no-probe --host-gl --out $S/fg_hostgl.tsv ;;
    fg_x64) $TS -m port.test.framegate sandbox_basic cov_k3_camera2 cov_k1_river --jobs $J --no-probe --out $S/fg_x64.tsv ;;
    suite_x64) suite $S/suite_x64 "$(ls port/test/test_*.py)" ;;
    suite_win32) export SG_BACKEND=win32; suite $S/suite_win32 "$(cat $R/log/roadmap/merges/I15/win32_files.txt) $NEWT"; unset SG_BACKEND ;;
    *) echo "unknown step $s"; false ;;
  esac >> $G 2>&1
  rc=$?
  release
  line="[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $G | cut -c1-160)"
  echo "$line" >> $S/g_gates.log
  echo "$line" 2>/dev/null || true
done
echo "[done] gates $*"
