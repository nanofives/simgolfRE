#!/bin/sh
# assemble the module, mirror + build x64, print only audio_sound diagnostics
cd "$(dirname "$0")/../../.." || exit 1
~/bin/py -3.12 log/roadmap/S4/7_assemble.py || exit 1
~/bin/py -3.12 log/roadmap/S4/7_mirror.py "$@" > log/roadmap/S4/7_mirror_build.log 2>&1
rc=$?
grep -o "audio_sound.cpp([0-9,]*): \(error\|warning\) C[0-9]*: [^[]*" log/roadmap/S4/7_mirror_build.log | sort -u | cut -c1-220 | head -60
grep -o "[a-z_]*\.h([0-9,]*): error [^[]*" log/roadmap/S4/7_mirror_build.log | sort -u | head
echo "rc=$rc"; tail -3 log/roadmap/S4/7_mirror_build.log
