#!/bin/sh
# S3.32: each gate step under the S3 heavy-run lock, then one machine test slot (log/roadmap/S3/32_gates.sh).
#   sh log/roadmap/S3/32_run_gates.sh static t_test_boot_surfaces segs_x64 ...
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
for s in "$@"; do
  ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S3.32 --what "32_gates $s" -- bash -c "test-slot sh log/roadmap/S3/32_gates.sh $s" 2>&1 | grep -v "waiting"
done
