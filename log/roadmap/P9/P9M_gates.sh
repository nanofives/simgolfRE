#!/usr/bin/env bash
# P9 merge gates on port/build/P9M: builds, selftests x64 + Win32, segments x64 vs I25, app pytest files one per process.
set -u
R=/c/Users/maria/Desktop/Proyectos/SimGolf
cd $R/port/build/P9M
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
L=log/roadmap/P9; G=$L/P9M_gates.log
lock() { test-slot bash -c "~/bin/py -3.12 log/roadmap/S3/w8_lock.py run P9M --need-gb 8 -- bash -c '$1'"; }
lock "~/bin/py -3.12 port/test/build.py --arch x64 > $L/P9M_build_x64.log 2>&1; echo build_x64 rc=\$? >> $G"
lock "~/bin/py -3.12 port/test/build.py --arch Win32 > $L/P9M_build_Win32.log 2>&1; echo build_Win32 rc=\$? >> $G"
for a in x64 Win32; do
  E=port/build/$a/app/Release/simgolf_port.exe
  for t in "" -p5b -p7 -p8b -p9a -p9b; do
    $E --selftest$t > $L/P9M_selftest${t}_$a.log 2>&1; echo "selftest$t $a rc=$? $(tail -1 $L/P9M_selftest${t}_$a.log)" >> $G
  done
done
lock "~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 6 --out $L/P9M_segs_x64.tsv > $L/P9M_segs_x64.log 2>&1; echo segments rc=\$? >> $G"
~/bin/py -3.12 -m port.test.integrator compare log/roadmap/merges/I25/segs_x64.tsv $L/P9M_segs_x64.tsv > $L/P9M_segs_compare.log 2>&1; echo "compare rc=$? $(tail -2 $L/P9M_segs_compare.log | tr '\n' ' ')" >> $G
export SG_PORTSIM_DLL=$PWD/port/build/x64/Release/portsim_c.dll SG_APP_EXE=$PWD/port/build/x64/app/Release/simgolf_port.exe
for f in test_static test_profiles test_app_shell test_app_input test_app_replay test_app_ingame test_app_present test_app_live; do
  lock "~/bin/py -3.12 -m pytest port/test/$f.py -q -p no:cacheprovider > $L/P9M_t_$f.log 2>&1"
  echo "[$f] $(tail -1 $L/P9M_t_$f.log)" >> $G
done
echo done >> $G
