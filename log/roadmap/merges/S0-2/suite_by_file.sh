#!/bin/sh
# I2 (S0 merge): copy of log/roadmap/merges/S6-1/suite_by_file.sh pointed at the S0 mirror port/build/I2_S0.
# Env: OUT (default log/roadmap/merges/S0-2/suite), SG_BACKEND (x64 default, win32 for the Win32 pass),
# FILES (space-separated file list, default every port/test/test_*.py), STATIC=0 skips the trailing static run.
# One line per file: [N/M] file STATUS (summary).
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I2_S0 || exit 1
out=${OUT:-log/roadmap/merges/S0-2/suite}
mkdir -p "$out"
files=${FILES:-$(ls port/test/test_*.py)}
m=$(echo $files | wc -w)
n=0
for f in $files; do
  n=$((n + 1))
  b=$(basename "$f" .py)
  t0=$(date +%s)
  ~/bin/py -3.12 -m pytest "$f" -q -p no:cacheprovider > "$out/$b.txt" 2>&1
  rc=$?
  s=$(tail -1 "$out/$b.txt")
  if [ $rc -eq 0 ] || [ $rc -eq 5 ]; then st=PASS; else st=FAIL; fi
  echo "[$n/$m] $b $st ($s) $(( $(date +%s) - t0 )) s"
done
if [ "${STATIC:-1}" = "1" ]; then
  ~/bin/py -3.12 -m pytest -m static -q -p no:cacheprovider > "$out/static.txt" 2>&1
  echo "[static] $(tail -1 "$out/static.txt")"
fi
