#!/bin/sh
# I24 resumed after the 2026-11-02 17:02 reboot: the queue died in segs_x64 (34/205); static 811/2 and both builds had passed.
# The gate steps run their own test-slot wrappers (gates.sh $TS, suite_by_file.sh pytest).
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I24
echo "== resume after reboot $(date '+%F %T')" >> $G/g_gates.log
sh $G/gates.sh segs_x64 segs_win32 segs_ft_x64 boot_x64 boot_win32 boot_ft_x64 chain defects \
  live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32 gfx_check census census_split census_auto terrain
echo "[gates done] $(date '+%F %T')" >> $G/g_gates.log
sh $G/queue_suites.sh
