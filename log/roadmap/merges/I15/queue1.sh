#!/bin/sh
# I15 queue 1 (orchestrator, 2026-10-29): every regression gate, then the first half of the x64 suite (x64_half_a.txt). Detached.
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I15
sh $G/gates.sh static segs_x64 segs_win32 segs_ft_x64 boot_x64 boot_win32 boot_ft_x64 chain defects census terrain live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32 gfx_check s3_raster equiv s3_ctl_live
echo "== suite_x64_a start $(date '+%F %T')" >> $G/g_gates.log
OUT=$G/suite LOG=$G/g_suite_x64.log FILES="$(cat $G/x64_half_a.txt)" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "[queue1 done] $(date '+%F %T')" >> $G/g_gates.log
