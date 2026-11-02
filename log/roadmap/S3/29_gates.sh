#!/bin/sh
# S3.29: gates on the tree port/build/S3_29 (21_gates.sh's pattern, --jobs 1, each step one `w8_lock.py run S3.29` hold).
# Writes log/roadmap/S3/29_out/g_<step>.log and appends a line to 29_out/g_gates.log.
# Usage: sh log/roadmap/S3/29_gates.sh <step>...   steps: build_x64 build_w32 t_s329 t_<file> w32_<file> g2_stream_off g2_stream_on
#        g2_chain_off g2_chain_on segs_x64 segs_win32 boot_x64 boot_win32
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/S3_29
cd $M || exit 1
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_TREE=$M
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY SG_GL_BACKEND SG_RASTER_TUNE
O=$R/log/roadmap/S3/29_out
mkdir -p $O
PY="$HOME/bin/py -3.12"
PYX=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
TS="test-slot $PYX"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
WL="$PY $R/log/roadmap/S3/w8_lock.py"
main() {
m=$#
n=0
for s in "$@"; do
  n=$((n + 1))
  unset SG_BACKEND
  echo "== $s start $(date '+%F %T')" > $O/g_$s.log
  t0=$(date +%s)
  $WL run S3.29 --what "$s" -- sh $R/log/roadmap/S3/29_gates.sh --step $s >> $O/g_$s.log 2>&1
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
    build_x64) $PYX port/test/build.py --arch x64 ;;
    build_w32) $PYX port/test/build.py --arch Win32 ;;
    t_s329) $TS -m pytest port/test/test_gl_raster.py -k s329 -m "" -q -p no:cacheprovider ;;
    g2_stream_off) PYTHONPATH=. $TS $R/log/roadmap/S3/1b_g2_stream.py > $O/g2_stream_off.log 2>&1; tail -1 $O/g2_stream_off.log ;;
    g2_stream_on) SG_RASTER_TUNE=32=1 PYTHONPATH=. $TS $R/log/roadmap/S3/1b_g2_stream.py > $O/g2_stream_on.log 2>&1; tail -1 $O/g2_stream_on.log ;;
    g2_chain_off) PYTHONPATH=. $TS $R/log/roadmap/S3/1_g2.py > $O/g2_chain_off.log 2>&1; tail -1 $O/g2_chain_off.log ;;
    g2_chain_on) SG_RASTER_TUNE=32=1 PYTHONPATH=. $TS $R/log/roadmap/S3/1_g2.py > $O/g2_chain_on.log 2>&1; tail -1 $O/g2_chain_on.log ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $O/segs_x64.tsv ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 $W32 --jobs 1 --out $O/segs_win32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 1 --out $O/boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 1 --out $O/boot_win32.tsv ;;
    g2_all) SG_RASTER_TUNE=32=1 PYTHONPATH=. $TS $R/log/roadmap/S3/1b_g2_stream.py > $O/g2_stream_on.log 2>&1; tail -1 $O/g2_stream_on.log ;;   # one run per hold
    fh_s329) PYTHONPATH=. $TS $R/log/roadmap/S3/29_framehash.py dump $O/framehash_s329.json ;;
    fh_w8) SG_PORTSIM_DLL=$R/port/build/S3_W8/port/build/x64/Release/portsim_c.dll PYTHONPATH=. $TS $R/log/roadmap/S3/29_framehash.py dump $O/framehash_w8.json ;;
    x64_all) $TS -m port.test.integrator segments --backend x64 --jobs 1 --out $O/segs_x64.tsv > $O/segs_x64.log 2>&1; tail -1 $O/segs_x64.log ;;   # one segments run per hold (lead, 2026-10-31)
    w32_all) $TS -m port.test.integrator segments --backend win32 $W32 --jobs 1 --out $O/segs_win32.tsv > $O/segs_win32.log 2>&1; tail -1 $O/segs_win32.log ;;
    t_*) f=${s#t_}; $TS -m pytest port/test/$f.py -m "" -q -p no:cacheprovider ;;
    w32_*) f=${s#w32_}; SG_BACKEND=win32 $TS -m pytest port/test/$f.py -m "" -q -p no:cacheprovider ;;
    *) echo "unknown step $s"; false ;;
  esac
}

if [ "$1" = "--step" ]; then run_step "$2"; exit $?; fi
main "$@"
