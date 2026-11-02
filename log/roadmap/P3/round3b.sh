#!/bin/sh
# P3 round 3b: every gate on the round-3 fix build (edits.py A + A2 + A3 + B + C). Sequential P3g holds.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
G=log/roadmap/P3/gates.sh
LT=_r3 sh $G t_test_audio_names t_test_static t_test_app_audio segs_x64 segs_win32 boot_x64 t_test_app_live t_test_app_present
for k in 1 2 3 4; do LT=_r3_$k sh $G mem; done
echo "[round3b done] $(date '+%F %T')"
