#!/bin/sh
# S3 wave 10 (lead): each gate step under the S3 heavy-run lock, then one machine test slot (log/roadmap/S3/w10_gates.sh).
#   sh log/roadmap/S3/w10_run_gates.sh static t_test_boot_surfaces segs_x64 ...
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
for s in "$@"; do
  ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S3.W10 --what "w10_gates $s" -- bash -c "test-slot sh log/roadmap/S3/w10_gates.sh $s" 2>&1 | grep -v "waiting"
done
