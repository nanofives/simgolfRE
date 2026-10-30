#!/bin/sh
# S3.16 heavy gates on the mirror port/build/S3_16, one step at a time, each under one wave 7 token (W7_HEAVY_RULE.md: --jobs 1).
# Prints "[N/M] step rc=.. s :: last line"; full output in log/roadmap/S3/16_g_<step>.log.
#   sh log/roadmap/S3/16_gates.sh [first] [last]
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_16 || exit 1
L=../../../log/roadmap/S3
O=$L/16
. $L/14_env.sh
unset SG_BACKEND SG_APP_PRIORITY SG_GFX_CHECKPOINT_AT
PY=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
# WAITPID=<windows pid>: first wait for that process (a step started by an earlier run of this script) to end, then release
# TOKEN=<lock dir name> it held
if [ -n "${WAITPID:-}" ]; then
  while tasklist //FI "PID eq $WAITPID" | grep -q " $WAITPID "; do sleep 60; done
  rm -rf $L/${TOKEN:-w7_heavy.lock}
  echo "[wait] pid $WAITPID ended, token ${TOKEN:-w7_heavy.lock} released"
fi
first=${1:-1}
last=${2:-999}
STEPS="build_w32 census census_tgt segs_x64 segs_win32 boot_x64 boot_win32"
m=$(echo $STEPS | wc -w)
TOK=""
take() {
  while :; do
    for t in w7_heavy.lock w7_heavy2.lock; do
      if mkdir $L/$t 2>/dev/null; then
        echo "S3.16 $1 $(date '+%F %T')" > $L/$t/owner.txt
        TOK=$L/$t
        return
      fi
    done
    sleep 150
  done
}
n=0
for s in $STEPS; do
  n=$((n + 1))
  [ $n -lt $first ] && continue
  [ $n -gt $last ] && continue
  take $s
  t0=$(date +%s)
  case $s in
    build_w32) (cd ../../.. && sh log/roadmap/S3/12_build.sh S3_16 Win32 && mv log/roadmap/S3/12_build_S3_16_Win32.log log/roadmap/S3/16_build_w32.log) ;;
    census) test-slot $PY -m port.test.framegate --all --jobs 1 --out ${O}_census.tsv --resume ;;
    # without the snapshot fix: only the scenarios holding the 7 checkpoints whose POLL lies before status.tgt's call (a scan of
    # every census scenario, 16_notes.md section 3); every other checkpoint gets the same clock ordinal in both modes
    census_tgt) SG_GFX_CHECKPOINT_AT=tgt test-slot $PY -m port.test.framegate cov_a_flags cov_a_keys3 cov_k1_paths cov_k1_river cov_k1_routing cov_l_bridge --jobs 1 --out ${O}_census_tgt.tsv --resume ;;
    segs_x64) test-slot $PY -m port.test.integrator segments --backend x64 --jobs 1 --out ${O}_segs_x64.tsv ;;
    segs_win32) test-slot $PY -m port.test.integrator segments --backend win32 $W32 --jobs 1 --out ${O}_segs_win32.tsv ;;
    boot_x64) test-slot $PY -m port.test.boot --all --jobs 1 --out ${O}_boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 test-slot $PY -m port.test.boot --all --jobs 1 --out ${O}_boot_win32.tsv ;;
  esac > ${O}_g_$s.log 2>&1
  rc=$?
  rm -rf $TOK
  echo "[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 ${O}_g_$s.log | cut -c1-160)"
done
echo "[done] gates"
