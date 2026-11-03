#!/bin/sh
# I25 fast gates (S11 wave 2 on cdeb2609), orchestrator, 2026-11-03. Mariano skipped the full suites, census and slow suite for this
# merge; these are the gates of log/roadmap/S11/MERGE_w2.md section D plus B3's golden files, one heavy step at a time (w8_lock.py, I25).
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I25
. $G/env.sh
echo "== queue start $(date '+%F %T')" >> $G/g_gates.log
lock_take "build"; sh $G/build.sh; lock_release
echo "[build] $(tail -2 $G/build_x64.log | tr '\n' ' ') / $(tail -1 $G/build_w32.log)" >> $G/g_gates.log
sh $G/gates.sh static segs_x64 segs_win32 boot_x64 boot_win32 app_replay_x64 gfx_check \
  t_test_static t_test_golfers \
  t_test_saveload t_test_course_menu t_test_browser t_test_save_dialogs t_test_loaders t_test_menu t_test_shotstyle t_test_gfx_info_panel \
  t_test_tick
echo "[queue done] $(date '+%F %T')" >> $G/g_gates.log
