#!/bin/sh
# I7: the gates S7's removal can change, re-run on port/build/I7b (S2 waves 6 / 7 + S5, S7 waves 1 / 2 dropped). S7 is measured
# inert in trace runs (its LiveIo is unused there), so the C++ core gates are re-run: repo static, the port files that run frames or
# list modules / ops, segments and boot on both backends (also with SG_TEXT_METRICS=freetype on x64), and the frame census.
# One heavy run at a time, free commit charge >= 16 GB before each; one line per step "[N/M] name rc=..".
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I7b || exit 1
O=log/roadmap/merges/I7/b
mkdir -p $O
M=$PWD
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
unset SG_FREETYPE_ROOT SG_APP_EXE
PY="$HOME/bin/py -3.12"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
X64FILES="port/test/test_static.py port/test/test_tick.py port/test/test_boot.py port/test/test_segments.py port/test/test_win32.py port/test/test_gfx.py port/test/test_buyland.py port/test/test_gfx_s2_10.py port/test/test_gfx_dialogs2.py port/test/test_gfx_reports.py port/test/test_gditext.py port/test/test_fttext.py port/test/test_labels.py port/test/test_sve_roundtrip.py port/test/test_s2_6_scenario.py port/test/test_commands.py port/test/test_terrain_live.py"
W32FILES="port/test/test_tick.py port/test/test_buyland.py port/test/test_gfx_dialogs2.py port/test/test_fttext.py port/test/test_sve_roundtrip.py"
first=${1:-1}

waitmem() {
  while :; do
    f=$(powershell -NoProfile -Command "[int]((Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory/1MB)")
    [ "${f:-0}" -ge 16 ] && return
    echo "  (free commit ${f} GB < 16, waiting)"
    sleep 60
  done
}

runfiles() {  # out dir, files
  d=$1; shift; mkdir -p $d; k=0; t=$(echo $* | wc -w)
  for f in $*; do
    k=$((k + 1)); b=$(basename $f .py)
    $PY -m pytest $f -q -p no:cacheprovider > $d/$b.txt 2>&1; rc=$?
    if [ $rc -eq 0 ] || [ $rc -eq 5 ]; then s=PASS; else s=FAIL; fi
    echo "[$k/$t] $b $s ($(tail -1 $d/$b.txt))"
  done
}

STEPS="static suite_x64 segs_x64 segs_win32 segs_ft_x64 boot_x64 boot_win32 boot_ft_x64 suite_win32 census terrain"
m=$(echo $STEPS | wc -w)
n=0
for s in $STEPS; do
  n=$((n + 1))
  [ $n -lt $first ] && continue
  waitmem
  t0=$(date +%s)
  case $s in
    static) $PY -m pytest -m static -q -p no:cacheprovider ;;
    suite_x64) runfiles $O/suite $X64FILES ;;
    segs_x64) $PY -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_x64.tsv ;;
    segs_win32) $PY -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out $O/segs_win32.tsv ;;
    segs_ft_x64) SG_TEXT_METRICS=freetype $PY -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_ft_x64.tsv ;;
    boot_x64) $PY -m port.test.boot --all --jobs 3 --out $O/boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $PY -m port.test.boot --all --jobs 3 --out $O/boot_win32.tsv ;;
    boot_ft_x64) SG_TEXT_METRICS=freetype $PY -m port.test.boot --all --jobs 3 --out $O/boot_ft_x64.tsv ;;
    suite_win32) SG_BACKEND=win32 runfiles $O/suite_win32 $W32FILES ;;
    census) $PY -m port.test.framegate --all --jobs 2 --out $O/census.tsv --resume ;;
    terrain) $PY ../../../log/roadmap/S3/2_census.py terrain --jobs 3 --out $O/terrain_verdicts.tsv ;;
  esac > $O/g_$s.log 2>&1
  rc=$?
  echo "[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $O/g_$s.log | cut -c1-160)"
done
echo "[done] gates_b"
