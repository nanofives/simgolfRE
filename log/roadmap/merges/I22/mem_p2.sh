#!/bin/sh
# I22: bisect the +0.97 MB: the same memory test on the lead's S3_W9 tree (wave 9 without P2). Orchestrator, 2026-11-01.
# test on I21's build (port/build/I21, wave 8 + both fix layers) and on I22's, twice each, alternating, one S3 lock hold per run
# (step I22r). The facts files are written to separate directories. Orchestrator, 2026-11-01.
R=/c/Users/maria/Desktop/Proyectos/SimGolf
O=$R/log/roadmap/merges/I22/mem_ab
mkdir -p $O
LOCKPY="$HOME/bin/py -3.12 $R/log/roadmap/S3/w8_lock.py"
for round in 1; do
  for tree in S3_W9; do
    M=$R/port/build/$tree
    cd $M || exit 1
    export SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll SG_PORTSIM_W32_DIR=$M/port/build/Win32
    export SG_PORTSIM_DLL32=$M/port/build/Win32/Release/portsim_c.dll SG_PORTSIM_RUNNER32=$M/port/build/Win32/Release/portsim_runner32.exe
    export SG_APP_EXE=$M/port/build/x64/app/Release/simgolf_port.exe SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed' SG_TREE=$M
    export SG_APP_FACTS_DIR=$O/facts_${tree}_$round
    mkdir -p $SG_APP_FACTS_DIR
    unset SG_TEXT_METRICS SG_EMU_ORACLE SG_APP_PRIORITY SG_BACKEND
    $LOCKPY take I22r --priority --what "mem bisect $tree round $round" --need-gb 16 > /dev/null 2>&1
    ~/bin/py -3.12 -m pytest port/test/test_app_present.py -k test_long_session_memory_bounded -m "" -q -p no:cacheprovider > $O/${tree}_$round.txt 2>&1
    rc=$?
    $LOCKPY release I22r > /dev/null 2>&1
    g=$(~/bin/py -3.12 -c "import json,glob;d=json.load(open(glob.glob('$SG_APP_FACTS_DIR/*present*')[0]));print(d['long_memory']['growth_bound'])" 2>/dev/null)
    echo "$tree round $round rc=$rc growth_bound=$g $(tail -1 $O/${tree}_$round.txt) $(date '+%T')" >> $O/summary.txt
  done
done
echo "[mem_ab done] $(date '+%F %T')" >> $O/summary.txt
