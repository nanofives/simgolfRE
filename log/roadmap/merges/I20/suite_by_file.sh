#!/bin/sh
# I20: the port suite one file per process on the mirror port/build/I20 (copy of log/roadmap/merges/I13/suite_by_file.sh).
# Env: OUT (dir under the mirror, default log/roadmap/merges/I20/suite), SG_BACKEND (unset = x64, win32 for the Win32 pass),
# FILES (space-separated list, default every port/test/test_*.py), FROM / TO (1-based file indices, to split a run under the
# 2-hour background limit). Each file waits for >= MINFREE GB (16) free commit charge. `-m ""` = full counts.
# One line per file: [N/M] file STATUS (summary). LOG (optional): each result line is also appended there; SIGPIPE ignored.
# After every test_app_* file the tracked S7 facts files are restored (U-1398, env.sh restore_facts).
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I20 || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I20/env.sh
out=${OUT:-log/roadmap/merges/I20/suite}
# G17 (re/roadmap/DECISIONS_PENDING.md, 2026-10-30): the suites stop after block 2; step 3 (OUT suite / suite_win32) is skipped
case "$out" in */suite|*/suite_win32) [ -n "$LOG" ] && echo "skipped by G17 (rest of the suite; record: I19 on f2dff003)" >> "$LOG"; exit 0 ;; esac
mkdir -p "$out"
files=${FILES:-$(ls port/test/test_*.py)}
m=$(echo $files | wc -w)
n=0
for f in $files; do
  n=$((n + 1))
  [ $n -lt ${FROM:-1} ] && continue
  [ $n -gt ${TO:-9999} ] && continue
  waitmem
  b=$(basename "$f" .py)
  t0=$(date +%s)
  # I20: the Win32 pass runs the app files with the Win32 app (as log/roadmap/S7/w4_gates.sh app_*_win32)
  APPX=$SG_APP_EXE; case "$SG_BACKEND:$b" in win32:test_app_*) APPX=$M/port/build/Win32/app/Release/simgolf_port.exe ;; esac
  SG_APP_EXE=$APPX ~/bin/py -3.12 -m pytest "$f" -m "" -q -p no:cacheprovider > "$out/$b.txt" 2>&1
  rc=$?
  case $b in test_app_*) restore_facts "$(basename $out)_$b" >> "$out/$b.txt" ;; esac
  s=$(grep -v '^  (facts' "$out/$b.txt" | tail -1)
  if [ $rc -eq 0 ] || [ $rc -eq 5 ]; then st=PASS; else st=FAIL; fi
  line="[$n/$m] $b $st $(( $(date +%s) - t0 )) s free=${FREE}GB ($s)"
  [ -n "$LOG" ] && echo "$line" >> "$LOG"
  echo "$line" 2>/dev/null || true
done
