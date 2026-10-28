#!/bin/sh
# S3.13 (continuation after the 2026-10-27 reboot): the port suites 13_gates.sh's steps 2 and 7 did not finish.
# Same environment as 13_gates.sh. Usage:
#   sh log/roadmap/S3/13_suite_rest.sh x64     # files 31..75 of the mirror's port/test/test_*.py -> 13_g_suite_x64_b.log
#   sh log/roadmap/S3/13_suite_rest.sh win32   # all 75 files, SG_BACKEND=win32 -> 13_g_suite_win32.log
# Each pytest takes a machine-wide test slot through proyectos_test_slot.pth.
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_13 || exit 1
O=log/roadmap/S3/13
M=$PWD
export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$M/port/build/Win32
unset SG_FREETYPE_ROOT SG_TEXT_METRICS SG_EMU_ORACLE SG_BACKEND
case $1 in
  x64)
    FILES="$(ls port/test/test_*.py | sed -n "${FROM:-31},75p")" STATIC=0 OUT=${O}_suite sh log/roadmap/S3/13_suite_by_file.sh ;;
  win32)
    SG_BACKEND=win32 FILES="$(ls port/test/test_*.py)" STATIC=0 OUT=${O}_suite_win32 sh log/roadmap/S3/13_suite_by_file.sh ;;
  *) echo "usage: $0 x64|win32"; exit 2 ;;
esac
echo "[done] suite $1"
