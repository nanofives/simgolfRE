#!/bin/sh
# S3.12: the wave 6 gates on port/build/S3_12 (12_assemble.py), one heavy run at a time (log/roadmap/merges/I13/gates.sh's
# pattern). Each step waits for >= 16 GB free commit charge, tees its full output to log/roadmap/S3/12_g_<name>.log and prints
# "[N/M] name rc=.. s :: last line". TREE=S3_12_head runs a step on the reference tree (01d84188 + I13's gfx_terrain.cpp).
# Usage: sh log/roadmap/S3/12_gates.sh <step name>...
trap '' PIPE
T=${TREE:-S3_12}
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/$T || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3/12_env.sh
O=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
S=$O/12_out/$T
mkdir -p $S s312/ledger
PY="$HOME/bin/py -3.12"
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
m=$#
n=0
for s in "$@"; do
  n=$((n + 1))
  waitmem
  L=$S/g_$s.log
  echo "== $s start $(date '+%F %T') free=${FREE}GB tree=$T" > $L
  t0=$(date +%s)
  case $s in
    static) $PY -m pytest -m static -q -p no:cacheprovider ;;
    frames) $TS $O/12_frames.py $S/frames_default.tsv && $TS $O/12_frames.py --compare $O/10_frames_old.tsv $S/frames_default.tsv ;;
    frames_t4) $TS $O/12_frames.py $S/frames_t4.tsv --line "#threads 4" && $TS $O/12_frames.py --compare $O/10_frames_old.tsv $S/frames_t4.tsv ;;
    frames_ctl) $TS $O/12_frames.py $S/frames_workerctl.tsv --line "#threads 4\n#workerctl 1";
                $TS $O/12_frames.py --compare $O/10_frames_old.tsv $S/frames_workerctl.tsv; echo "(control: rows must differ)" ;;
    equiv) $TS $O/12_equiv.py --out $S/equiv.tsv ;;
    live_equiv_x64) $TS -m port.test.live equiv --all --jobs 3 --out $S/equiv_x64.tsv ;;
    app_replay_x64) $TS -m port.test.app_replay gate --out $S/app_replay_x64.tsv ;;
    app_replay_win32) $TS -m port.test.app_replay gate --backend win32 --out $S/app_replay_win32.tsv ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 4 --out $S/segs_x64.tsv ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out $S/segs_win32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 3 --out $S/boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 3 --out $S/boot_win32.tsv ;;
    census) $TS -m port.test.framegate --all --jobs 2 --out $S/census.tsv --resume ;;
    terrain) $TS ../../../log/roadmap/S3/2_census.py terrain --jobs 3 --out $S/terrain_verdicts.tsv ;;
    *) echo "unknown step $s"; false ;;
  esac >> $L 2>&1
  rc=$?
  line="[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $L | cut -c1-160)"
  echo "$line" >> $S/g_gates.log
  echo "$line" 2>/dev/null || true
done
echo "[done] gates $*"
