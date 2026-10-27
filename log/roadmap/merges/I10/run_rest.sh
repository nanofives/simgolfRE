#!/bin/sh
# I10 second session (after the 05:25 reboot): every gate the first session had not run, one heavy step at a time, in order.
# 1. x64 suite files FROM..75 (default 34: files 1-33 are in g_suite_x64_a.log / _b.log)
# 2. test_app_live alone again (load-sensitive, S7.8 / S7.9 notes)
# 3. Win32 suite over I7's 59-file list (log/roadmap/merges/I7/win32_files.txt)
# 4. gates.sh steps, each wrapped in a machine-wide test slot (they are not pytest)
# Every line also goes to log/roadmap/merges/I10/g_rest.log; SIGPIPE is ignored so the run outlives the background shell.
trap '' PIPE
I=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I10
say() { echo "$*" >> $I/g_rest.log; echo "$*" 2>/dev/null || true; }
STEPS=${STEPS:-"x64 applive win32 gates"}
GATES=${GATES:-"segs_x64 segs_win32 boot_x64 boot_win32 live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32 chain defects census terrain"}
for st in $STEPS; do
  say "== $st start $(date '+%F %T')"
  case $st in
    x64) LOG=$I/g_suite_x64_c.log FROM=${FROM:-34} sh $I/suite_by_file.sh ;;
    applive) LOG=$I/g_suite_x64_c.log OUT=log/roadmap/merges/I10/suite_applive2 FILES=port/test/test_app_live.py sh $I/suite_by_file.sh ;;
    win32) SG_BACKEND=win32 LOG=$I/g_suite_win32.log OUT=log/roadmap/merges/I10/suite_win32 \
             FILES="$(cat /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I7/win32_files.txt)" sh $I/suite_by_file.sh ;;
    gates) for g in $GATES; do test-slot sh $I/gates.sh $g; done ;;
  esac
  say "== $st end $(date '+%F %T')"
done
say "[done] run_rest $STEPS"
