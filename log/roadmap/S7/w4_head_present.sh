#!/bin/sh
# S7 lead: test_app_present.py's memory test on the plain I19 tree (no S7 wave 4 code), x64, after the wave-4 pass on
# S7_w4_root ends (w4_g_gates.log [done]): does test_long_session_memory_bounded fail on the merge base too?
R=/c/Users/maria/Desktop/Proyectos/SimGolf
O=$R/log/roadmap/S7
until grep -q "^\[done\]" $O/w4_g_gates.log 2>/dev/null; do sleep 60; done
M=$R/port/build/I19
cd $M || exit 1
mkdir -p $O/w4_out/head_facts
SG_PORTSIM_DLL=$M/port/build/x64/Release/portsim_c.dll SG_APP_EXE=$M/port/build/x64/app/Release/simgolf_port.exe \
SG_APP_FACTS_DIR=$O/w4_out/head_facts SG_TREE=$M \
  ~/bin/py -3.12 -m pytest port/test/test_app_present.py -m "" -v -p no:cacheprovider -k "long_session" \
  > $O/w4_head_present.log 2>&1
echo "[head present done] rc=$?" >> $O/w4_head_present.log
