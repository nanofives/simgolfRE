#!/bin/sh
# I10: the port suite one file per process (log/p38/suite_by_file.sh's rule) on the mirror port/build/I10.
# Env: OUT (dir under the mirror, default log/roadmap/merges/I10/suite), SG_BACKEND (unset = x64, win32 for the Win32 pass),
# FILES (space-separated list, default every port/test/test_*.py), FROM / TO (1-based file indices, to split a run under the
# 2-hour background limit). Each file waits for >= MINFREE GB (16) free commit charge before it starts.
# One line per file: [N/M] file STATUS (summary).
# LOG (optional): each result line is also appended there, and SIGPIPE is ignored, so the loop outlives the background shell's
# 2-hour limit (I10 second session; the first session's tee'd loop stopped at the limit).
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I10 || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I10/env.sh
out=${OUT:-log/roadmap/merges/I10/suite}
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
  ~/bin/py -3.12 -m pytest "$f" -q -p no:cacheprovider > "$out/$b.txt" 2>&1
  rc=$?
  s=$(tail -1 "$out/$b.txt")
  if [ $rc -eq 0 ] || [ $rc -eq 5 ]; then st=PASS; else st=FAIL; fi
  line="[$n/$m] $b $st $(( $(date +%s) - t0 )) s free=${FREE}GB ($s)"
  [ -n "$LOG" ] && echo "$line" >> "$LOG"
  echo "$line" 2>/dev/null || true
done
