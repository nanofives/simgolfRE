#!/bin/bash
# S3.28: framegate (raw only) on the scenarios whose terrain recordings call lowerEdgeCorner, one S3.28 hold, with the per-pass log.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
T=log/roadmap/S3
TREE=port/build/S3_28
export SG_PORTSIM_DLL=$PWD/$TREE/port/build/x64/Release/portsim_c.dll
SCEN="course_season cov_b_holes18 cov_b_paths cov_k1_terrain cov_l_bridge cov_r_s2_screens"
rm -f $T/28_out/passlog_*.txt
/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe $T/w8_lock.py run S3.28 --what "S3.28 pass log ($SCEN)" -- bash -c "cd $TREE && for s in $SCEN; do \
  SG_TERRAIN_PASS_LOG=\$(pwd -W)/../../../$T/28_out/passlog_\$s.txt ~/bin/py -3.12 -m port.test.framegate \$s --jobs 1 --out ../../../$T/28_out/passlog_fg_\$s.tsv; done" \
  > $T/28_out/passlog.log 2>&1
echo "passlog rc=$?"
~/bin/py -3.12 $T/28_passlog.py
