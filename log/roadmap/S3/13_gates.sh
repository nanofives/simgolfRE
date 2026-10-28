#!/bin/sh
# S3.13: copy of log/roadmap/merges/I9/gates.sh on the mirror port/build/S3_13 (11_assemble.py + 13_edits.py on 78ffb06d).
# S3.8 + S3.11, assembled by log/roadmap/S3/11_assemble.py --dst I9 --commit 78ffb06d), one heavy run at a time. Each step waits until the
# free commit charge is 16 GB or more, prints one line per step as it ends ("[N/M] name rc=..") and tees its full output to
# log/roadmap/merges/I9/g_<name>.log. Steps are I8's gates.sh without S4's own gate.
# Usage: sh log/roadmap/merges/I9/gates.sh [first step number] [last step number]
# S3.13: every runner that is not pytest is wrapped in test-slot (the machine-wide test slots; claude2 runs no hooks).
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_13 || exit 1
O=log/roadmap/S3/13
M=$PWD
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
unset SG_FREETYPE_ROOT SG_TEXT_METRICS SG_EMU_ORACLE SG_BACKEND
PY=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe   # test-slot starts an exe, not the ~/bin/py shell shim
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
first=${1:-1}
last=${2:-999}
mkdir -p s313/ledger

waitmem() {
  while :; do
    f=$(powershell -NoProfile -Command "[int]((Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory/1MB)")
    [ "${f:-0}" -ge 16 ] && return
    echo "  (free commit ${f} GB < 16, waiting)"
    sleep 60
  done
}

STEPS="static suite_x64 segs_x64 segs_win32 boot_x64 boot_win32 suite_win32 s5_ft_segs_x64 s5_ft_boot_x64 chain defects gfx_check census terrain"
m=$(echo $STEPS | wc -w)
n=0
for s in $STEPS; do
  n=$((n + 1))
  [ $n -lt $first ] && continue
  [ $n -gt $last ] && continue
  waitmem
  t0=$(date +%s)
  case $s in
    static) $PY -m pytest -m static -q -p no:cacheprovider ;;
    suite_x64) STATIC=0 OUT=${O}_suite sh log/roadmap/S3/13_suite_by_file.sh ;;
    segs_x64) test-slot $PY -m port.test.integrator segments --backend x64 --jobs 4 --out ${O}_segs_x64.tsv ;;
    segs_win32) test-slot $PY -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out ${O}_segs_win32.tsv ;;
    boot_x64) test-slot $PY -m port.test.boot --all --jobs 3 --out ${O}_boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 test-slot $PY -m port.test.boot --all --jobs 3 --out ${O}_boot_win32.tsv ;;
    suite_win32) SG_BACKEND=win32 STATIC=0 OUT=${O}_suite_win32 FILES="$(cat log/roadmap/merges/I9/win32_files.txt)" sh log/roadmap/S3/13_suite_by_file.sh ;;
    s5_ft_segs_x64) SG_TEXT_METRICS=freetype test-slot $PY -m port.test.integrator segments --backend x64 --jobs 4 --out ${O}_segs_ft_x64.tsv ;;
    s5_ft_boot_x64) SG_TEXT_METRICS=freetype test-slot $PY -m port.test.boot --all --jobs 3 --out ${O}_boot_ft_x64.tsv ;;
    chain) test-slot $PY -m port.test.integrator chain --backend x64 --all --jobs 4 --out s313/ledger/chain_x64.tsv && cp s313/ledger/chain_x64.tsv ${O}_chain_x64.tsv ;;
    defects) test-slot $PY -m port.test.integrator defects --backend x64 --jobs 4 --out s313/ledger/defects_x64.tsv &&
             cp ${O}_segs_x64.tsv s313/ledger/segs_x64.tsv &&
             test-slot $PY -m port.test.integrator ledger --segments s313/ledger/segs_x64.tsv --chain s313/ledger/chain_x64.tsv --defects s313/ledger/defects_x64.tsv --out s313/ledger/divergences.tsv;
             cp s313/ledger/defects_x64.tsv ${O}_defects_x64.tsv; cp s313/ledger/divergences.tsv ${O}_ledger_divergences.tsv ;;
    gfx_check) $PY log/roadmap/S3/13_gfx_check.py x64 win32 ;;
    census) test-slot $PY -m port.test.framegate --all --jobs 2 --out ${O}_census.tsv --resume ;;
    terrain) test-slot $PY log/roadmap/S3/2_census.py terrain --jobs 3 --out ${O}_terrain_verdicts.tsv ;;
  esac > ${O}_g_$s.log 2>&1
  rc=$?
  echo "[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 ${O}_g_$s.log | cut -c1-160)"
done
echo "[done] gates"
