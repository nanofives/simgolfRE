#!/bin/sh
# P3: test_app_audio's dsound shutdown test on the UNFIXED build (port/build/P3 = e9f59779 + measurement lines), x64, one hold
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/P3f
LOCKPY="$HOME/bin/py -3.12 /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3/w8_lock.py"
$LOCKPY take P3h --what "P3 base shutdown test" --need-gb 8 || exit 3
SG_AUDIO_CHECK_X64=/c/Users/maria/Desktop/Proyectos/SimGolf/port/build/P3/port/build/x64/app/Release/simgolf_audio_check.exe \
  test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe -m pytest port/test/test_app_audio.py \
  -k "test_shutdown_leaves_nothing and x64" -m "" -q -p no:cacheprovider
rc=$?
$LOCKPY release P3h
exit $rc
