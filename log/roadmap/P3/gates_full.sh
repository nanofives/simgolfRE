#!/bin/sh
# P3: the heavy gates of the fix, after the quick ones (gates_quick: [5/5] in g_gates.log); one P3g hold per step
cd /c/Users/maria/Desktop/Proyectos/SimGolf
until grep -q "^\[5/5\]" log/roadmap/P3/gates/g_gates.log 2>/dev/null; do sleep 30; done
sh log/roadmap/P3/gates.sh segs_x64 segs_win32 boot_x64 t_test_app_live t_test_app_present
for k in 1 2 3 4; do LT=_$k sh log/roadmap/P3/gates.sh mem; done
echo "[gates_full done] $(date '+%F %T')"
