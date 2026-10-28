#!/bin/sh
# S3.14: gates on the mirror port/build/S3_14 (14_assemble.py = 13_assemble.py + 14_edits.py on d66aa755), one heavy run at a time.
# Each step waits until the free commit charge is 16 GB or more, prints one line as it ends ("[N/M] name rc=..") and tees its
# full output to log/roadmap/S3/14_g_<name>.log. The app files run first and one file per process (their pacing gates measure
# the host). Every runner that is not pytest is wrapped in test-slot (the real python.exe: test-slot starts an exe).
# Usage: sh log/roadmap/S3/14_gates.sh [first step number] [last step number]
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_14 || exit 1
O=log/roadmap/S3/14
. ../../../log/roadmap/S3/14_env.sh
unset SG_FREETYPE_ROOT SG_BACKEND SG_APP_PRIORITY
PY=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
first=${1:-1}
last=${2:-999}

waitmem() {
  while :; do
    f=$(powershell -NoProfile -Command "[int]((Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory/1MB)")
    [ "${f:-0}" -ge 16 ] && return
    echo "  (free commit ${f} GB < 16, waiting)"
    sleep 60
  done
}

APP_FILES="$(ls port/test/test_app_*.py)"
REST_FILES="$(ls port/test/test_*.py | grep -v '/test_app_')"
STEPS="app_x64 segs_x64 segs_win32 boot_x64 boot_win32 census terrain static suite_x64"
m=$(echo $STEPS | wc -w)
n=0
for s in $STEPS; do
  n=$((n + 1))
  [ $n -lt $first ] && continue
  [ $n -gt $last ] && continue
  waitmem
  t0=$(date +%s)
  case $s in
    app_x64) FILES="$APP_FILES" OUT=${O}_suite sh ../../../log/roadmap/S3/14_suite_by_file.sh ;;
    segs_x64) test-slot $PY -m port.test.integrator segments --backend x64 --jobs 4 --out ${O}_segs_x64.tsv ;;
    segs_win32) test-slot $PY -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out ${O}_segs_win32.tsv ;;
    boot_x64) test-slot $PY -m port.test.boot --all --jobs 3 --out ${O}_boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 test-slot $PY -m port.test.boot --all --jobs 3 --out ${O}_boot_win32.tsv ;;
    census) test-slot $PY -m port.test.framegate --all --jobs 2 --out ${O}_census.tsv --resume ;;
    terrain) test-slot $PY log/roadmap/S3/2_census.py terrain --jobs 3 --out ${O}_terrain_verdicts.tsv ;;
    static) $PY -m pytest -m static -q -p no:cacheprovider ;;
    suite_x64) FILES="$REST_FILES" OUT=${O}_suite sh ../../../log/roadmap/S3/14_suite_by_file.sh ;;
  esac > ${O}_g_$s.log 2>&1
  rc=$?
  echo "[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 ${O}_g_$s.log | cut -c1-160)"
done
echo "[done] gates"
