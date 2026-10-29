#!/bin/bash
# I16 step 3b: S10.7 copy step, then the C4 promotions of 2_/3_/6_/7_promote_cmds.txt.
# Skipped: gameMain 0x0045baf0 (U-1351 open) and sound strcpy 0x0004a010 (MERGE.md section 7).
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
S=log/roadmap/S10
grep '^cp ' $S/7_promote_cmds.txt | grep -v 0004a010 | while read -r line; do eval "$line" && echo "copied: ${line##* }"; done
for f in 2 3 6 7; do
  grep 're_classify.py promote' $S/${f}_promote_cmds.txt | sed 's/ *#.*$//' | while read -r line; do
    case "$line" in *0x0045baf0*|*0x0004a010*) echo "SKIP $line"; continue;; esac
    out=$(eval "$line" 2>&1); rc=$?
    echo "rc=$rc [$f] $(echo "$line" | grep -o 'promote 0x[0-9a-f]*') :: $(echo "$out" | tail -1)"
  done
done
