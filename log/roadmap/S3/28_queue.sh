#!/bin/bash
# S3.28: after the running S3.28 hold ends, the recording run, then the rest of the census (one S3.28 take at a time).
cd /c/Users/maria/Desktop/Proyectos/SimGolf
PY=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
while ~/bin/py -3.12 log/roadmap/S3/w8_lock.py status 2>&1 | grep -q "step=S3.28"; do sleep 20; done
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S3.28 --what "record s328_s2 (game boot, instance s3_w8)" -- \
  $PY log/roadmap/S3/28_gl_record.py --scenario cov_r_s2_screens --instance s3_w8 --label s328_s2 --inject-at course > log/roadmap/S3/28_out/rec_s328_s2.log 2>&1
echo "record rc=$? $(tail -n 1 log/roadmap/S3/28_out/rec_s328_s2.log | cut -c1-200)"
bash log/roadmap/S3/28_census.sh
