#!/bin/sh
# S3.21: gates on the tree port/build/S3_21 (I19's gates.sh pattern, --jobs 1, each step under the wave-8 heavy lock:
# log/roadmap/S3/W8_HEAVY_RULE.md, ENFORCED: each step is one `w8_lock.py run S3.21` hold). Writes log/roadmap/S3/21_out/g_<step>.log and appends a line to 21_out/g_gates.log.
# Usage: sh log/roadmap/S3/21_gates.sh <step>...
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/S3_21
cd $M || exit 1
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_TREE=$M
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY SG_GL_BACKEND
O=$R/log/roadmap/S3/21_out
mkdir -p $O
PY="$HOME/bin/py -3.12"
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
SAMPLE="sandbox_basic cov_k1_river championship cov_k3_camera2 cov_a_build"
WL="$PY $R/log/roadmap/S3/w8_lock.py"
main() {
m=$#
n=0
for s in "$@"; do
  n=$((n + 1))
  unset SG_BACKEND
  echo "== $s start $(date '+%F %T')" > $O/g_$s.log
  t0=$(date +%s)
  # each step = one hold: w8_lock.py run takes the lock in wait-file order, runs `sh 21_gates.sh --step <s>`, always releases
  $WL run S3.21 --what "$s" -- sh $R/log/roadmap/S3/21_gates.sh --step $s >> $O/g_$s.log 2>&1
  rc=$?
  line="[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $O/g_$s.log | cut -c1-160)"
  echo "$line" >> $O/g_gates.log
  echo "$line" 2>/dev/null || true
done
echo "[done] gates $*"
exit 0
}

run_step() {
  s=$1
  case $s in
    build_x64) SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed' $PY port/test/build.py --arch x64 ;;
    build_w32) SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed' $PY port/test/build.py --arch Win32 ;;
    static) $PY -m pytest -m static -q -p no:cacheprovider ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $O/segs_x64.tsv ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 $W32 --jobs 1 --out $O/segs_win32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 1 --out $O/boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 1 --out $O/boot_win32.tsv ;;
    terrain) $TS $R/log/roadmap/S3/2_census.py terrain --jobs 1 --out $O/terrain_verdicts.tsv ;;
    sample) $TS -m port.test.framegate $SAMPLE --jobs 1 --no-probe --out $O/census_sample.tsv ;;
    sample_ctl28) SG_RASTER_TUNE=28=1,29=1 $TS -m port.test.framegate sandbox_basic cov_k1_river --jobs 1 --no-probe --out $O/census_sample_ctl28.tsv ;;
    sample_w32) SG_BACKEND=win32 $TS -m port.test.framegate sandbox_basic cov_k1_river --jobs 1 --no-probe --out $O/census_sample_w32.tsv ;;
    census) $TS -m port.test.framegate --all --jobs 1 --no-probe --out $O/census.tsv --resume ;;
    t_*) f=${s#t_}; $TS -m pytest port/test/$f.py -m "" -q -p no:cacheprovider ;;
    w32_*) f=${s#w32_}; SG_BACKEND=win32 $TS -m pytest port/test/$f.py -m "" -q -p no:cacheprovider ;;
    *) echo "unknown step $s"; false ;;
  esac
}

if [ "$1" = "--step" ]; then run_step "$2"; exit $?; fi
main "$@"
