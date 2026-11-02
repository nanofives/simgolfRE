#!/bin/sh
# P3 round 2 (edits.py with part A2): rebuild the fix mirror, the audio gates, a long run with the device, then every gate again.
# One w8_lock hold per step (builds P3b, long runs P3f, gates P3g), all sequential.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
G=log/roadmap/P3/gates.sh
sh log/roadmap/P3/buildf.sh x64 && sh log/roadmap/P3/buildf.sh Win32 || { echo "[round2] build failed"; exit 1; }
LT=_r2 sh $G t_test_audio_device_heap t_test_audio_exe
sh log/roadmap/P3/run_longf.sh fix2_dev1
sh log/roadmap/P3/run_longf.sh fix2_nodev1 -- --audio-no-device
LT=_r2 sh $G t_test_audio_names t_test_static t_test_app_audio segs_x64 segs_win32 boot_x64 t_test_app_live t_test_app_present
for k in 1 2 3 4; do LT=_r2_$k sh $G mem; done
echo "[round2 done] $(date '+%F %T')"
