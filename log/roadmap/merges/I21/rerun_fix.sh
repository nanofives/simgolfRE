#!/bin/sh
# I21: re-run the two files whose stale wave 8 pins log/roadmap/S3/w8_fix_i21.py updated (applied to this mirror 2026-10-31),
# on x64, one S3 lock hold per file as step I21r (step I21 belongs to the running suite loop). Same pytest call as suite_by_file.sh.
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I21 || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I21/env.sh
O=$R/log/roadmap/merges/I21
mkdir -p $O/suite_fix
for b in test_gfx_pixel test_gfx_reports; do
  $LOCKPY take I21r --priority --what "fix re-run $b" --need-gb ${MINFREE:-16} > /dev/null 2>&1
  ~/bin/py -3.12 -m pytest port/test/$b.py -m "" -q -p no:cacheprovider > $O/suite_fix/$b.txt 2>&1
  rc=$?
  $LOCKPY release I21r > /dev/null 2>&1
  echo "$b rc=$rc $(tail -1 $O/suite_fix/$b.txt) $(date '+%T')" >> $O/suite_fix/summary.txt
done
