#!/bin/bash
# S3.23: the split census in batches of 4 scenarios, one lock hold per batch (W8_HEAVY_RULE.md fair turns: the OLDEST
# w8_wait_* file may take a free lock; after a hold this script queues behind every existing wait file again).
# A batch that finds < 16 GB free commit releases the lock, re-queues, waits 5 min and retries (never skips).
# Scenarios whose result is already cached in <out>.d are dropped from their batch.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
T=log/roadmap/S3
TREE=port/build/S3_23
OUT=$T/23_census_split_part.tsv
export SG_PORTSIM_DLL=$PWD/$TREE/port/build/x64/Release/portsim_c.dll
names=$(cd $TREE && ~/bin/py -3.12 -c "import json;print(' '.join(sorted(json.load(open('re/analysis/data/scenario_refs.json'))['scenarios'])))")
set -- $names
oldest() { [ "$(ls -tr $T/w8_wait_* 2>/dev/null | head -1)" = "$T/w8_wait_S3.23" ]; }
while [ $# -gt 0 ]; do
  batch=""
  for k in 1 2 3 4; do
    if [ $# -gt 0 ]; then [ -f $OUT.d/$1.json ] || batch="$batch $1"; shift; fi
  done
  [ -z "$batch" ] && continue
  done_batch=0
  while [ $done_batch = 0 ]; do
    [ -f $T/w8_wait_S3.23 ] || date +%H:%M > $T/w8_wait_S3.23
    until [ ! -d $T/w8_heavy.lock ] && oldest && mkdir $T/w8_heavy.lock 2>/dev/null; do sleep 5; done
    rm -f $T/w8_wait_S3.23
    echo "S3.23 census split $batch $(date +%H:%M)" > $T/w8_heavy.lock/owner.txt
    if ~/bin/py -3.12 $T/w8_commit_free.py; then
      (cd $TREE && ~/bin/py -3.12 -m port.test.framegate $batch --split --jobs 1 --resume --out ../../../$OUT) 2>&1 | tee -a $T/23_census_split.log
      done_batch=1
      rm -rf $T/w8_heavy.lock
      date +%H:%M > $T/w8_wait_S3.23
      sleep 125
    else
      rm -rf $T/w8_heavy.lock
      date +%H:%M > $T/w8_wait_S3.23
      sleep 300
    fi
  done
done
rm -f $T/w8_wait_S3.23
echo CENSUS_DONE
