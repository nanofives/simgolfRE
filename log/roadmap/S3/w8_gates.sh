#!/bin/bash
# S3 wave 8 stack check on port/build/S3_W8 (w8_assemble.py on 2291a342): one heavy hold per step through w8_lock.py.
#   bash log/roadmap/S3/w8_gates.sh [step ...]      (default: all steps in order; log lines "[k/N] step rc=.. secs")
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
T=/c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_W8
O=$S/w8_out
mkdir -p $O
cd $T || exit 1
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL="$(pwd -W)/port/build/x64/Release/portsim_c.dll"
TS="$HOME/bin/py -3.12"
LOCK="$TS $S/w8_lock.py run S3-lead"
PT="-q -p no:cacheprovider"
FILES="test_static test_gl_raster test_terrain_overlay test_framegate test_framegate_split test_gfx test_gfx_s3_26 test_gfx_s3_19"
step() {
  case $1 in
    build_x64) $TS $S/24_build.py x64 --tree S3_W8 ;;
    build_w32) $TS $S/24_build.py Win32 --tree S3_W8 ;;
    pytest_x64) for f in $FILES; do echo "== $f"; $TS -m pytest port/test/$f.py $PT || echo "FAILED $f"; done ;;
    pytest_w32) for f in test_gl_raster test_terrain_overlay test_framegate_split test_gfx test_gfx_s3_26; do echo "== $f"; SG_BACKEND=win32 $TS -m pytest port/test/$f.py $PT || echo "FAILED $f"; done ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $O/segs_x64.tsv ;;
    segs_w32) $TS -m port.test.integrator segments --backend win32 --jobs 1 --out $O/segs_w32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 1 --out $O/boot_x64.tsv ;;
    *) echo "unknown step $1"; return 2 ;;
  esac
}
STEPS=${*:-build_x64 build_w32 pytest_x64 pytest_w32 segs_x64 segs_w32 boot_x64}
N=$(echo $STEPS | wc -w); k=0
for s in $STEPS; do
  k=$((k+1)); t0=$(date +%s)
  $LOCK --what "w8 stack $s" -- bash -c "$(declare -f step); $(declare -p TS PT FILES S O T 2>/dev/null); export SG_FREETYPE_ROOT SG_PORTSIM_DLL; step $s" > $O/g_$s.log 2>&1
  rc=$?
  echo "[$k/$N] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -n 1 $O/g_$s.log | cut -c1-160)"
done
echo W8_GATES_DONE
