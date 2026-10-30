#!/bin/sh
# I18 single queue (one heavy run at a time, S3.9 holds another slot): gates, then the x64 suite (all port/test files, app
# files last), then the Win32 suite on I13's 66-file list. Detached; progress in g_gates.log / g_suite_*.log.
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I18
~/bin/py -3.12 $G/waitfor.py $G/build_x64.log "build. done" 7200 > /dev/null
sh $G/gates.sh net_win32 static segs_x64 segs_win32 segs_ft_x64 boot_x64 boot_win32 boot_ft_x64 live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32 census
X=$(cd port/build/I18 && ls port/test/test_*.py | grep -v test_app_; ls port/build/I18/port/test/test_app_*.py | sed 's#port/build/I18/##')
echo "== suite_x64 start $(date '+%F %T')" >> $G/g_gates.log
OUT=$G/suite LOG=$G/g_suite_x64.log FILES="$X" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "== suite_win32 start $(date '+%F %T')" >> $G/g_gates.log
SG_BACKEND=win32 OUT=$G/suite_win32 LOG=$G/g_suite_win32.log FILES="$(cat $G/win32_files.txt)" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "[queue done] $(date '+%F %T')" >> $G/g_gates.log
