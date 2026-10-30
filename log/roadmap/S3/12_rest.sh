#!/bin/sh
# S3.12: everything after chain1, in order: chain2 (static, app replay, live equivalence), the x64 suite one file per process,
# the Win32 files (I7's list + test_gl_raster / test_terrain_state). Restart a part with STEP=<chain2|x64|win32> FROM=<n>.
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
O=$R/log/roadmap/S3/12_out/S3_12
until grep -q "^\[done\]" $O/chain1.log; do sleep 30; done
case ${STEP:-chain2} in
  chain2) sh $R/log/roadmap/S3/12_gates.sh static app_replay_x64 app_replay_win32 live_equiv_x64 > $O/chain2.log 2>&1 ;&
  x64) LOG=$O/suite_x64_run.log sh $R/log/roadmap/S3/12_suite_by_file.sh > /dev/null 2>&1; FROM= ;&
  win32) W=$(cat $R/log/roadmap/merges/I7/win32_files.txt | tr '\n' ' ') ;
         SG_BACKEND=win32 FILES="$W port/test/test_gl_raster.py port/test/test_terrain_state.py" LOG=$O/suite_win32_run.log \
           sh $R/log/roadmap/S3/12_suite_by_file.sh > /dev/null 2>&1 ;;
esac
echo "[rest done]" >> $O/suite_win32_run.log
