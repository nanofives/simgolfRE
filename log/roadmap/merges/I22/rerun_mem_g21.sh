#!/bin/sh
# I22: re-run test_app_present::test_long_session_memory_bounded at the G21 limit (the known load-dependent test: 4.28 MB in the suite against
# its 4 MB limit; I19 4.30 MB in the suite, 3.34 MB alone). One S3 lock hold as step I22r. Orchestrator, 2026-10-31.
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I22 || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I22/env.sh
O=$R/log/roadmap/merges/I22
mkdir -p $O/suite_fix
$LOCKPY take I22r --priority --what "test_long_session_memory_bounded at the G21 limit" --need-gb ${MINFREE:-16} > /dev/null 2>&1
SG_APP_EXE=$SG_APP_EXE ~/bin/py -3.12 -m pytest port/test/test_app_present.py -k test_long_session_memory_bounded -m "" -q -p no:cacheprovider > $O/suite_fix/mem_g21.txt 2>&1
rc=$?
$LOCKPY release I22r > /dev/null 2>&1
echo "mem_g21 rc=$rc $(tail -1 $O/suite_fix/mem_g21.txt) $(date '+%T')" >> $O/suite_fix/summary.txt
