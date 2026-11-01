#!/bin/sh
# I21 (S7 wave 4 on f2dff003): the regression gates of re/roadmap/BASELINE.md plus S7's switch-0 runs (segs0 / boot0 =
# log/roadmap/S7/12_lead_segs_sw0.py / 12_lead_boot_sw0.py, as log/roadmap/S7/w4_gates.sh) on the mirror port/build/I21 (copy of I18's gates.sh). Each step waits for >= MINFREE GB
# free commit charge, writes log/roadmap/merges/I21/g_<name>.log and appends "[N/M] name rc=.. s :: last line" to g_gates.log.
# The census runs use --no-probe, as S3.20 measured them (20_gates.sh).
# Usage: [LT=<suffix>] sh log/roadmap/merges/I21/gates.sh <step>...
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I21 || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I21/env.sh
O=log/roadmap/merges/I21
PY="$HOME/bin/py -3.12"
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
mkdir -p s3r/ledger
m=$#
n=0
for s in "$@"; do
  n=$((n + 1))
  unset SG_BACKEND
  lock_take "gate $s"
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
    segs0_x64) $PY /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S7/12_lead_segs_sw0.py $O/segs0_x64.tsv ;;
    segs0_win32) SG_BACKEND=win32 $PY /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S7/12_lead_segs_sw0.py $O/segs0_win32.tsv ;;
    boot0_x64) $PY /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S7/12_lead_boot_sw0.py $O/boot0_x64.tsv --jobs 3 ;;
    boot0_win32) SG_BACKEND=win32 $PY /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S7/12_lead_boot_sw0.py $O/boot0_win32.tsv --jobs 3 ;;
    net_win32) SG_BACKEND=win32 $PY -m pytest port/test/test_net.py -m "" -q -p no:cacheprovider ;;
    gfx_check) $TS log/roadmap/S3/13_gfx_check.py x64 win32 ;;
    census) $TS -m port.test.framegate --all --jobs 2 --no-probe --out $O/census.tsv --resume ;;
    census_split) $TS -m port.test.framegate --all --jobs 2 --no-probe --split --out $O/census_split.tsv --resume ;;
    census_auto) $TS -m port.test.framegate --all --jobs 2 --no-probe --gl auto --out $O/census_auto.tsv --resume ;;
    terrain) $TS log/roadmap/S3/2_census.py terrain --jobs 3 --out $O/terrain_verdicts.tsv ;;
    ctl13) $TS -m port.test.framegate cov_a_champ --checkpoints n_yes,end --no-probe --negctl tick.frame=13 --out $O/ctl13.tsv ;;
    t_*) f=${s#t_}; $TS -m pytest port/test/$f.py -m "" -q -p no:cacheprovider ;;
    w32_*) f=${s#w32_}; SG_BACKEND=win32 $TS -m pytest port/test/$f.py -m "" -q -p no:cacheprovider ;;
    *) echo "unknown step $s"; false ;;
  esac >> $O/g_$s$LT.log 2>&1
  rc=$?
  lock_release
  line="[$n/$m] $s$LT rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $O/g_$s$LT.log | cut -c1-160)"
  echo "$line" >> $O/g_gates.log
  echo "$line" 2>/dev/null || true
done
echo "[done] gates $*"
