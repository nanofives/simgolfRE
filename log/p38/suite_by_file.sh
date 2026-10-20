#!/bin/sh
# Phase 3.8: run port/test one file per pytest process (a single whole-suite process grew to 10-16 GB and exhausted the
# machine's commit charge), then the static suite. One line per file: [N/M] file STATUS (summary).
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
out=${OUT:-log/p38/suite_by_file}
mkdir -p "$out"
files=$(ls port/test/test_*.py)
m=$(echo "$files" | wc -l)
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
~/bin/py -3.12 -m pytest -m static -q -p no:cacheprovider > "$out/static.txt" 2>&1
echo "[static] $(tail -1 "$out/static.txt")"
