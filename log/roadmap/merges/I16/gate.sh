#!/bin/bash
# I16 gate (MERGE.md section 4). Each part tee'd to its own log.
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
L=log/roadmap/merges/I16
PY="$HOME/bin/py -3.12"

echo "== tests $(date -Iseconds)"
$PY -m pytest tests/test_match.py tests/test_match_tools.py tests/test_smsg_import.py tests/test_match_toolchain.py -q 2>&1 | tee $L/gate_tests.log | tail -3

echo "== match $(date -Iseconds)"
: > $L/gate_match.log
for f in re/match/smsg_*.cpp; do
  echo "### $f" >> $L/gate_match.log
  $PY re/tools/match.py "$f" --no-evidence >> $L/gate_match.log 2>&1
  echo "rc=$? $f" >> $L/gate_match.log
done
echo "match done"

echo "== static $(date -Iseconds)"
$PY -m pytest -m static -q 2>&1 | tee $L/gate_static.log | tail -3
echo "== end $(date -Iseconds)"
