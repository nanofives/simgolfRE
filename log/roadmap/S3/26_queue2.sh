#!/bin/bash
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
PY=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
~/bin/py -3.12 $S/w8_lock.py run S3.26 --what "build x64 (2)" -- $PY $S/24_build.py x64 --tree S3_26 > $S/26_build_x64.log 2>&1
echo "[build] rc=$?"; grep -E "error C|error LNK|: error" $S/26_build_x64.log | head -5
TAG=host2 bash $S/26_snaps.sh golfer_stories cov_k3_camera2 cov_b_holes18 cov_k1_paths cov_k1_river cov_a_flags cov_a_keys3 cov_k1_routing cov_l_bridge
echo QUEUE2DONE
