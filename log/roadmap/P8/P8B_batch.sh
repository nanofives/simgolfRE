#!/bin/bash
# P8B measurement batch (run from port/build/P8B under w8_lock + test-slot): screenshots and pass times.
# Usage: bash log/roadmap/P8/P8B_batch.sh shots|times
set -u
O=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/P8/P8B_runs
LT="$HOME/bin/py -3.12 log/roadmap/P8/P8B_lt.py"
COMMON="--audio off --profile fixed"
case "$1" in
shots)
  # sandbox_basic: present 447 = the last in-game present (presents.tsv of sb_off), 300 an earlier in-game one
  $LT sandbox_basic $O/shot_sb_wide1080 -- $COMMON --wide --render-scale 1920x1080 --world-shots 300,447 --shot-size 1920x1080 --scale smooth
  $LT sandbox_basic $O/shot_sb_wide1440 -- $COMMON --wide --render-scale 2560x1440 --world-shots 300,447 --shot-size 2560x1440 --scale smooth
  $LT sandbox_basic $O/shot_sb_43_1080 -- $COMMON --render-scale 1440x1080 --world-shots 300,447 --shot-size 1920x1080 --scale smooth
  $LT sandbox_basic $O/shot_sb_43_1440 -- $COMMON --render-scale 1920x1440 --world-shots 300,447 --shot-size 2560x1440 --scale smooth
  $LT sandbox_basic $O/shot_sb_wide1_check -- $COMMON --wide --render-scale 1 --wide-width 1067 --world-check --world-shots 447 --shot-size 1067x600 --scale nearest
  ;;
cs_shots)
  S="$2"
  $LT course_season $O/shot_cs_wide1080 -- $COMMON --wide --render-scale 1920x1080 --world-shots $S --shot-size 1920x1080 --scale smooth
  $LT course_season $O/shot_cs_wide1440 -- $COMMON --wide --render-scale 2560x1440 --world-shots $S --shot-size 2560x1440 --scale smooth
  $LT course_season $O/shot_cs_43_1080 -- $COMMON --render-scale 1440x1080 --world-shots $S --shot-size 1920x1080 --scale smooth
  ;;
times)
  for sz in 1067x600 1920x1080 2560x1440; do
    for th in default 1; do
      d=$O/time_cs_${sz}_t${th}
      rm -f $d.world.log $d.view.log
      if [ "$th" = 1 ]; then export SG_RASTER_THREADS=1; else unset SG_RASTER_THREADS; fi
      SG_WORLD_PASS_LOG=$d.world.log SG_TERRAIN_PASS_LOG=$d.view.log $LT course_season $d -- $COMMON --wide --render-scale $sz --world-check
    done
  done
  unset SG_RASTER_THREADS
  d=$O/time_cs_off
  rm -f $d.view.log
  SG_TERRAIN_PASS_LOG=$d.view.log $LT course_season $d -- $COMMON
  ;;
esac
# camera moves (strip passes): bash log/roadmap/P8/P8B_batch.sh cam
if [ "$1" = cam ]; then
  for sz in 1067x600 1920x1080 2560x1440; do
    for th in default 1; do
      d=$O/time_cam_${sz}_t${th}
      rm -f $d.world.log $d.view.log
      if [ "$th" = 1 ]; then export SG_RASTER_THREADS=1; else unset SG_RASTER_THREADS; fi
      SG_WORLD_PASS_LOG=$d.world.log SG_TERRAIN_PASS_LOG=$d.view.log $LT cov_l_camera $d -- $COMMON --wide --render-scale $sz --world-check
    done
  done
  unset SG_RASTER_THREADS
  d=$O/time_cam_off; rm -f $d.view.log
  SG_TERRAIN_PASS_LOG=$d.view.log $LT cov_l_camera $d -- $COMMON
fi
