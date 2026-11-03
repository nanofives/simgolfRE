#!/bin/sh
# I24 gates queue (S3 wave 10 + P3 on c316a3a6), orchestrator. One heavy step at a time through log/roadmap/S3/w8_lock.py as step I24.
# 1. build x64 + Win32; 2. the 18 regression gates (as I22). The suites run from queue_suites.sh after the scope is settled.
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I24
. $G/env.sh
echo "== queue start $(date '+%F %T')" >> $G/g_gates.log
lock_take "build"; sh $G/build.sh; lock_release
echo "[build] $(tail -2 $G/build_x64.log | tr '\n' ' ') / $(tail -1 $G/build_w32.log)" >> $G/g_gates.log
sh $G/gates.sh static segs_x64 segs_win32 segs_ft_x64 boot_x64 boot_win32 boot_ft_x64 chain defects \
  live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32 gfx_check census census_split census_auto terrain
echo "[gates done] $(date '+%F %T')" >> $G/g_gates.log
