#!/bin/bash
# S3 wave 9 stack check on port/build/S3_W9 (w9_assemble.py): ONE w8_lock.py hold per build / pytest file / gate step, then the census
# (raw + split) in batches of 7 scenarios. Lines "[k/N] step rc=.. secs :: last line"; W9_GATES_DONE at the end.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
S=log/roadmap/S3
T=port/build/S3_W9
O=$S/w9_out
mkdir -p $O
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL="$(cd $T && pwd -W)/port/build/x64/Release/portsim_c.dll"
LOCK="$HOME/bin/py -3.12 $S/w8_lock.py run S3-lead"
PT="-q -p no:cacheprovider"
STEPS=()
STEPS+=("build_x64|~/bin/py -3.12 $S/24_build.py x64 --tree S3_W9")
STEPS+=("build_w32|~/bin/py -3.12 $S/24_build.py Win32 --tree S3_W9")
for f in test_static test_gl_raster test_terrain_s3_28 test_gl_wgl test_gl_wgl_selfcheck test_terrain_render test_terrain_live test_gfx test_gfx_pixel test_gfx_reports test_framegate_split test_gfx_s3_26; do
  STEPS+=("x64_$f|cd $T && SG_BACKEND=x64 ~/bin/py -3.12 -m pytest port/test/$f.py $PT")
done
for f in test_terrain_s3_28 test_gl_wgl_selfcheck test_gfx; do
  STEPS+=("w32_$f|cd $T && SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/$f.py $PT")
done
STEPS+=("segs_x64|cd $T && $HOME/bin/test-slot ~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 1 --out ../../../$O/segs_x64.tsv")
STEPS+=("segs_w32|cd $T && $HOME/bin/test-slot ~/bin/py -3.12 -m port.test.integrator segments --backend win32 --jobs 1 --out ../../../$O/segs_w32.tsv")
STEPS+=("boot_x64|cd $T && $HOME/bin/test-slot ~/bin/py -3.12 -m port.test.boot --all --jobs 1 --out ../../../$O/boot_x64.tsv")
STEPS+=("boot_w32|cd $T && SG_BACKEND=win32 $HOME/bin/test-slot ~/bin/py -3.12 -m port.test.boot --all --jobs 1 --out ../../../$O/boot_w32.tsv")
N=${#STEPS[@]}; k=0
for s in "${STEPS[@]}"; do
  k=$((k+1)); name=${s%%|*}; cmd=${s#*|}; t0=$(date +%s)
  $LOCK --what "w9 stack $name" -- bash -c "$cmd" > $O/g_$name.log 2>&1
  rc=$?
  echo "[$k/$N] $name rc=$rc $(( $(date +%s) - t0 )) s :: $(grep -E 'passed|failed|counts|error' $O/g_$name.log | tail -n 1 | cut -c1-160)"
done
# census: raw + split, 7 scenarios per hold, resumable
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
    $LOCK --what "w9 census$batch" -- bash -c "cd $T && $HOME/bin/test-slot ~/bin/py -3.12 -m port.test.framegate $batch --split --jobs 1 --resume --out ../../../$OUT" >> $O/census_split.log 2>&1
    echo "[$(ls $OUT.d/*.json 2>/dev/null | wc -l)/$NS] census batch:$batch"
  done
done
$LOCK --what "w9 census full tsv" -- bash -c "cd $T && $HOME/bin/test-slot ~/bin/py -3.12 -m port.test.framegate --all --split --jobs 1 --resume --out ../../../$OUT" >> $O/census_split.log 2>&1
echo "census rc=$? $(tail -n 1 $O/census_split.log | cut -c1-200)"
echo W9_GATES_DONE
