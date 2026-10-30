#!/bin/sh
# I19: test_app_present::test_long_session_memory_bounded alone on a quiet machine (load-dependent, orchestrator item 2)
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/I19 || exit 1
. /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I19/env.sh
O=/c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/merges/I19
echo "== start $(date '+%F %T')" > $O/mem_alone.log
test-slot /c/Users/maria/AppData/Local/Programs/Python/Python312/python.exe -m pytest "port/test/test_app_present.py::test_long_session_memory_bounded" -m "" -v -p no:cacheprovider >> $O/mem_alone.log 2>&1
echo "== rc=$? $(date '+%F %T')" >> $O/mem_alone.log
