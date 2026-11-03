#!/bin/bash
# S3.31 census on port/build/S3_31 (copy of w8_census.sh): framegate --split in batches of 7 scenarios, one w8_lock.py hold
# each (step S3.31); cached scenarios (<out>.d) are skipped; then the full TSV from the cache.
#   bash log/roadmap/S3/31_census.sh
cd /c/Users/maria/Desktop/Proyectos/SimGolf
T=log/roadmap/S3
# one live take per step: wait until no other S3.31 hold or wait is running (the unit tests)
while ~/bin/py -3.12 $T/w8_lock.py status | grep -q "S3.31"; do sleep 30; done
TREE=port/build/S3_31
OUT=$T/31_out/census_split.tsv
export SG_PORTSIM_DLL=$PWD/$TREE/port/build/x64/Release/portsim_c.dll
names=$(cd $TREE && ~/bin/py -3.12 -c "import json;print(' '.join(sorted(json.load(open('re/analysis/data/scenario_refs.json'))['scenarios'])))")
N=$(echo $names | wc -w); done_n=0
for pass in 1 2; do
  set -- $names
  while [ $# -gt 0 ]; do
    batch=""
    for k in 1 2 3 4 5 6 7; do
      if [ $# -gt 0 ]; then [ -f $OUT.d/$1.json ] || batch="$batch $1"; shift; fi
    done
    [ -z "$batch" ] && continue
    ~/bin/py -3.12 $T/w8_lock.py run S3.31 --what "S3.31 census split$batch" -- bash -c \
      "cd $TREE && test-slot bash -c '~/bin/py -3.12 -m port.test.framegate $batch --split --jobs 1 --resume --out ../../../$OUT'" >> $T/31_out/census_split.log 2>&1
    echo "batch rc=$?"
    done_n=$(ls $OUT.d/*.json 2>/dev/null | wc -l)
    echo "[$done_n/$N] census batch:$batch"
    sleep 60
  done
done
~/bin/py -3.12 $T/w8_lock.py run S3.31 --what "S3.31 census full tsv" -- bash -c \
  "cd $TREE && ~/bin/py -3.12 -m port.test.framegate --all --split --jobs 1 --resume --out ../../../$OUT" >> $T/31_out/census_split.log 2>&1
echo "census rc=$? $(tail -n 1 $T/31_out/census_split.log | cut -c1-200)"
echo S331_CENSUS_DONE
