#!/bin/sh
# S3.12: the port suite one file per process (log/p38/suite_by_file.sh's rule) on port/build/${TREE:-S3_12}.
# Env: OUT (dir, default log/roadmap/S3/12_out/<tree>/suite[_win32]), SG_BACKEND (unset = x64, win32 for the Win32 pass),
# FILES (space-separated list, default every port/test/test_*.py), FROM / TO (1-based file indices), LOG (each result line is
# also appended there; SIGPIPE ignored so the loop outlives its background shell). The app tests write their facts to
# OUT/facts_<name>/ (SG_APP_FACTS_DIR) where the test takes the parameter (test_app_ingame); the tracked log/roadmap/S7/8_*facts.json files the
# other app tests rewrite are restored one by one after each test_app_* file from byte copies of their HEAD content (never a dir checkout).
# One line per file: [N/M] file STATUS (summary).
trap '' PIPE
T=${TREE:-S3_12}
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/$T || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3/12_env.sh
R=/c/Users/maria/Desktop/Proyectos/SimGolf
sub=suite; [ "$SG_BACKEND" = win32 ] && sub=suite_win32
out=${OUT:-$R/log/roadmap/S3/12_out/$T/$sub}
mkdir -p "$out"
files=${FILES:-$(ls port/test/test_*.py)}
m=$(echo $files | wc -w)
n=0
restore_facts() {   # byte copies taken before the first run (log/roadmap/S3/12_out/facts_head, sha1 = HEAD content)
  for c in $R/log/roadmap/S3/12_out/facts_head/*.json; do
    f=$R/log/roadmap/S7/$(basename $c)
    cmp -s $c $f || { cp -p $c $f; echo "restored log/roadmap/S7/$(basename $c)"; }
  done
}
for f in $files; do
  n=$((n + 1))
  [ $n -lt ${FROM:-1} ] && continue
  [ $n -gt ${TO:-9999} ] && continue
  waitmem
  b=$(basename "$f" .py)
  t0=$(date +%s)
  mkdir -p $out/facts_$b; SG_APP_FACTS_DIR=$out/facts_$b ~/bin/py -3.12 -m pytest "$f" -m "" -q -p no:cacheprovider > "$out/$b.txt" 2>&1
  rc=$?
  case $b in test_app_*) restore_facts >> "$out/$b.txt" 2>&1 ;; esac
  s=$(grep -v '^restored ' "$out/$b.txt" | tail -1)
  if [ $rc -eq 0 ] || [ $rc -eq 5 ]; then st=PASS; else st=FAIL; fi
  load=$(powershell -NoProfile -Command "[int](Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average")
  line="[$n/$m] $b $st $(( $(date +%s) - t0 )) s free=${FREE}GB load=${load}% ($s)"
  [ -n "$LOG" ] && echo "$line" >> "$LOG"
  echo "$line" 2>/dev/null || true
done
