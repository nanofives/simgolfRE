#!/bin/bash
# S3.28 gates on port/build/S3_28: one w8_lock.py hold per step (step S3.28), logs in log/roadmap/S3/28_out/g_<step>.log.
#   bash log/roadmap/S3/28_gates.sh [step ...]      (default: all steps in order; log lines "[k/N] step rc=.. secs")
S=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
T=/c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_28
O=$S/28_out
mkdir -p $O
cd $T || exit 1
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL="$(pwd -W)/port/build/x64/Release/portsim_c.dll"
TS="$HOME/bin/py -3.12"
LOCK="$TS $S/w8_lock.py run S3.28"
PT="-q -p no:cacheprovider"
FILES="test_static test_terrain_s3_28 test_terrain_render test_gfx test_framegate_split test_terrain_live"
step() {
  case $1 in
    build_x64) $TS $S/24_build.py x64 --tree S3_28 ;;
    build_w32) $TS $S/24_build.py Win32 --tree S3_28 ;;
    pytest_x64) for f in $FILES; do echo "== $f"; $TS -m pytest port/test/$f.py $PT || echo "FAILED $f"; done ;;
    pytest_w32) for f in test_terrain_s3_28 test_gfx test_framegate_split; do echo "== $f"; SG_BACKEND=win32 $TS -m pytest port/test/$f.py $PT || echo "FAILED $f"; done ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $O/segs_x64.tsv ;;
    segs_w32) $TS -m port.test.integrator segments --backend win32 --jobs 1 --out $O/segs_w32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 1 --out $O/boot_x64.tsv ;;
    boot_w32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 1 --out $O/boot_w32.tsv ;;
    *) echo "unknown step $1"; return 2 ;;
  esac
}
STEPS=${*:-build_w32 pytest_x64 pytest_w32 segs_x64 segs_w32 boot_x64 boot_w32}
N=$(echo $STEPS | wc -w); k=0
for s in $STEPS; do
  k=$((k+1)); t0=$(date +%s)
  $LOCK --what "S3.28 $s" -- bash -c "$(declare -f step); $(declare -p TS PT FILES S O T 2>/dev/null); export SG_FREETYPE_ROOT SG_PORTSIM_DLL; step $s" > $O/g_$s.log 2>&1
  rc=$?
  echo "[$k/$N] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -n 1 $O/g_$s.log | cut -c1-160)"
done
echo S328_GATES_DONE
