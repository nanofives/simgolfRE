#!/bin/sh
# S7 lead: test_app_present's long session (course_season_6y live trace) with --audio off and --audio on, private bytes after
# frame 2,000 compared (test_long_session_memory_bounded read 4,427,776 B against its 4 MB limit with the default --audio on).
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/S7_11_root
EXE=$M/port/build/x64/app/Release/simgolf_port.exe
O=$R/log/roadmap/S7/11_lead_mem
mkdir -p $O
cd $M || exit 1
for a in off on; do
  rm -rf $O/$a; mkdir -p $O/$a
  t0=$(date +%s)
  $EXE --install original --data $O/$a/data --record-dir $O/$a/rec --live-trace $M/port/corpus/traces/course_season_6y.trace \
    --mem-every 1000 --audio $a > $O/$a/stdout.txt 2>&1
  echo "[$a] rc=$? $(( $(date +%s) - t0 )) s" | tee -a $O/summary.txt
done
