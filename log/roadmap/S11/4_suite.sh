#!/bin/sh
# S11.4: the port suite one file per process on the copy port/build/S11_4 (pattern of log/roadmap/merges/I22/suite_by_file.sh), one
# w8_lock.py hold per file. Run it wrapped: `test-slot bash -c 'bash log/roadmap/S11/4_suite.sh'`.
# Env: OUT (default log/roadmap/S11/4_out/suite<_win32>), SG_BACKEND (unset = x64), FILES (default every port/test/test_*.py),
# FROM / TO (1-based indices), EXTRA (extra env, e.g. SG_EMU_ORACLE=update). One line per file: [N/M] file STATUS (summary).
trap '' PIPE
R=/c/Users/maria/Desktop/Proyectos/SimGolf
T=$R/port/build/S11_4
cd $T || exit 1
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL=$T/port/build/x64/Release/portsim_c.dll
export SG_PORTSIM_W32_DIR=$T/port/build/Win32
export SG_PORTSIM_DLL32=$T/port/build/Win32/Release/portsim_c.dll
export SG_PORTSIM_RUNNER32=$T/port/build/Win32/Release/portsim_runner32.exe
export SG_APP_EXE=$T/port/build/x64/app/Release/simgolf_port.exe
export SG_APP_FACTS_DIR=$R/log/roadmap/S11/4_out/facts
mkdir -p $SG_APP_FACTS_DIR
unset SG_TEXT_METRICS SG_APP_PRIORITY
out=${OUT:-$R/log/roadmap/S11/4_out/suite${SG_BACKEND:+_$SG_BACKEND}}
mkdir -p "$out"
files=${FILES:-$(ls port/test/test_*.py)}
m=$(echo $files | wc -w)
n=0
LOCK="$HOME/bin/py -3.12 $R/log/roadmap/S3/w8_lock.py run S11.4"
for f in $files; do
  n=$((n + 1))
  [ $n -lt ${FROM:-1} ] && continue
  [ $n -gt ${TO:-9999} ] && continue
  case $f in port/test/*) ;; *) f=port/test/$f.py ;; esac
  b=$(basename "$f" .py)
  t0=$(date +%s)
  APPX=$SG_APP_EXE; case "$SG_BACKEND:$b" in win32:test_app_*) APPX=$T/port/build/Win32/app/Release/simgolf_port.exe ;; esac
  $LOCK --what "S11.4 file $b" -- bash -c "cd $T && $EXTRA SG_APP_EXE=$APPX ~/bin/py -3.12 -m pytest $f -m '' -q -p no:cacheprovider" > "$out/$b.txt" 2>&1
  rc=$?
  s=$(grep -v '^  (facts\|^\[w8_lock\]' "$out/$b.txt" | tail -1)
  if [ $rc -eq 0 ] || [ $rc -eq 5 ]; then st=PASS; else st=FAIL; fi
  line="[$n/$m] $b $st $(( $(date +%s) - t0 )) s ($s)"
  echo "$line" >> "$out/../suite${SG_BACKEND:+_$SG_BACKEND}.log"
  echo "$line" 2>/dev/null || true
done
