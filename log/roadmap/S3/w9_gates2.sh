#!/bin/bash
# S3 wave 9 stack check, second part on port/build/S3_W9: segments, boot and the census, which w9_gates.sh could not start
# (test-slot launches its command with CreateProcess, and ~/bin/py is a shell script: WinError 193). Each command runs as
# `test-slot bash -c '...'`, one w8_lock.py hold per step / census batch.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
S=log/roadmap/S3
T=port/build/S3_W9
O=$S/w9_out
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL="$(cd $T && pwd -W)/port/build/x64/Release/portsim_c.dll"
LOCK="$HOME/bin/py -3.12 $S/w8_lock.py run S3-lead"
TSL="$HOME/bin/test-slot"
step() {   # name, backend, inner command (run from the tree)
  $LOCK --what "w9 stack $1" -- bash -c "cd $T && SG_BACKEND=$2 $TSL bash -c '$3'" > $O/g_$1.log 2>&1
  echo "[$1] rc=$? :: $(grep -E 'counts|error|Error' $O/g_$1.log | tail -n 1 | cut -c1-160)"
}
step segs_x64 x64 "~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 1 --out ../../../$O/segs_x64.tsv"
step segs_w32 x64 "~/bin/py -3.12 -m port.test.integrator segments --backend win32 --jobs 1 --out ../../../$O/segs_w32.tsv"
step boot_x64 x64 "~/bin/py -3.12 -m port.test.boot --all --jobs 1 --out ../../../$O/boot_x64.tsv"
step boot_w32 win32 "~/bin/py -3.12 -m port.test.boot --all --jobs 1 --out ../../../$O/boot_w32.tsv"
OUT=$O/census_split.tsv
names=$(cd $T && ~/bin/py -3.12 -c "import json;print(' '.join(sorted(json.load(open('re/analysis/data/scenario_refs.json'))['scenarios'])))")
NS=$(echo $names | wc -w)
for pass in 1 2; do
  set -- $names
  while [ $# -gt 0 ]; do
    batch=""
    for j in 1 2 3 4 5 6 7; do
      if [ $# -gt 0 ]; then [ -f $OUT.d/$1.json ] || batch="$batch $1"; shift; fi
    done
    [ -z "$batch" ] && continue
    $LOCK --what "w9 census$batch" -- bash -c "cd $T && $TSL bash -c '~/bin/py -3.12 -m port.test.framegate $batch --split --jobs 1 --resume --out ../../../$OUT'" >> $O/census_split.log 2>&1
    echo "[$(ls $OUT.d/*.json 2>/dev/null | wc -l)/$NS] census batch:$batch"
  done
done
$LOCK --what "w9 census full tsv" -- bash -c "cd $T && $TSL bash -c '~/bin/py -3.12 -m port.test.framegate --all --split --jobs 1 --resume --out ../../../$OUT'" >> $O/census_split.log 2>&1
echo "census rc=$? $(tail -n 1 $O/census_split.log | cut -c1-200)"
echo W9_GATES2_DONE
