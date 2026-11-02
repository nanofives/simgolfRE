#!/bin/sh
# I22: resume the Win32 suite after the 2026-11-01 14:23 reboot (stopped during file 50, test_live). Same list and settings
# as queue_suites.sh's Win32 pass; FROM=50. Launched under test-slot (nested pytest sees PROYECTOS_TEST_SLOT=held).
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I22
W=$(cat $G/win32_files.txt | tr -d '\r' | grep . | tr '\n' ' ')
echo "== suite_win32 resume FROM=50 $(date '+%F %T') (after reboot)" >> $G/g_gates.log
SG_BACKEND=win32 OUT=$G/suite_win32 LOG=$G/g_suite_win32.log FILES="$W" FROM=50 sh $G/suite_by_file.sh
echo "[queue done] $(date '+%F %T')" >> $G/g_gates.log
