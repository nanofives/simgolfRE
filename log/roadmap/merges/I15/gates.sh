#!/bin/sh
# I15 (S3 wave 6 on ee6efc1d): the regression gates of re/roadmap/BASELINE.md (I14's gates.sh minus S4's own gate) plus S3.12's
# gate (MERGE_w6.md sections 4 / 7) on the mirror port/build/I15. Each step waits for >= MINFREE GB free commit charge, writes
# log/roadmap/merges/I15/g_<name>.log and appends "[N/M] name rc=.. s :: last line" to g_gates.log.
# Usage: [LT=<suffix>] sh log/roadmap/merges/I15/gates.sh <step>...
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I15 || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I15/env.sh
O=log/roadmap/merges/I15
PY="$HOME/bin/py -3.12"
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
mkdir -p s3r/ledger
m=$#
n=0
for s in "$@"; do
  n=$((n + 1))
  unset SG_BACKEND
  waitmem
  echo "== $s start $(date '+%F %T') free=${FREE}GB" > $O/g_$s$LT.log
  t0=$(date +%s)
  case $s in
    static) $PY -m pytest -m static -q -p no:cacheprovider ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_x64.tsv ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out $O/segs_win32.tsv ;;
    segs_ft_x64) SG_TEXT_METRICS=freetype $TS -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_ft_x64.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 3 --out $O/boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 3 --out $O/boot_win32.tsv ;;
    boot_ft_x64) SG_TEXT_METRICS=freetype $TS -m port.test.boot --all --jobs 3 --out $O/boot_ft_x64.tsv ;;
    live_equiv_x64) $TS -m port.test.live equiv --all --jobs 3 --out $O/equiv_x64.tsv ;;
    live_equiv_win32) $TS -m port.test.live equiv --all --jobs 3 --backend win32 --out $O/equiv_win32.tsv ;;
    app_replay_x64) $TS -m port.test.app_replay gate --out $O/app_replay_x64.tsv ;;
    app_replay_win32) $TS -m port.test.app_replay gate --backend win32 --out $O/app_replay_win32.tsv ;;
    chain) $TS -m port.test.integrator chain --backend x64 --all --jobs 4 --out s3r/ledger/chain_x64.tsv && cp s3r/ledger/chain_x64.tsv $O/ ;;
    defects) $TS -m port.test.integrator defects --backend x64 --jobs 4 --out s3r/ledger/defects_x64.tsv &&
             cp $O/segs_x64.tsv s3r/ledger/segs_x64.tsv &&
             $TS -m port.test.integrator ledger --segments s3r/ledger/segs_x64.tsv --chain s3r/ledger/chain_x64.tsv --defects s3r/ledger/defects_x64.tsv --out s3r/ledger/divergences.tsv;
             cp s3r/ledger/defects_x64.tsv $O/; cp s3r/ledger/divergences.tsv $O/ledger_divergences.tsv ;;
    gfx_check) $TS log/roadmap/S3/13_gfx_check.py x64 win32 ;;
    census) $TS -m port.test.framegate --all --jobs 2 --out $O/census.tsv --resume ;;
    terrain) $TS log/roadmap/S3/2_census.py terrain --jobs 3 --out $O/terrain_verdicts.tsv ;;
    s3_raster) $PY -m pytest port/test/test_gl_raster.py port/test/test_terrain_state.py -m "" -q -p no:cacheprovider ;;
    frames) $TS $R/log/roadmap/S3/12_frames.py $O/frames_default.tsv && $PY $R/log/roadmap/S3/12_frames.py --compare $R/log/roadmap/S3/10_frames_old.tsv $O/frames_default.tsv ;;
    frames_t4) $TS $R/log/roadmap/S3/12_frames.py $O/frames_t4.tsv --line "#threads 4" && $PY $R/log/roadmap/S3/12_frames.py --compare $R/log/roadmap/S3/10_frames_old.tsv $O/frames_t4.tsv ;;
    frames_ctl) $TS $R/log/roadmap/S3/12_frames.py $O/frames_workerctl.tsv --line "#threads 4
#workerctl 1";
                $PY $R/log/roadmap/S3/12_frames.py --compare $R/log/roadmap/S3/10_frames_old.tsv $O/frames_workerctl.tsv; echo "(control: rows must differ)" ;;
    equiv) $TS $R/log/roadmap/S3/12_equiv.py --out $O/equiv_raster.tsv ;;
    s3_ctl_live) SG_TREE=$M $TS $R/log/roadmap/S3/12_ctl_live.py sandbox_basic cov_k1_terrain ;;
    s3_session) $PY $R/$O/session.py I15 i15 --runs 3 ;;
    *) echo "unknown step $s"; false ;;
  esac >> $O/g_$s$LT.log 2>&1
  rc=$?
  line="[$n/$m] $s$LT rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $O/g_$s$LT.log | cut -c1-160)"
  echo "$line" >> $O/g_gates.log
  echo "$line" 2>/dev/null || true
done
echo "[done] gates $*"
