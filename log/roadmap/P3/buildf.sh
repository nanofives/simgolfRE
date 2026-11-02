#!/bin/sh
# P3: build the fix mirror port/build/P3f (git archive e9f59779 + junctions + log/roadmap/P3/edits.py), one w8_lock hold
# (step P3b, so its wait file is apart from the measurement runs' P3). Usage: sh log/roadmap/P3/buildf.sh [x64|Win32]
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/P3f
O=$R/log/roadmap/P3
ARCH=${1:-x64}
LOCKPY="$HOME/bin/py -3.12 $R/log/roadmap/S3/w8_lock.py"
$LOCKPY take P3b --what "P3f build $ARCH" --need-gb 8 || exit 3
cd $M || { $LOCKPY release P3b; exit 1; }
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
~/bin/py -3.12 port/test/build.py --arch $ARCH > $O/buildf_$ARCH.log 2>&1
rc=$?
$LOCKPY release P3b
echo "build $ARCH rc=$rc $(date '+%T')" | tee -a $O/buildf_$ARCH.log
exit $rc
