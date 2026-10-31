#!/bin/sh
# S7 lead, wave 4 on the I19 tree (S7.10 + S7.11 + S7.12 rebased) on port/build/S7_w4_root (11_mirror.py; base 37e58e96 + S7.10 / S7.11 files). One step at a time,
# each after >= 16 GB free commit charge; full output tee'd to log/roadmap/S7/11_g_<step>.log, one line per step in
# w4_g_gates.log: "[N/M] step rc=.. s :: last line". Steps: app_<file>_x64 / app_<file>_win32 (one test file per process),
# segs_x64 segs_win32 boot_x64 boot_win32 equiv_x64 equiv_win32 replay_x64 replay_win32.
# Usage: sh log/roadmap/S7/11_gates.sh <step>...
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/S7_w4_root
cd $M || exit 1
O=$R/log/roadmap/S7
S=$O/w4_out
mkdir -p $S/facts
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_APP_FACTS_DIR=$S/facts SG_TREE=$M
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY SG_RASTER_THREADS SG_TERRAIN_PASS_LOG SG_BACKEND
X64APP=$M/port/build/x64/app/Release/simgolf_port.exe
W32APP=$M/port/build/Win32/app/Release/simgolf_port.exe
PY="$HOME/bin/py -3.12"
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
W32="--dll32 $SG_PORTSIM_DLL32 --runner32 $SG_PORTSIM_RUNNER32"
waitmem() {
  while :; do
    FREE=$(powershell -NoProfile -Command "[int]((Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory/1MB)")
    [ "${FREE:-0}" -ge 16 ] && return
    echo "  (free commit ${FREE} GB < 16, waiting)"; sleep 60
  done
}
m=$#; n=0
for s in "$@"; do
  n=$((n + 1)); waitmem
  L=$O/w4_g_$s.log
  echo "== $s start $(date '+%F %T') free=${FREE}GB" > $L
  t0=$(date +%s)
  case $s in
    app_*_x64) f=${s#app_}; f=${f%_x64}
       SG_APP_EXE=$X64APP $PY -m pytest port/test/test_app_$f.py -m "" -v -p no:cacheprovider ;;
    app_*_win32) f=${s#app_}; f=${f%_win32}
       SG_BACKEND=win32 SG_APP_EXE=$W32APP $PY -m pytest port/test/test_app_$f.py -m "" -v -p no:cacheprovider ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 4 --out $S/segs_x64.tsv ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out $S/segs_win32.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 3 --out $S/boot_x64.tsv ;;
    boot_win32) SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 3 --out $S/boot_win32.tsv ;;
    equiv_x64) $TS -m port.test.live equiv --all --jobs 3 --out $S/equiv_x64.tsv ;;
    equiv_win32) SG_BACKEND=win32 $TS -m port.test.live equiv --all --jobs 3 --out $S/equiv_win32.tsv ;;
    replay_x64) $TS -m port.test.app_replay gate --out $S/app_replay_x64.tsv ;;
    replay_win32) $TS -m port.test.app_replay gate --backend win32 --out $S/app_replay_win32.tsv ;;
    segs0_x64) $PY $O/12_lead_segs_sw0.py $S/segs0_x64.tsv ;;
    segs0_win32) SG_BACKEND=win32 $PY $O/12_lead_segs_sw0.py $S/segs0_win32.tsv ;;
    defects_x64) $PY -m pytest port/test/test_defects_golfsfx.py -m "" -v -p no:cacheprovider ;;
    boot0_x64) $PY $O/12_lead_boot_sw0.py $S/boot0_x64.tsv --jobs 3 ;;
    boot0_win32) SG_BACKEND=win32 $PY $O/12_lead_boot_sw0.py $S/boot0_win32.tsv --jobs 3 ;;
    *) echo "unknown step $s"; false ;;
  esac >> $L 2>&1
  rc=$?
  line="[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s free=${FREE}GB :: $(tail -1 $L | cut -c1-160)"
  echo "$line" >> $O/w4_g_gates.log; echo "$line" 2>/dev/null || true
done
echo "[done] $*" >> $O/w4_g_gates.log
