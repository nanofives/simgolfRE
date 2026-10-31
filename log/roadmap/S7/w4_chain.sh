#!/bin/sh
# S7 lead: after the final pass (12_g_gates.log [done]) and the HEAD memory runs (12_lead_mem_head/summary.txt r2), build
# port/build/S7_w4_root (I19 tree + S7 wave 4, w4_mirror.py) x64 and Win32 (runner included), then the gate on it.
R=/c/Users/maria/Desktop/Proyectos/SimGolf
O=$R/log/roadmap/S7
until grep -q "head r2" $O/12_lead_mem_head/summary.txt 2>/dev/null; do sleep 60; done
cd $R
~/bin/py -3.12 $O/w4_mirror.py --build x64 > $O/w4_build_x64.log 2>&1 || { echo "[build x64 FAILED]" >> $O/w4_g_gates.log; exit 1; }
~/bin/py -3.12 $O/w4_mirror.py --build Win32 --target portsim_c --target portsim_runner32 --target simgolf_port \
  --target simgolf_audio_check > $O/w4_build_w32.log 2>&1 || { echo "[build Win32 FAILED]" >> $O/w4_g_gates.log; exit 1; }
S=""
for f in ingame input live present replay shell audio; do S="$S app_${f}_x64 app_${f}_win32"; done
sh $O/w4_gates.sh defects_x64 $S segs_x64 segs_win32 segs0_x64 segs0_win32 boot_x64 boot_win32 boot0_x64 boot0_win32 \
  replay_x64 replay_win32
