#!/bin/sh
# S3.25: the remaining recordings, one heavy-lock hold each (25_heavy.py), in turn.
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
export SIMGOLF_ALLOW_MULTI=1
for job in "queue cov_b_holes18 -" "queue cov_k3_camera2 -" "inject cov_b_inject2 -" "queue golfer_stories _box"; do
  set -- $job
  suf=""; [ "$3" != "-" ] && suf="$3"
  ~/bin/py -3.12 -u log/roadmap/S3/25_heavy.py "rec $1 $2$suf" -- ~/bin/py -3.12 -u log/roadmap/S3/25_rec.py "$1" "$2" --instance s3_w8 \
    --suffix "$suf" > "log/roadmap/S3/25_run_$1_$2$suf.log" 2>&1
  echo "done $1 $2$suf rc $?"
done
