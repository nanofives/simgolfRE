#!/bin/sh
# P3 round 3 (edits.py with A2 + A3): rebuild the fix mirror, the audio gates, one long run with the device. Sequential holds.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
G=log/roadmap/P3/gates.sh
sh log/roadmap/P3/buildf.sh x64 && sh log/roadmap/P3/buildf.sh Win32 || { echo "[round3] build failed"; exit 1; }
LT=_r3 sh $G t_test_audio_device_heap t_test_audio_exe
sh log/roadmap/P3/run_longf.sh fix3_dev1
echo "[round3 done] $(date '+%F %T')"
