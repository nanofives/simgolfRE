#!/bin/sh
# S7 lead re-runs after the first heavy pass (log/roadmap/S7/11_g_gates.log): app_ingame x64 / Win32 (the mirror lacked
# re/analysis until junctions were added), live equivalence Win32 (14 PASS then 56 ERROR with no stderr), then the
# long-session memory A/B (--audio off / on).
R=/c/Users/maria/Desktop/Proyectos/SimGolf
O=$R/log/roadmap/S7
cp $O/11_g_gates.log $O/11_g_gates_pass1.log
sh $O/11_gates.sh app_ingame_x64 app_ingame_win32 equiv_win32
sh $O/11_lead_mem_ab.sh
echo "[lead rerun done]" >> $O/11_g_gates.log
