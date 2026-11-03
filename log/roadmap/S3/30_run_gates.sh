#!/bin/sh
# S3.30: each gate step under the S3 heavy-run lock, then one machine test slot (log/roadmap/S3/30_gates.sh).
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
for s in "$@"; do
  ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S3.30 --what "30_gates $s" -- bash -c "test-slot sh log/roadmap/S3/30_gates.sh $s" 2>&1 | grep -v "waiting"
done
