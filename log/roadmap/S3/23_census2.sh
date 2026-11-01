#!/bin/bash
# S3.23: the split census in batches of 4 scenarios, one `w8_lock.py run` hold per batch (W8_HEAVY_RULE.md, ENFORCED).
# Scenarios cached in <out>.d are skipped; a batch whose framegate fails leaves no cache file and is retried next pass.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
T=log/roadmap/S3
TREE=port/build/S3_23
OUT=$T/23_census_split_part.tsv
export SG_PORTSIM_DLL=$PWD/$TREE/port/build/x64/Release/portsim_c.dll
names=$(cd $TREE && ~/bin/py -3.12 -c "import json;print(' '.join(sorted(json.load(open('re/analysis/data/scenario_refs.json'))['scenarios'])))")
for pass in 1 2; do
  set -- $names
  while [ $# -gt 0 ]; do
    batch=""
    for k in 1 2 3 4; do
      if [ $# -gt 0 ]; then [ -f $OUT.d/$1.json ] || batch="$batch $1"; shift; fi
    done
    [ -z "$batch" ] && continue
    ~/bin/py -3.12 $T/w8_lock.py run S3.23 --what "census split$batch" -- bash -c \
      "cd $TREE && ~/bin/py -3.12 -m port.test.framegate $batch --split --jobs 1 --resume --out ../../../$OUT" 2>&1 | tee -a $T/23_census_split.log
    sleep 125
  done
done
echo CENSUS_DONE
