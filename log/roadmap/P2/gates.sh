#!/bin/sh
# P2 gates on the mirror port/build/P2 (env as log/roadmap/merges/I18/env.sh). Usage: sh log/roadmap/P2/gates.sh <step>...
# Steps: gd_x64 gd_win32 ft_x64 ft_win32 app_shell app_live port_static segs_x64 segs_win32 boot_x64. One line per step in log/roadmap/P2/g_gates.log.
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
M=$R/port/build/P2
cd $M || exit 1
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_TREE=$M
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY
O=log/roadmap/P2
PY="$HOME/bin/py -3.12"
TS="test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe"
m=$#; n=0
for s in "$@"; do
  n=$((n + 1)); unset SG_BACKEND
  echo "== $s start $(date '+%F %T')" > $O/g_$s.log
  t0=$(date +%s)
  case $s in
    gd_x64) $PY -m pytest port/test/test_gditext.py port/test/test_gditext_tempdir.py -m "" -q -p no:cacheprovider ;;
    gd_win32) export SG_BACKEND=win32; $PY -m pytest port/test/test_gditext.py port/test/test_gditext_tempdir.py -m "" -q -p no:cacheprovider ;;
    ft_x64) $PY -m pytest port/test/test_fttext.py -m "" -q -p no:cacheprovider ;;
    ft_win32) export SG_BACKEND=win32; $PY -m pytest port/test/test_fttext.py -m "" -q -p no:cacheprovider ;;
    app_shell) $PY -m pytest port/test/test_app_shell.py -m "" -q -p no:cacheprovider ;;
    app_live) $PY -m pytest port/test/test_app_live.py -m "" -q -p no:cacheprovider ;;
    segs_win32) $TS -m port.test.integrator segments --backend win32 --jobs 4 --out $O/segs_win32.tsv ;;
    port_static) $PY -m pytest port/test/test_static.py -m "" -q -p no:cacheprovider ;;
    segs_x64) $TS -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_x64.tsv ;;
    boot_x64) $TS -m port.test.boot --all --jobs 3 --out $O/boot_x64.tsv ;;
    *) echo "unknown step $s"; false ;;
  esac >> $O/g_$s.log 2>&1
  rc=$?
  line="[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(tail -1 $O/g_$s.log | cut -c1-160)"
  echo "$line" >> $O/g_gates.log; echo "$line"
done
