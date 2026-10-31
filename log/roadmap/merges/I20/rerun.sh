#!/bin/sh
# I20 (orchestrator 2026-10-31): after the queue ends, re-run ALONE at low load the two block-2 failures:
# 1. SG_BACKEND=win32 test_live.py::test_equivalence_all; if it fails, cov_a_build2 alone with its live / boot detail (Win32);
# 2. x64 test_app_present.py::test_long_session_memory_bounded (load-dependent, as I19's mem_alone.sh).
# Load and free commit charge are logged before each run. Log: rerun.log (one line per step in g_gates.log).
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
O=$R/log/roadmap/merges/I20
cd $R/port/build/I20 || exit 1
. $O/env.sh
L=$O/rerun.log
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
echo "== rerun armed $(date '+%F %T')" > $L
while ! grep -q "^\[queue done\]" $O/g_gates.log; do sleep 60; done
load() { powershell -NoProfile -Command "\$c=(Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average; \$f=[int]((Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory/1MB); \"cpu=\$c% free=\${f}GB\""; }
quiet() {  # wait for >= 16 GB free commit and CPU <= 40 % in two samples 30 s apart (give up waiting after 2 h, logged)
  i=0
  while :; do
    a=$(load); sleep 30; b=$(load)
    ca=${a#cpu=}; ca=${ca%%%*}; cb=${b#cpu=}; cb=${cb%%%*}; fb=${b#*free=}; fb=${fb%GB}
    if [ "${ca:-100}" -le 40 ] && [ "${cb:-100}" -le 40 ] && [ "${fb:-0}" -ge 16 ]; then echo "  load ok: $a / $b" >> $L; return; fi
    i=$((i + 1)); [ $i -ge 120 ] && { echo "  load still high after 2 h: $b (running anyway)" >> $L; return; }
    sleep 30
  done
}
step() {  # name, command...
  n=$1; shift
  quiet
  echo "== $n start $(date '+%F %T') $(load)" >> $L
  t0=$(date +%s)
  "$@" >> $L 2>&1
  rc=$?
  echo "== $n rc=$rc $(( $(date +%s) - t0 )) s end $(date '+%F %T') $(load)" >> $L
  echo "[rerun] $n rc=$rc $(( $(date +%s) - t0 )) s :: $(grep -v '^==' $L | tail -1 | cut -c1-160)" >> $O/g_gates.log
  return $rc
}
step live_equiv_win32_alone env SG_BACKEND=win32 $TS -m pytest "port/test/test_live.py::test_equivalence_all" -m "" -v -p no:cacheprovider
if [ $? -ne 0 ]; then
  step cov_a_build2_live_win32 env SG_BACKEND=win32 $TS -m port.test.live equiv cov_a_build2 --backend win32 --out $O/rerun_cov_a_build2_live_win32.tsv
  step cov_a_build2_boot_win32 env SG_BACKEND=win32 $TS -m port.test.boot cov_a_build2 --out $O/rerun_cov_a_build2_boot_win32.tsv
fi
step memory_x64_alone $TS -m pytest "port/test/test_app_present.py::test_long_session_memory_bounded" -m "" -v -p no:cacheprovider
echo "[rerun done] $(date '+%F %T')" >> $O/g_gates.log
