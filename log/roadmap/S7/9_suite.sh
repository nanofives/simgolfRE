#!/bin/sh
# S7.9: the port suite one file per process on the mirror port/build/S7_9_head (log/p38/suite_by_file.sh's rule), from the repo
# root:  sh log/roadmap/S7/9_suite.sh [x64|win32] [files...]     (default: every port/test/test_*.py for x64, I7's Win32 list
# log/roadmap/merges/I7/win32_files.txt for win32). Per-file output in log/roadmap/S7/9_suite_<backend>/<file>.txt; one line per
# file: [N/M] file STATUS (summary). PYTEST_ARGS adds pytest arguments (e.g. "-m slow").
B=${1:-x64}
[ $# -gt 0 ] && shift
R=$(pwd)
if [ "$B" = win32 ]; then . "$R/log/roadmap/S7/9_env.sh" win32; else . "$R/log/roadmap/S7/9_env.sh"; fi
out=$L/9_suite_$B
mkdir -p "$out"
if [ $# -gt 0 ]; then files="$*"
elif [ "$B" = win32 ]; then files=$(cat "$R/log/roadmap/merges/I7/win32_files.txt")
else files=$(ls port/test/test_*.py); fi
m=$(echo $files | wc -w)
n=0
for f in $files; do
  n=$((n + 1))
  b=$(basename "$f" .py)
  ~/bin/py -3.12 -m pytest "$f" -q -p no:cacheprovider $PYTEST_ARGS > "$out/$b.txt" 2>&1
  rc=$?
  s=$(tail -1 "$out/$b.txt")
  if [ $rc -eq 0 ] || [ $rc -eq 5 ]; then st=PASS; else st=FAIL; fi
  echo "[$n/$m] $b $st ($s)"
done
