#!/usr/bin/env bash
# P9C gates, run from anywhere: builds (x64, Win32), app selftests x64 + Win32 (incl. p8b / p9a / p9b / p9c and the controls),
# segments x64, framegate sandbox_basic + cov_b_location, sim equality, the app test files one per process. Every heavy step
# takes the shared lock (log/roadmap/S3/w8_lock.py) inside the test slot. Usage: bash P9C_gates.sh [build|selftest|segments|
# framegate|simeq|pytest|all]
set -u
cd /c/Users/maria/Desktop/Proyectos/SimGolf/port/build/P9C
export SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
export SG_PORTSIM_DLL=$PWD/port/build/x64/Release/portsim_c.dll
L=log/roadmap/P9
lock() { ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run P9C --need-gb 6 -- bash -c "test-slot bash -c '$1'" > /dev/null 2>&1; }
step="${1:-all}"
if [ "$step" = all ] || [ "$step" = build ]; then
  lock "~/bin/py -3.12 port/test/build.py --arch x64 > $L/P9C_build_x64.log 2>&1; echo build_x64 rc=\$? >> $L/P9C_gates.log"
  lock "~/bin/py -3.12 port/test/build.py --arch Win32 > $L/P9C_build_Win32.log 2>&1; echo build_Win32 rc=\$? >> $L/P9C_gates.log"
fi
if [ "$step" = all ] || [ "$step" = selftest ]; then
  for a in x64 Win32; do
    E=port/build/$a/app/Release/simgolf_port_console.exe
    for t in "" -p5b -p7 -p8b -p9a -p9b -p9c; do
      $E --selftest$t > $L/P9C_selftest${t}_$a.log 2>&1
      echo "selftest$t $a rc=$? $(tail -1 $L/P9C_selftest${t}_$a.log)" >> $L/P9C_gates.log
    done
    for c in p9c-unremoved p9c-wrong-scale p9c-wrong-colour; do
      $E --selftest-p9c --negctl $c > $L/P9C_selftest-p9c_${c}_$a.log 2>&1
      echo "selftest-p9c --negctl $c $a rc=$? RED rows: $(grep -c FAIL $L/P9C_selftest-p9c_${c}_$a.log) $(grep FAIL $L/P9C_selftest-p9c_${c}_$a.log | tr '\n' ' ')" >> $L/P9C_gates.log
    done
  done
fi
if [ "$step" = all ] || [ "$step" = segments ]; then
  lock "~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 4 --out $L/P9C_segs_x64.tsv > $L/P9C_segs_x64.log 2>&1; echo segments rc=\$? >> $L/P9C_gates.log"
fi
if [ "$step" = all ] || [ "$step" = framegate ]; then
  lock "~/bin/py -3.12 -m port.test.framegate sandbox_basic cov_b_location --out $L/P9C_framegate.tsv > $L/P9C_framegate.log 2>&1"
  echo "framegate $(tail -1 $L/P9C_framegate.log)" >> $L/P9C_gates.log
fi
if [ "$step" = all ] || [ "$step" = simeq ]; then
  rm -f $L/P9C_simeq.tsv
  lock "bash $L/P9C_simeq.sh sandbox_basic course_season championship"
  echo "simeq $(grep -c -P '\tIDENTICAL$' $L/P9C_simeq.tsv) IDENTICAL / 6, control $(grep -c -P '\tRED$' $L/P9C_simeq.tsv) RED / 2" >> $L/P9C_gates.log
fi
if [ "$step" = all ] || [ "$step" = pytest ]; then
  for t in test_static test_profiles test_app_shell test_app_input test_app_replay test_app_ingame test_app_present test_app_live; do
    lock "~/bin/py -3.12 -m pytest port/test/$t.py -q -p no:cacheprovider > $L/P9C_t_$t.log 2>&1"
    echo "$t: $(tail -1 $L/P9C_t_$t.log)" >> $L/P9C_gates.log
  done
fi
echo "done $step $(date)" >> $L/P9C_gates.log
