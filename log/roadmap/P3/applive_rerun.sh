#!/bin/sh
# P3: test_app_live.py rerun on the round-3 fix build (round 3b: test_presents_replay 511 vs 510 refreshFull calls), one P3h hold
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/P3f
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/P3/env.sh
LOCKPY="$HOME/bin/py -3.12 /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3/w8_lock.py"
$LOCKPY take P3h --what "P3 app_live rerun r3" --need-gb 8 || exit 3
test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe -m pytest port/test/test_app_live.py -m "" -q -p no:cacheprovider
rc=$?
$LOCKPY release P3h
echo "rc=$rc"
