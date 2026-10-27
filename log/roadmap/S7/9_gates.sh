#!/bin/sh
# S7.9: the merge gates on the mirror port/build/S7_9_head, one heavy run at a time, from the repo root:
#   sh log/roadmap/S7/9_gates.sh [first step number]
# Each step waits for >= 16 GB free commit charge, writes log/roadmap/S7/9_g_<step>.log and prints "[N/M] step rc=.. s :: last line".
# Baselines (78ffb06d = I7b): log/roadmap/merges/I7/b/{segs,boot}_{x64,win32}.tsv; S7's own rows: I7's live_equiv / app_replay TSVs.
R=$(pwd)
. "$R/log/roadmap/S7/9_env.sh"
PY="$HOME/bin/py -3.12"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
first=${1:-1}
last=${2:-99}

waitmem() {
  while :; do
    f=$(powershell -NoProfile -Command "[int]((Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory/1MB)")
    [ "${f:-0}" -ge "${MINFREE:-16}" ] && return
    echo "  (free commit ${f} GB < ${MINFREE:-16}, waiting)"
    sleep 60
  done
}

STEPS="suite_x64 suite_win32 equivall_slow_x64 equivall_slow_win32 live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32 segs_x64 segs_win32 boot_x64 boot_win32 repo_static"
m=$(echo $STEPS | wc -w)
n=0
for s in $STEPS; do
  n=$((n + 1))
  [ $n -lt $first ] && continue
  [ $n -gt $last ] && break
  waitmem
  t0=$(date +%s)
  case $s in
    suite_x64) (cd "$R" && sh log/roadmap/S7/9_suite.sh x64) ;;
    suite_win32) (cd "$R" && sh log/roadmap/S7/9_suite.sh win32) ;;
    equivall_slow_x64) $PY -m pytest port/test/test_live.py -m slow -k equivalence_all -v -p no:cacheprovider ;;
    equivall_slow_win32) SG_BACKEND=win32 $PY -m pytest port/test/test_live.py -m slow -k equivalence_all -v -p no:cacheprovider ;;
    live_equiv_x64) $PY -m port.test.live equiv --all --jobs 3 --out $L/9_equiv_x64.tsv ;;
    live_equiv_win32) $PY -m port.test.live equiv --all --jobs 3 --backend win32 --out $L/9_equiv_win32.tsv ;;
    app_replay_x64) $PY -m port.test.app_replay gate --out $L/9_app_replay_x64.tsv ;;
    app_replay_win32) $PY -m port.test.app_replay gate --backend win32 --out $L/9_app_replay_win32.tsv ;;
    segs_x64) $PY -m port.test.integrator segments --backend x64 --jobs 4 --out $L/9_segs_x64.tsv ;;
    segs_win32) $PY -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out $L/9_segs_win32.tsv ;;
    boot_x64) $PY -m port.test.boot --all --jobs 3 --out $L/9_boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $PY -m port.test.boot --all --jobs 3 --out $L/9_boot_win32.tsv ;;
    repo_static) $PY -m pytest -m static -q -p no:cacheprovider ;;
  esac > $L/9_g_$s.log 2>&1
  rc=$?
  echo "[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $L/9_g_$s.log | cut -c1-160)"
done
echo "[done] 9_gates"
