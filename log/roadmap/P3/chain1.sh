#!/bin/sh
# P3 chain 1: baseline and one-variable variants, one w8_lock hold per run (run_long.sh)
cd /c/Users/maria/Desktop/Proyectos/SimGolf
S=log/roadmap/P3/run_long.sh
sh $S base1
sh $S audiooff1 -- --audio off
sh $S base2
sh $S threads1_1 SG_RASTER_THREADS=1
sh $S headless1 -- --terrain headless
sh $S base3
echo "[chain1 done] $(date '+%F %T')"
