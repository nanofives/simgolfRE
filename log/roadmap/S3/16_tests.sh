# S3.16: the port test files the step's edits reach, one pytest process per file, on the mirror's x64 build.
#   cd port/build/S3_16 && sh ../../../log/roadmap/S3/16_tests.sh [files...]   (log: log/roadmap/S3/16_tests.log)
set -u
. ../../../log/roadmap/S3/14_env.sh
FILES=${*:-test_static test_dialogs test_commands test_shots test_gfx test_framegate test_gfx_eoi test_gfx_hud test_gfx_s2_10 test_gfx_scene test_gfx_text test_terrain_overlay test_gfx_dialogs test_gfx_dialogs2 test_gfx_reports test_tick}
n=$(echo $FILES | wc -w); k=0
for f in $FILES; do
  k=$((k+1))
  out=$(~/bin/py -3.12 -m pytest -q -p no:cacheprovider port/test/$f.py 2>&1 | tail -1)
  echo "[$k/$n] $f $out"
done
