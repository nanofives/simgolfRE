#!/bin/sh
# I24 suites (S3 wave 10 + P3 on c316a3a6), orchestrator, 2026-11-01. Waits for queue_gates.sh's "[gates done]", then the full x64 suite
# (one file per process, test_app_* last) and the Win32 suite on I22's file list + S3.28's test_terrain_s3_28.py, one S3 lock hold
# per file (suite_by_file.sh). G20 (the suites' scope) had no answer when the gates ended, so the full suites run (the default).
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I24
while ! grep -q '^\[gates done\]' $G/g_gates.log; do sleep 60; done
. $G/env.sh
X=$(cd port/build/I24 && { ls port/test/test_*.py | grep -v test_app_; ls port/test/test_app_*.py; } | tr '\n' ' ')
W=$({ cat log/roadmap/merges/I22/win32_files.txt; printf "%s
" port/test/test_terrain_s3_28.py port/test/test_trace_call.py port/test/test_boot_surfaces.py port/test/test_audio_device_heap.py; } | tr -d '\r' | grep . | sort -u | tr '\n' ' ')
echo "$W" | tr ' ' '\n' | grep . > $G/win32_files.txt
echo "== suite_x64 start $(date '+%F %T')" >> $G/g_gates.log
OUT=$G/suite LOG=$G/g_suite_x64.log FILES="$X" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "== suite_win32 start $(date '+%F %T')" >> $G/g_gates.log
SG_BACKEND=win32 OUT=$G/suite_win32 LOG=$G/g_suite_win32.log FILES="$W" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "[queue done] $(date '+%F %T')" >> $G/g_gates.log
