#!/bin/sh
# I15 queue 2 (orchestrator): the Win32 suite on I13's 66-file list, then the second half of the x64 suite. Detached, beside queue 1.
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I15
echo "== suite_win32 start $(date '+%F %T')" >> $G/g_gates.log
SG_BACKEND=win32 OUT=$G/suite_win32 LOG=$G/g_suite_win32.log FILES="$(cat $G/win32_files.txt)" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "== suite_x64_b start $(date '+%F %T')" >> $G/g_gates.log
OUT=$G/suite LOG=$G/g_suite_x64.log FILES="$(cat $G/x64_half_b.txt)" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "[queue2 done] $(date '+%F %T')" >> $G/g_gates.log
