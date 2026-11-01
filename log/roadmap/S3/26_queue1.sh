#!/bin/bash
# S3.26 diagnostics, one heavy hold each (after the x64 build)
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
PY=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
until grep -q "rc=" /c/Users/maria/AppData/Local/Temp/claude/C--Users-maria-Desktop-Proyectos-SimGolf/bcfc2fce-ddb9-4710-bd07-4e598fffa79e/tasks/bwjfj10zp.output; do sleep 10; done
ls $S/../../../port/build/S3_26/port/build/x64/Release/portsim_c.dll || exit 1
bash $S/26_snaps.sh cov_b_inject golfer_stories
echo "[tlive] start $(date +%T)"
~/bin/py -3.12 $S/w8_lock.py run S3.26 --what "terrainlive cov_r_s2_screens" -- bash $S/26_run.sh $PY -m port.test.terrainlive cov_r_s2_screens > $S/26_tlive_s2.log 2>&1
echo "[tlive] rc=$?"; grep -v "^\[w8_lock\]" $S/26_tlive_s2.log | tail -20
bash $S/26_snaps.sh cov_r_s2_screens cov_k3_camera2
echo QUEUE1DONE
