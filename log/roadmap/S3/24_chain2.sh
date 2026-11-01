#!/bin/bash
# S3.24 gaps after the lead's review: one w8_lock.py run per step; logs 24_g_<step>.log, chain log 24_chain2.log
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
cd /c/Users/maria/Desktop/Proyectos/SimGolf
for step in "$@"; do
  echo "$(date '+%H:%M:%S') step $step" >> $S/24_chain2.log
  ~/bin/py -3.12 $S/w8_lock.py run S3.24 --what "gate $step" -- bash $S/24_gates.sh $step > $S/24_g_$step.log 2>&1
  echo "$(date '+%H:%M:%S') step $step rc=$?" >> $S/24_chain2.log
done
echo "$(date '+%H:%M:%S') chain done" >> $S/24_chain2.log
