#!/bin/bash
# S3.26 gate steps, one heavy run each (always called through w8_lock.py run): 26_gates.sh <step>
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
T=/c/Users/maria/Desktop/Proyectos/SimGolf/port/build/${TREE:-S3_26}
PY=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
cd $T || exit 1
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL="$(pwd -W)/port/build/x64/Release/portsim_c.dll"
O=$S/26_out${TREE:+_$TREE}; mkdir -p $O
case $1 in
  build_x64) $PY $S/24_build.py x64 --tree ${TREE:-S3_26} ;;
  build_w32) $PY $S/24_build.py Win32 --tree ${TREE:-S3_26} ;;
  pytest_*) f=${1#pytest_}; $PY -m pytest port/test/$f.py -q -p no:cacheprovider ;;
  w32pytest_*) f=${1#w32pytest_}; SG_BACKEND=win32 $PY -m pytest port/test/$f.py -q -p no:cacheprovider ;;
  segs_x64) $PY -m port.test.integrator segments --backend x64 --jobs 1 --out $O/segs_x64.tsv ;;
  segs_w32) $PY -m port.test.integrator segments --backend win32 --jobs 1 --out $O/segs_w32.tsv ;;
  boot_x64) $PY -m port.test.boot --all --jobs 1 --out $O/boot_x64.tsv ;;
  boot_w32) SG_BACKEND=win32 $PY -m port.test.boot --all --jobs 1 --out $O/boot_w32.tsv ;;
  census_*) b=${1#census_}; SC=$(sed -n "$(( b * 14 + 1 )),$(( b * 14 + 14 ))p" $S/24_census_scen.txt | tr '\n' ' ')
     [ -z "$SC" ] && { echo "empty batch"; exit 0; }
     echo "batch $b: $SC"; $PY -m port.test.framegate $SC --jobs 1 --no-probe --out $O/census_b$b.tsv --resume ;;
  hostrows) $PY -m port.test.framegate cov_b_inject cov_b_inject2 cov_r_s2_screens golfer_stories cov_k3_camera cov_k3_camera2 cov_b_holes18 --gl host --jobs 1 --no-probe --out $O/host_rows_x64.tsv --resume ;;
  w32rows) SG_BACKEND=win32 $PY -m port.test.framegate cov_b_inject cov_b_inject2 cov_r_s2_screens golfer_stories cov_k3_camera2 cov_b_holes18 --gl host --jobs 1 --no-probe --out $O/host_rows_w32.tsv --resume ;;
  *) echo "unknown step $1"; exit 2 ;;
esac
