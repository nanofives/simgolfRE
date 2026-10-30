#!/bin/sh
# I15 queue 4 (orchestrator): re-run s3_session after queue 3 (its first run failed on session.py's repo-root path,
# parents[3] -> parents[4]); alone, after the app files.
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I15
~/bin/py -3.12 $G/waitfor.py $G/g_gates.log "queue3 done" 36000 > /dev/null
LT=_r2 sh $G/gates.sh s3_session
echo "[queue4 done] $(date '+%F %T')" >> $G/g_gates.log
