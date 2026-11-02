#!/bin/bash
# S3.29: full census on port/build/S3_29 (framegate --split: raw GlRaster columns + split verdict) in batches of 7 scenarios, one
# `w8_lock.py run S3.29` hold each (w8_census.sh's pattern); cached scenarios (<out>.d) are skipped; then the full TSV from the cache.
#   bash log/roadmap/S3/29_census.sh on|off      on = SG_RASTER_TUNE=32=1 (the textured walk), off = the default
cd /c/Users/maria/Desktop/Proyectos/SimGolf
T=log/roadmap/S3
TREE=port/build/S3_29
cfg=${1:-on}
OUT=$T/29_out/census_$cfg.tsv
LOG=$T/29_out/census_$cfg.log
PYX=/c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe
export SG_PORTSIM_DLL=$PWD/$TREE/port/build/x64/Release/portsim_c.dll
unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY SG_GL_BACKEND SG_RASTER_TUNE
[ "$cfg" = on ] && export SG_RASTER_TUNE=32=1
names=$(cd $TREE && ~/bin/py -3.12 -c "import json;print(' '.join(sorted(json.load(open('re/analysis/data/scenario_refs.json'))['scenarios'])))")
N=$(echo $names | wc -w)
for pass in 1 2; do
  set -- $names
  while [ $# -gt 0 ]; do
    batch=""
    for k in 1 2 3 4 5 6 7; do
      if [ $# -gt 0 ]; then [ -f $OUT.d/$1.json ] || batch="$batch $1"; shift; fi
    done
    [ -z "$batch" ] && continue
    ~/bin/py -3.12 $T/w8_lock.py run S3.29 --what "29 census $cfg$batch" -- bash -c \
      "cd $TREE && $PYX -m port.test.framegate $batch --split --jobs 1 --resume --out ../../../$OUT" >> $LOG 2>&1
    done_n=$(ls $OUT.d/*.json 2>/dev/null | wc -l)
    echo "[$done_n/$N] census $cfg batch:$batch"
    sleep 60
  done
done
~/bin/py -3.12 $T/w8_lock.py run S3.29 --what "29 census $cfg full tsv" -- bash -c \
  "cd $TREE && $PYX -m port.test.framegate --all --split --jobs 1 --resume --out ../../../$OUT" >> $LOG 2>&1
echo "census $cfg rc=$? $(tail -n 1 $LOG | cut -c1-200)"
echo S329_CENSUS_${cfg}_DONE
