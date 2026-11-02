#!/bin/sh
# P3: build the x64 port + app in the P3 mirror (port/build/P3 = git archive e9f59779 + junctions + P3 measurement lines),
# one w8_lock hold. Usage: sh log/roadmap/P3/build.sh [x64|Win32]
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/P3
O=$R/log/roadmap/P3
ARCH=${1:-x64}
LOCKPY="$HOME/bin/py -3.12 $R/log/roadmap/S3/w8_lock.py"
$LOCKPY take P3 --what "P3 build $ARCH" --need-gb 8 || exit 3
cd $M || { $LOCKPY release P3; exit 1; }
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
~/bin/py -3.12 port/test/build.py --arch $ARCH > $O/build_$ARCH.log 2>&1
rc=$?
$LOCKPY release P3
echo "build $ARCH rc=$rc $(date '+%T')" | tee -a $O/build_$ARCH.log
exit $rc
