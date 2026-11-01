#!/bin/bash
# S3.26: every gate step in order, one w8_lock hold each. Args: steps (default: the full list)
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
STEPS=${*:-"build_w32 pytest_test_static pytest_test_gfx pytest_test_gfx_s3_26 pytest_test_framegate pytest_test_commands pytest_test_tick w32pytest_test_gfx w32pytest_test_gfx_s3_26 segs_x64 segs_w32 boot_x64 boot_w32 hostrows w32rows census_0 census_1 census_2 census_3 census_4"}
for s in $STEPS; do
  echo "[gate] $s start $(date +%T)"
  ~/bin/py -3.12 $S/w8_lock.py run S3.26 --what "gate $s" -- bash $S/26_gates.sh $s > $S/26_out/gate_${TREE:+${TREE}_}$s.log 2>&1
  echo "[gate] $s rc=$? $(date +%T) :: $(grep -v '^\[w8_lock\]' $S/26_out/gate_${TREE:+${TREE}_}$s.log | tail -1 | cut -c1-200)"
done
echo GATEALLDONE
