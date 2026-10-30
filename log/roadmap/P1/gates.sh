#!/bin/sh
# P1 gates on the mirror port/build/P1 (env as log/roadmap/merges/I18/env.sh). Usage: sh log/roadmap/P1/gates.sh <step>...
# Steps: audio_x64 audio_win32 port_static segs_x64 boot_x64. One line per step in log/roadmap/P1/g_gates.log.
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/P1
cd $M || exit 1
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_TREE=$M
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY
O=log/roadmap/P1
PY="$HOME/bin/py -3.12"
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
AUDIO=$(ls port/test/test_audio_*.py)
m=$#; n=0
for s in "$@"; do
  n=$((n + 1)); unset SG_BACKEND
  echo "== $s start $(date '+%F %T')" > $O/g_$s.log
  t0=$(date +%s)
  case $s in
    audio_x64|audio_win32)
      [ $s = audio_win32 ] && export SG_BACKEND=win32
      k=0; for f in $AUDIO; do k=$((k + 1))
        $PY -m pytest "$f" -m "" -q -p no:cacheprovider > $O/suite_$s.$(basename $f .py).txt 2>&1; rc=$?
        st=FAIL; { [ $rc -eq 0 ] || [ $rc -eq 5 ]; } && st=PASS
        echo "[$k/$(echo $AUDIO | wc -w)] $(basename $f .py) $st ($(tail -1 $O/suite_$s.$(basename $f .py).txt))"
      done ;;
    port_static) $PY -m pytest port/test/test_static.py -m "" -q -p no:cacheprovider ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_x64.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 3 --out $O/boot_x64.tsv ;;
    *) echo "unknown step $s"; false ;;
  esac >> $O/g_$s.log 2>&1
  rc=$?
  line="[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $O/g_$s.log | cut -c1-160)"
  echo "$line" >> $O/g_gates.log; echo "$line"
done
