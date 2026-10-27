#!/bin/sh
# S7.8: the app test files on the mirror port/build/S7_8_head, one pytest process per file (log/p38/suite_by_file.sh rule).
#   sh log/roadmap/S7/8_apptests.sh [file ...]     (from the repo root; default: every app test file, slow included)
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/port/build/S7_8_head
L=$R/log/roadmap/S7
cd "$M" || exit 1
export SG_APP_EXE=$M/port/build/S7_8/app/Release/simgolf_port.exe SG_PORTSIM_DLL=$M/port/build/S7_8/Release/portsim_c.dll
FILES=${*:-"test_app_shell test_app_input test_app_replay test_app_live test_app_ingame test_app_present"}
N=$(echo $FILES | wc -w); K=0
for f in $FILES; do
  K=$((K + 1))
  echo "[$K/$N] $f START"
  ~/bin/py -3.12 -m pytest "port/test/$f.py" -p no:cacheprovider -v > "$L/8_$f.log" 2>&1
  echo "[$K/$N] $f $(tail -1 "$L/8_$f.log")"
done
echo "apptests done"
