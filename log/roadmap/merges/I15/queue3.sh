#!/bin/sh
# I15 queue 3 (orchestrator): the load-sensitive steps, alone, after queues 1 and 2: S3.12's frame times and app session, then the six
# test_app_* files one per process.
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I15
~/bin/py -3.12 $G/waitfor.py $G/g_gates.log "queue1 done" 36000 > /dev/null
~/bin/py -3.12 $G/waitfor.py $G/g_gates.log "queue2 done" 36000 > /dev/null
sh $G/gates.sh frames frames_t4 frames_ctl s3_session
echo "== suite_app start $(date '+%F %T')" >> $G/g_gates.log
OUT=$G/suite_app LOG=$G/g_suite_app.log FILES="$(cat $G/app_files.txt)" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "[queue3 done] $(date '+%F %T')" >> $G/g_gates.log
