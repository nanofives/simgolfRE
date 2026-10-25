#!/bin/sh
# S2.13: run port test files one per process from the mirror root, one "[k/N] file STATUS" line each.
# usage: run_files.sh <mirror root> <dll> <backend x64|win32> <log prefix> <file>...
ROOT="$1"; DLL="$2"; BACK="$3"; PFX="$4"; shift 4
N=$#; K=0
cd "$ROOT" || exit 2
export SG_PORTSIM_DLL="$DLL"
if [ "$BACK" = "win32" ]; then export SG_BACKEND=win32; else unset SG_BACKEND; fi
for f in "$@"; do
  K=$((K + 1))
  name=$(basename "$f" .py)
  ~/bin/py -3.12 -m pytest "port/test/$f" -q -p no:cacheprovider > "${PFX}_${name}.log" 2>&1
  rc=$?
  tail_line=$(tail -n 1 "${PFX}_${name}.log")
  if [ $rc -eq 0 ]; then st=PASS; else st=FAIL; fi
  echo "[$K/$N] $name $st ($tail_line)"
done
