#!/bin/sh
# I13: every regression gate of re/roadmap/BASELINE.md (HEAD d66aa755 = 919dbdd5 + S7) plus S3 resubmission b gates (MERGE_w5b.md section 5) on
# the mirror port/build/I13 (13_assemble.py --dst I13 --commit d66aa755), one heavy run at a time. Each
# step waits for >= 16 GB free commit charge, tees its full output to log/roadmap/merges/I13/g_<name>.log and prints
# "[N/M] name rc=.. s :: last line". The suites are separate steps of suite_by_file.sh (split with FROM / TO by the caller).
# Usage: sh log/roadmap/merges/I13/gates.sh <step name>...
trap '' PIPE   # I13 second session: outlive the background shell (see suite_by_file.sh)
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I13 || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I13/env.sh
O=log/roadmap/merges/I13
PY="$HOME/bin/py -3.12"
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
mkdir -p i13/ledger
m=$#
n=0
for s in "$@"; do
  n=$((n + 1))
  waitmem
  echo "== $s start $(date '+%F %T') free=${FREE}GB" > $O/g_$s.log
  t0=$(date +%s)
  case $s in
    static) $PY -m pytest -m static -q -p no:cacheprovider ;;
    live_equiv_x64) $TS -m port.test.live equiv --all --jobs 3 --out $O/equiv_x64.tsv ;;
    live_equiv_win32) $TS -m port.test.live equiv --all --jobs 3 --backend win32 --out $O/equiv_win32.tsv ;;
    app_replay_x64) $TS -m port.test.app_replay gate --out $O/app_replay_x64.tsv ;;
    app_replay_win32) $TS -m port.test.app_replay gate --backend win32 --out $O/app_replay_win32.tsv ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_x64.tsv ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out $O/segs_win32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 3 --out $O/boot_x64.tsv ;;
    segs_ft_x64) SG_TEXT_METRICS=freetype $TS -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_ft_x64.tsv ;;
    boot_ft_x64) SG_TEXT_METRICS=freetype $TS -m port.test.boot --all --jobs 3 --out $O/boot_ft_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 3 --out $O/boot_win32.tsv ;;
    chain) $TS -m port.test.integrator chain --backend x64 --all --jobs 4 --out i13/ledger/chain_x64.tsv && cp i13/ledger/chain_x64.tsv $O/ ;;
    defects) $TS -m port.test.integrator defects --backend x64 --jobs 4 --out i13/ledger/defects_x64.tsv &&
             cp $O/segs_x64.tsv i13/ledger/segs_x64.tsv &&
             $TS -m port.test.integrator ledger --segments i13/ledger/segs_x64.tsv --chain i13/ledger/chain_x64.tsv --defects i13/ledger/defects_x64.tsv --out i13/ledger/divergences.tsv;
             cp i13/ledger/defects_x64.tsv $O/; cp i13/ledger/divergences.tsv $O/ledger_divergences.tsv ;;
    gfx_check) $TS ../../../log/roadmap/S3/13_gfx_check.py x64 win32 ;;
    census) $TS -m port.test.framegate --all --jobs 2 --out $O/census.tsv --resume ;;
    terrain) $TS log/roadmap/S3/2_census.py terrain --jobs 3 --out $O/terrain_verdicts.tsv ;;
    *) echo "unknown step $s"; false ;;
  esac >> $O/g_$s.log 2>&1
  rc=$?
  line="[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $O/g_$s.log | cut -c1-160)"
  echo "$line" >> $O/g_gates.log
  echo "$line" 2>/dev/null || true
done
echo "[done] gates $*"
