#!/bin/sh
# I21 queue (S3 wave 8 on 2291a342), written by the orchestrator after the I21 child stalled while writing it. One heavy step
# at a time, each through log/roadmap/S3/w8_lock.py as step I21 (env.sh lock_take / lock_release). Progress in g_gates.log.
# 1. build x64 + Win32; 2. regression gates incl. census raw / split / --gl auto, terrain, gfx_check;
# 3. the full x64 run, one file per process, test_app_* last; 4. the Win32 run on I19's 72 files + test_app_audio,
# test_defects_golfsfx and wave 8's new test_framegate_split, test_gfx_s3_26.
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I21
. $G/env.sh
echo "== queue start $(date '+%F %T')" >> $G/g_gates.log
lock_take "build"; sh $G/build.sh; lock_release
echo "[build] $(tail -2 $G/build_x64.log | tr '\n' ' ') / $(tail -1 $G/build_w32.log)" >> $G/g_gates.log
sh $G/gates.sh static segs_x64 segs_win32 segs_ft_x64 boot_x64 boot_win32 boot_ft_x64 chain defects \
  live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32 gfx_check census census_split census_auto terrain
X=$(cd port/build/I21 && { ls port/test/test_*.py | grep -v test_app_; ls port/test/test_app_*.py; } | tr '\n' ' ')
W=$({ tr ' ' '\n' < log/roadmap/merges/I19/win32_files.txt; printf '%s\n' port/test/test_app_audio.py port/test/test_defects_golfsfx.py \
  port/test/test_framegate_split.py port/test/test_gfx_s3_26.py; } | grep . | sort -u | tr '\n' ' ')
echo "$W" | tr ' ' '\n' | grep . > $G/win32_files.txt
echo "== suite_x64 start $(date '+%F %T')" >> $G/g_gates.log
OUT=$G/suite LOG=$G/g_suite_x64.log FILES="$X" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "== suite_win32 start $(date '+%F %T')" >> $G/g_gates.log
SG_BACKEND=win32 OUT=$G/suite_win32 LOG=$G/g_suite_win32.log FILES="$W" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "[queue done] $(date '+%F %T')" >> $G/g_gates.log
