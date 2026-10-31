#!/bin/sh
# S7 lead: test_app_present's long session on HEAD's own app (port/build/I15 = 37e58e96's merged tree, no S7 wave 4 code),
# measured like test_long_session_memory_bounded (private bytes, first sample from frame 2,000 to the end). Waits for the
# final pass (12_g_gates.log `[done]`) so only one heavy run of S7 runs at a time.
R=/c/Users/maria/Desktop/Proyectos/SimGolf
O=$R/log/roadmap/S7/12_lead_mem_head
until grep -q "^\[done\]" $R/log/roadmap/S7/12_g_gates.log 2>/dev/null; do sleep 60; done
mkdir -p $O
M=$R/port/build/I15
cd $M || exit 1
EXE=$M/port/build/x64/app/Release/simgolf_port.exe
for k in 1 2; do
  rm -rf $O/r$k; mkdir -p $O/r$k
  t0=$(date +%s)
  $EXE --install original --data $O/r$k/data --record-dir $O/r$k/rec \
    --live-trace $R/port/corpus/traces/course_season_6y.trace --mem-every 1000 > $O/r$k/stdout.txt 2>&1
  echo "[head r$k] rc=$? $(( $(date +%s) - t0 )) s" >> $O/summary.txt
done
