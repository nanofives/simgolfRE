#!/bin/bash
# S3 wave 8 merged-tree census on port/build/S3_W8: framegate --split (raw GlRaster columns + split verdict) in batches of 7
# scenarios, one w8_lock.py hold each; cached scenarios (<out>.d) are skipped; then the full TSV from the cache.
#   bash log/roadmap/S3/w8_census.sh        (waits for W8_GATES_DONE in w8_out_gates.log first)
cd /c/Users/maria/Desktop/Proyectos/SimGolf
T=log/roadmap/S3
TREE=port/build/S3_W8
OUT=$T/w8_out/census_split.tsv
export SG_PORTSIM_DLL=$PWD/$TREE/port/build/x64/Release/portsim_c.dll
until grep -q W8_GATES_DONE $T/w8_out_gates.log 2>/dev/null; do sleep 30; done
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
    ~/bin/py -3.12 $T/w8_lock.py run S3-lead --what "w8 census split$batch" -- bash -c \
      "cd $TREE && ~/bin/py -3.12 -m port.test.framegate $batch --split --jobs 1 --resume --out ../../../$OUT" >> $T/w8_out/census_split.log 2>&1
    done_n=$(ls $OUT.d/*.json 2>/dev/null | wc -l)
    echo "[$done_n/$N] census batch:$batch"
    sleep 125
  done
done
~/bin/py -3.12 $T/w8_lock.py run S3-lead --what "w8 census full tsv" -- bash -c \
  "cd $TREE && ~/bin/py -3.12 -m port.test.framegate --all --split --jobs 1 --resume --out ../../../$OUT" >> $T/w8_out/census_split.log 2>&1
echo "census rc=$? $(tail -n 1 $T/w8_out/census_split.log | cut -c1-200)"
echo W8_CENSUS_DONE
