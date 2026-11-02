#!/bin/sh
# P3 chain 3: the I21 / I22 condition (no audio device: --audio-no-device, the shadow takes initSound's failure path) three
# times, then that condition with one more variable each; one w8_lock hold per run (run_long2.sh)
cd /c/Users/maria/Desktop/Proyectos/SimGolf
S=log/roadmap/P3/run_long2.sh
sh $S nodev1 -- --audio-no-device
sh $S nodev2 -- --audio-no-device
sh $S nodev3 -- --audio-no-device
sh $S nodev_threads1 SG_RASTER_THREADS=1 -- --audio-no-device
sh $S nodev_headless -- --audio-no-device --terrain headless
echo "[chain3 done] $(date '+%F %T')"
