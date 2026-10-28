#!/bin/sh
# S3.14: 13_suite_by_file.sh pointed at the mirror port/build/S3_14 (env from 14_env.sh, sourced by 14_gates.sh). Env: OUT, FILES
# (space-separated file list, default every port/test/test_*.py). One line per file: [N/M] file STATUS (summary).
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_14 || exit 1
out=${OUT:-log/roadmap/S3/14_suite}
mkdir -p "$out"
files=${FILES:-$(ls port/test/test_*.py)}
m=$(echo $files | wc -w)
n=0
for f in $files; do
  n=$((n + 1))
  b=$(basename "$f" .py)
  ~/bin/py -3.12 -m pytest "$f" -q -p no:cacheprovider > "$out/$b.txt" 2>&1
  rc=$?
  s=$(tail -1 "$out/$b.txt")
  if [ $rc -eq 0 ] || [ $rc -eq 5 ]; then st=PASS; else st=FAIL; fi
  echo "[$n/$m] $b $st ($s)"
done
