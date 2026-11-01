#!/bin/sh
# I21: re-run census_auto for cov_r_s2_screens alone (its 32 rows were NOT_RUN in the full run: boot -11 at record 0, the
# gditext stale-font transient P2 fixes). Orchestrator, 2026-10-31. First version called test-slot through w8_lock.py run,
# which cannot start a bash script (FileNotFoundError) and skipped env.sh; this one sources env.sh like gates.sh and holds
# the S3 lock as its own step I21r (step I21 belongs to the running suite loop).
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I21 || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I21/env.sh
O=$R/log/roadmap/merges/I21
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
$LOCKPY take I21r --priority --what "census_auto cov_r_s2_screens (I21 transient re-run)" --need-gb ${MINFREE:-16} > /dev/null 2>&1
echo "start $(date '+%F %T')" > $O/census_auto_s2_rerun.log
$TS -m port.test.framegate cov_r_s2_screens --jobs 1 --no-probe --gl auto --out $O/census_auto_s2_rerun.tsv >> $O/census_auto_s2_rerun.log 2>&1
rc=$?
$LOCKPY release I21r > /dev/null 2>&1
echo "rc=$rc $(date '+%F %T')" >> $O/census_auto_s2_rerun.log
