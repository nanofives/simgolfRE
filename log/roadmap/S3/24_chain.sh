#!/bin/bash
# S3.24: the gate steps one turn each (24_turn.sh), 130 s apart; logs log/roadmap/S3/24_g_<step>.log
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
cd /c/Users/maria/Desktop/Proyectos/SimGolf
until grep -q "\[turn\] end" $S/24_rows_r1.log 2>/dev/null; do sleep 20; done
for step in "$@"; do
  [ -n "$started" ] && sleep 130; started=1
  echo "$(date '+%H:%M:%S') step $step" >> $S/24_chain.log
  bash $S/24_turn.sh "gate $step" bash $S/24_gates.sh $step > $S/24_g_$step.log 2>&1
  echo "$(date '+%H:%M:%S') step $step rc=$?" >> $S/24_chain.log
done
echo "$(date '+%H:%M:%S') chain done" >> $S/24_chain.log
