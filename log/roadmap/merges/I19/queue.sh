#!/bin/sh
# I19 single queue (one heavy run at a time; S7's lead holds another slot): the section's light gates first, then the regression
# gates, then the x64 suite (every port/test file, test_app_* last), then the Win32 suite on win32_files.txt (I18's 66 + wave 7's 6).
# Detached; progress in g_gates.log / g_suite_*.log.
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I19
echo "== queue start $(date '+%F %T')" >> $G/g_gates.log
sh $G/gates.sh gfx_check t_test_gl_wgl_selfcheck t_test_gl_wgl w32_test_gl_wgl_selfcheck w32_test_gl_wgl ctl13 net_win32 \
  static segs_x64 segs_win32 segs_ft_x64 boot_x64 boot_win32 boot_ft_x64 chain defects census census_auto terrain \
  live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32
X=$(cd port/build/I19 && { ls port/test/test_*.py | grep -v test_app_; ls port/test/test_app_*.py; })
echo "== suite_x64 start $(date '+%F %T')" >> $G/g_gates.log
OUT=$G/suite LOG=$G/g_suite_x64.log FILES="$X" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "== suite_win32 start $(date '+%F %T')" >> $G/g_gates.log
SG_BACKEND=win32 OUT=$G/suite_win32 LOG=$G/g_suite_win32.log FILES="$(cat $G/win32_files.txt)" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "[queue done] $(date '+%F %T')" >> $G/g_gates.log
