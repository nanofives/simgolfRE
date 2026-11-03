#!/bin/bash
# S11.4 gates on the copy port/build/S11_4 (4_assemble.py --names applied). ONE w8_lock.py hold per step (builds, one pytest file,
# each gate run). Writes log/roadmap/S11/4_out/g_<step>.log and appends "[k/N] step rc=.. s :: last line" to 4_out/g_gates.log.
#   bash log/roadmap/S11/4_gates.sh <step>...      steps: build_x64 build_w32 static model_header segs_x64 segs_w32 boot_x64 boot_w32
#                                                  t_<file> (x64 pytest of port/test/<file>.py) w32_<file>
R=/c/Users/maria/Desktop/Proyectos/SimGolf
T=$R/port/build/S11_4
O=$R/log/roadmap/S11/4_out
mkdir -p $O
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL=$T/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$T/port/build/Win32
export SG_PORTSIM_DLL32=$T/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$T/port/build/Win32/Release/portsim_runner32.exe
export SG_APP_EXE=$T/port/build/x64/app/Release/simgolf_port.exe
export SG_APP_FACTS_DIR=$O/facts
mkdir -p $SG_APP_FACTS_DIR
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY SG_BACKEND
LOCK="$HOME/bin/py -3.12 $R/log/roadmap/S3/w8_lock.py run S11.4"
PT="-q -p no:cacheprovider"
TS="~/bin/py -3.12"   # the caller wraps this script in test-slot (the PreToolUse hook rule)
W32="--dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe"
n=0; m=$#
for s in "$@"; do
  n=$((n + 1))
  case $s in
    build_x64) cmd="cd $T && ~/bin/py -3.12 port/test/build.py --arch x64" ;;
    build_w32) cmd="cd $T && ~/bin/py -3.12 port/test/build.py --arch Win32" ;;
    static) cmd="cd $T && $TS -m pytest port/test/test_static.py -m '' $PT" ;;
    model_header) cmd="cd $T && $TS -m pytest tests/test_model_header.py -m '' $PT" ;;
    segs_x64) cmd="cd $T && $TS -m port.test.integrator segments --backend x64 --jobs 4 --out $O/segs_x64.tsv" ;;
    segs_win32) cmd="cd $T && $TS -m port.test.integrator segments --backend win32 $W32 --jobs 4 --out $O/segs_win32.tsv" ;;
    boot_x64) cmd="cd $T && $TS -m port.test.boot --all --jobs 3 --out $O/boot_x64.tsv" ;;
    boot_win32) cmd="cd $T && SG_BACKEND=win32 $TS -m port.test.boot --all --jobs 3 --out $O/boot_win32.tsv" ;;
    t_*) f=${s#t_}; cmd="cd $T && $TS -m pytest port/test/$f.py -m '' $PT" ;;
    w32_*) f=${s#w32_}; cmd="cd $T && SG_BACKEND=win32 $TS -m pytest port/test/$f.py -m '' $PT" ;;
    *) echo "unknown step $s"; continue ;;
  esac
  t0=$(date +%s)
  $LOCK --what "S11.4 $s" -- bash -c "$cmd" > $O/g_$s.log 2>&1
  rc=$?
  line="[$n/$m] $s rc=$rc $(( $(date +%s) - t0 )) s :: $(grep -E 'passed|failed|error|PASS|FAIL|counts' $O/g_$s.log | tail -n 1 | cut -c1-160)"
  echo "$line" >> $O/g_gates.log
  echo "$line"
done
echo "[done] $*"
