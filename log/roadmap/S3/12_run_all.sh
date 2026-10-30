#!/bin/bash
# S3.12 on the e179bd24 base: the whole gate in order, one heavy run at a time, each part's progress in its own log under
# log/roadmap/S3/12_out/S3_12/ (the reference is plain e179bd24 = port/build/S3_12_e179). Restart from a part with
# STEP=<timing|chain1|chain2|x64|win32|pairs> (FROM=<n> for a suite part).
#   timing  3 interleaved app sessions, plain e179bd24 then wave 6 (12_session.py; 12_session_timing_e179.log)
#   chain1  frames frames_t4 frames_ctl equiv segs_x64 segs_win32 boot_x64 boot_win32 terrain census
#   chain2  static app_replay_x64 app_replay_win32 live_equiv_x64
#   x64     the port suite one file per process;   win32  I7's 59 files + test_gl_raster / test_terrain_state on the Win32 runner
#   pairs   test_app_ingame, 2 pairs plain e179bd24 / wave 6
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
O=$R/log/roadmap/S3/12_out/S3_12
mkdir -p $O
case ${STEP:-timing} in
  timing) (cd $R && for k in 1 2 3; do for t in e179 12; do echo "=== $t r$k $(date '+%T')"; ~/bin/py -3.12 log/roadmap/S3/12_session.py $t e179_${t}_r$k --runs 1 2>&1; done; done) > $R/log/roadmap/S3/12_session_timing_e179.log 2>&1 ;&
  chain1) sh $R/log/roadmap/S3/12_gates.sh frames frames_t4 frames_ctl equiv segs_x64 segs_win32 boot_x64 boot_win32 terrain census > $O/chain1.log 2>&1 ;&
  chain2) sh $R/log/roadmap/S3/12_gates.sh static app_replay_x64 app_replay_win32 live_equiv_x64 > $O/chain2.log 2>&1 ;&
  x64) LOG=$O/suite_x64_run.log sh $R/log/roadmap/S3/12_suite_by_file.sh > /dev/null 2>&1; FROM= ;&
  win32) W=$(cat $R/log/roadmap/merges/I7/win32_files.txt | tr '\n' ' ')
         SG_BACKEND=win32 FILES="$W port/test/test_gl_raster.py port/test/test_terrain_state.py" LOG=$O/suite_win32_run.log \
           sh $R/log/roadmap/S3/12_suite_by_file.sh > /dev/null 2>&1 ;&
  pairs) sh $R/log/roadmap/S3/12_app_ingame_pairs.sh ;;
esac
echo "[all done] $(date '+%F %T')" >> $O/run_all.log
