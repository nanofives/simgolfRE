#!/bin/sh
# P3 chain 2: one-variable variants, one w8_lock hold per run (run_long2.sh keeps audio.json too)
cd /c/Users/maria/Desktop/Proyectos/SimGolf
S=log/roadmap/P3/run_long2.sh
sh $S audiooff1 -- --audio off
sh $S audiooff2 -- --audio off
sh $S base2
sh $S threads1_1 SG_RASTER_THREADS=1
sh $S headless1 -- --terrain headless
echo "[chain2 done] $(date '+%F %T')"
