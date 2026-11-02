#!/bin/bash
# S3.28 remaining gates on port/build/S3_28, ONE run per w8_lock.py hold (W9_COMMON.md: one pytest file, one segments or boot step).
#   bash log/roadmap/S3/28_gates2.sh [step ...]      logs log/roadmap/S3/28_out/g2_<step>.log, one line "[k/N] step rc=.." each
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
T=/c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_28
O=$S/28_out
cd $T || exit 1
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL="$(pwd -W)/port/build/x64/Release/portsim_c.dll"
PY=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
PT="-q -p no:cacheprovider"
cmd() {
  case $1 in
    static) echo "$PY -m pytest port/test/test_static.py $PT" ;;
    s328_box) echo "$PY -m pytest port/test/test_terrain_s3_28.py -k map_corner $PT" ;;
    terrain_live) echo "$PY -m pytest port/test/test_terrain_live.py $PT" ;;
    segs_x64) echo "$PY -m port.test.integrator segments --backend x64 --jobs 1 --out $O/segs_x64.tsv" ;;
    segs_w32) echo "$PY -m port.test.integrator segments --backend win32 --jobs 1 --out $O/segs_w32.tsv" ;;
    boot_x64) echo "$PY -m port.test.boot --all --jobs 1 --out $O/boot_x64.tsv" ;;
    boot_w32) echo "env SG_BACKEND=win32 $PY -m port.test.boot --all --jobs 1 --out $O/boot_w32.tsv" ;;
    w32_s328) echo "env SG_BACKEND=win32 $PY -m pytest port/test/test_terrain_s3_28.py $PT" ;;
    w32_gfx) echo "env SG_BACKEND=win32 $PY -m pytest port/test/test_gfx.py $PT" ;;
    *) echo "false" ;;
  esac
}
STEPS=${*:-static s328_box segs_x64 boot_x64 segs_w32 boot_w32 terrain_live w32_s328 w32_gfx}
N=$(echo $STEPS | wc -w); k=0
for s in $STEPS; do
  k=$((k+1)); t0=$(date +%s)
  $PY $S/w8_lock.py run S3.28 --what "S3.28 $s" -- $(cmd $s) > $O/g2_$s.log 2>&1
  rc=$?
  echo "[$k/$N] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -n 1 $O/g2_$s.log | cut -c1-160)"
done
echo S328_GATES2_DONE
