#!/bin/bash
# S3.23 finishing list, after 23_census.sh printed CENSUS_DONE. Heavy steps one lock hold each (W8_HEAVY_RULE.md, oldest wait first).
cd /c/Users/maria/Desktop/Proyectos/SimGolf
T=log/roadmap/S3
TREE=port/build/S3_23
VS_CMAKE="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
export PATH="$VS_CMAKE:$PATH"
X64DLL=$PWD/$TREE/port/build/x64/Release/portsim_c.dll
hold() {   # hold <what> <command...>: one bounded heavy run through w8_lock.py (W8_HEAVY_RULE.md, ENFORCED)
  local what=$1; shift
  ~/bin/py -3.12 $T/w8_lock.py run S3.23 --what "$what" -- "$@"; local rc=$?
  echo "[$what] rc=$rc"; sleep 125; return $rc
}
until grep -q CENSUS_DONE $T/23_census_run.log; do sleep 120; done
# 1. the full TSV from the cache (no boots)
# the cache must be copied BEFORE the --resume run (an empty <out>.d makes it boot every scenario without the lock)
rm -rf $T/23_census_split.tsv.d && cp -r $T/23_census_split_part.tsv.d $T/23_census_split.tsv.d || exit 1
[ "$(ls $T/23_census_split.tsv.d | wc -l)" = 70 ] || { echo 'cache incomplete'; exit 1; }
(cd $TREE && SG_PORTSIM_DLL=$X64DLL ~/bin/py -3.12 -m port.test.framegate --all --split --jobs 1 --resume \
   --out ../../../$T/23_census_split.tsv) 2>&1 | tail -3
~/bin/py -3.12 $T/23_summary.py > $T/23_summary.txt 2>&1
~/bin/py -3.12 $T/23_compare.py > $T/23_compare.txt 2>&1
cat $T/23_compare.txt
# 2. port static (x64)
hold "port static" bash -c "cd $TREE && SG_PORTSIM_DLL=$X64DLL ~/bin/py -3.12 -m pytest port/test/test_static.py -q -p no:cacheprovider 2>&1 | tee ../../../$T/23_port_static.log | tail -3"
# 3. Win32 build (portsim_c + runner32) and 4. test_framegate_split.py on it
hold "build Win32" bash -c "cd $TREE && ~/bin/py -3.12 port/test/build.py --arch Win32 > ../../../$T/23_build_w32.log 2>&1; tail -3 ../../../$T/23_build_w32.log"
hold "pytest split win32" bash -c "cd $TREE && SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_framegate_split.py -q -p no:cacheprovider 2>&1 | tee ../../../$T/23_pytest_split_w32.log | tail -5"
echo FINISH_DONE
