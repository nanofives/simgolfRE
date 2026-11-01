#!/bin/bash
# S3.24 gate steps, one heavy run each (call through 24_turn.sh): 24_gates.sh <step>
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
T=/c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_24
cd $T || exit 1
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL="$(pwd -W)/port/build/x64/Release/portsim_c.dll"
W32="$(pwd -W)/port/build/Win32/Release"
TS="$HOME/bin/py -3.12"
ROWS=championship,cov_a_build,cov_a_champ,cov_a_golfer,cov_a_keys,cov_a_keys3,cov_a_play,cov_b_decor,cov_r_bio,cov_r_tutorial,ui_tour,ui_tour_v
case $1 in
  build_x64) $TS $S/24_build.py x64 ;;
  build_w32) $TS $S/24_build.py Win32 ;;
  pytest_x64) $TS -m pytest port/test/test_terrain_overlay.py -q -p no:cacheprovider ;;
  pytest_x64_frames) SG_S38_FRAMES=1 $TS -m pytest port/test/test_terrain_overlay.py -q -p no:cacheprovider -k frame ;;
  pytest_w32) $TS -m pytest port/test/test_terrain_overlay.py -q -p no:cacheprovider; echo "== Win32"; SG_BACKEND=win32 $TS -m pytest port/test/test_terrain_overlay.py -q -p no:cacheprovider ;;
  pytest_static) $TS -m pytest port/test/test_static.py -q -p no:cacheprovider ;;
  rows_default) $TS $S/24_rows.py --tag d2 --scenarios $ROWS ;;
  rows_ctl11) $TS $S/24_rows.py --tag c11 --scenarios $ROWS --negctl 11 ;;
  rows_default_raster) $TS $S/24_rows.py --tag d2r --raster --scenarios $ROWS ;;
  segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $S/24_segs_x64.tsv ;;
  segs_w32) $TS -m port.test.integrator segments --backend win32 --jobs 1 --out $S/24_segs_w32.tsv ;;
  boot_x64) $TS -m port.test.boot --all --jobs 1 --out $S/24_boot_x64.tsv ;;
  boot_w32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 1 --out $S/24_boot_w32.tsv ;;
  rows_w32) SG_BACKEND=win32 $TS $S/24_rows.py --tag d2w --scenarios $ROWS ;;
  streams) cd ../../.. && $TS log/roadmap/S3/24_streams.py real log/roadmap/S3/24_data/streams.npz log/roadmap/S3/24_data/dump_*.bin --per 1000 ;;
  census_*) b=${1#census_}; bash $S/24_census.sh 2 $b ;;
  *) echo "unknown step $1"; exit 2 ;;
esac
