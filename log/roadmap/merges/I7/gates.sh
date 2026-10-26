#!/bin/sh
# I7: every regression gate of re/roadmap/BASELINE.md plus the four waves' own gate lines, one heavy run at a time, from the mirror
# root port/build/I7. Each step waits until the free commit charge is 16 GB or more, prints one line per step as it ends
# ("[N/M] name rc=..") and tees its full output to log/roadmap/merges/I7/g_<name>.log.
# Usage: sh log/roadmap/merges/I7/gates.sh [first step number]
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I7 || exit 1
O=log/roadmap/merges/I7
M=$PWD
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_APP_EXE=$M/port/build/x64/app/Release/simgolf_port.exe
unset SG_FREETYPE_ROOT
PY="$HOME/bin/py -3.12"
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
first=${1:-1}

waitmem() {
  while :; do
    f=$(powershell -NoProfile -Command "[int]((Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory/1MB)")
    [ "${f:-0}" -ge 16 ] && return
    echo "  (free commit ${f} GB < 16, waiting)"
    sleep 60
  done
}

STEPS="suite_x64 segs_x64 segs_win32 boot_x64 boot_win32 suite_win32 s5_widths s5_labels s5_ft_segs_x64 s5_ft_segs_win32 s5_ft_boot_x64 s5_ft_boot_win32 s5_gfx_ft s7_live_equiv_x64 s7_live_equiv_win32 s7_app_replay_x64 s7_app_replay_win32 chain defects census terrain"
m=$(echo $STEPS | wc -w)
n=0
for s in $STEPS; do
  n=$((n + 1))
  [ $n -lt $first ] && continue
  waitmem
  t0=$(date +%s)
  case $s in
    suite_x64) STATIC=0 OUT=$O/suite sh $O/suite_by_file.sh ;;
    segs_x64) $PY -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_x64.tsv ;;
    segs_win32) $PY -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out $O/segs_win32.tsv ;;
    boot_x64) $PY -m port.test.boot --all --jobs 3 --out $O/boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $PY -m port.test.boot --all --jobs 3 --out $O/boot_win32.tsv ;;
    suite_win32) SG_BACKEND=win32 STATIC=0 OUT=$O/suite_win32 FILES="$(cat $O/win32_files.txt)" sh $O/suite_by_file.sh ;;
    s5_widths) $PY -m port.test.widths_check; $PY -m port.test.widths_check --provider freetype;
               $PY -m port.test.widths_check --provider freetype --win32 port/build/Win32 ;;
    s5_labels) $PY -m port.test.labels corpus && $PY -m port.test.labels reference && $PY -m port.test.labels compare gdi &&
               $PY -m port.test.labels render freetype && $PY -m port.test.labels compare freetype ;;
    s5_ft_segs_x64) SG_TEXT_METRICS=freetype $PY -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_ft_x64.tsv ;;
    s5_ft_segs_win32) SG_TEXT_METRICS=freetype $PY -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out $O/segs_ft_win32.tsv ;;
    s5_ft_boot_x64) SG_TEXT_METRICS=freetype $PY -m port.test.boot --all --jobs 3 --out $O/boot_ft_x64.tsv ;;
    s5_ft_boot_win32) SG_TEXT_METRICS=freetype SG_BACKEND=win32 $PY -m port.test.boot --all --jobs 3 --out $O/boot_ft_win32.tsv ;;
    s5_gfx_ft) SG_TEXT_METRICS=freetype $PY i7/roadmap/S5/3_gfx_ft.py --provider fttext --out $O/gfx_ft_x64.json ;;
    s7_live_equiv_x64) $PY -m port.test.live equiv --all --jobs 3 --out $O/live_equiv_x64.tsv ;;
    s7_live_equiv_win32) $PY -m port.test.live equiv --all --jobs 3 --backend win32 --out $O/live_equiv_win32.tsv ;;
    s7_app_replay_x64) $PY -m port.test.app_replay gate --out $O/app_replay_x64.tsv ;;
    s7_app_replay_win32) $PY -m port.test.app_replay gate --backend win32 --out $O/app_replay_win32.tsv ;;
    chain) mkdir -p i7/ledger; $PY -m port.test.integrator chain --backend x64 --all --jobs 4 --out i7/ledger/chain_x64.tsv ;;
    defects) $PY -m port.test.integrator defects --backend x64 --jobs 4 --out i7/ledger/defects_x64.tsv &&
             cp $O/segs_x64.tsv i7/ledger/segs_x64.tsv &&
             $PY -m port.test.integrator ledger --segments i7/ledger/segs_x64.tsv --chain i7/ledger/chain_x64.tsv --defects i7/ledger/defects_x64.tsv --out i7/ledger/divergences.tsv &&
             cp i7/ledger/*.tsv $O/ ;;
    census) $PY -m port.test.framegate --all --jobs 2 --out $O/census.tsv --resume ;;
    terrain) $PY ../../../log/roadmap/S3/2_census.py terrain --jobs 3 --out $O/terrain_verdicts.tsv ;;
  esac > $O/g_$s.log 2>&1
  rc=$?
  echo "[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $O/g_$s.log | cut -c1-160)"
done
echo "[done] gates"
