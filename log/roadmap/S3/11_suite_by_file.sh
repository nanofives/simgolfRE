#!/bin/sh
# S3.11: I6's suite_by_file_I6b.sh pointed at the S3.11 tree port/build/S3_11_src (its port/build/x64 and Win32 are junctions to
# port/build/S3_11 / S3_11_w32). Env: OUT (default log/roadmap/S3/11_suite_x64), SG_BACKEND (x64 default, win32 for the Win32 pass),
# FILES (space-separated file list, default every port/test/test_*.py), STATIC=0 skips the trailing static run.
# One line per file: [N/M] file STATUS (summary).
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_11_src || exit 1
out=${OUT:-log/roadmap/S3/11_suite_x64}
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
if [ "${STATIC:-1}" = "1" ]; then
  ~/bin/py -3.12 -m pytest -m static -q -p no:cacheprovider > "$out/static.txt" 2>&1
  echo "[static] $(tail -1 "$out/static.txt")"
fi
