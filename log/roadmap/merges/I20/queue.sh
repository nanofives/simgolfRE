#!/bin/sh
# I20 single queue (S7 wave 4 on f2dff003), one heavy run at a time, detached; progress in g_gates.log / g_suite_*.log.
# 1. regression gates (no census / terrain: wave 4 changes no drawing code), with S7's switch-0 segment and boot runs;
# 2. block 2: the files wave 4 can reach, x64 then Win32 (marker `[block2 done]`);
# 3. the rest of the x64 suite, then the rest of the Win32 suite (I19's win32_files.txt + test_app_audio + test_defects_golfsfx).
trap '' PIPE
cd /c/Users/maria/Desktop/Proyectos/SimGolf || exit 1
G=log/roadmap/merges/I20
echo "== queue start $(date '+%F %T')" >> $G/g_gates.log
sh $G/gates.sh static segs_x64 segs_win32 segs_ft_x64 boot_x64 boot_win32 boot_ft_x64 \
  segs0_x64 segs0_win32 boot0_x64 boot0_win32 chain defects live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32
B2=$(cd port/build/I20 && { ls port/test/test_audio_*.py; echo port/test/test_defects_golfsfx.py port/test/test_profiles.py;
  ls port/test/test_defects_*.py | grep -v test_defects_golfsfx; echo port/test/test_integrator.py port/test/test_static.py \
  port/test/test_live.py; ls port/test/test_app_*.py | grep -v test_app_audio; echo port/test/test_app_audio.py; } | tr '\n' ' ')
echo "$B2" | tr ' ' '\n' | grep . > $G/block2_files.txt
echo "== block2_x64 start $(date '+%F %T') ($(wc -l < $G/block2_files.txt) files)" >> $G/g_gates.log
OUT=$G/b2_x64 LOG=$G/g_b2_x64.log FILES="$B2" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "== block2_win32 start $(date '+%F %T')" >> $G/g_gates.log
SG_BACKEND=win32 OUT=$G/b2_win32 LOG=$G/g_b2_win32.log FILES="$B2" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "[block2 done] $(date '+%F %T')" >> $G/g_gates.log
X=$(cd port/build/I20 && { ls port/test/test_*.py | grep -v test_app_; ls port/test/test_app_*.py; } | grep -vxF -f $G/block2_files.txt | tr '\n' ' ')
W=$({ tr ' ' '\n' < log/roadmap/merges/I19/win32_files.txt; echo port/test/test_app_audio.py; echo port/test/test_defects_golfsfx.py; } \
  | grep . | grep -vxF -f $G/block2_files.txt | tr '\n' ' ')
echo "$X" | tr ' ' '\n' | grep . > $G/rest_x64_files.txt
echo "$W" | tr ' ' '\n' | grep . > $G/rest_win32_files.txt
echo "== suite_x64 (rest) start $(date '+%F %T') ($(wc -l < $G/rest_x64_files.txt) files)" >> $G/g_gates.log
OUT=$G/suite LOG=$G/g_suite_x64.log FILES="$X" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "== suite_win32 (rest) start $(date '+%F %T') ($(wc -l < $G/rest_win32_files.txt) files)" >> $G/g_gates.log
SG_BACKEND=win32 OUT=$G/suite_win32 LOG=$G/g_suite_win32.log FILES="$W" sh $G/suite_by_file.sh > /dev/null 2>&1
echo "[queue done] $(date '+%F %T')" >> $G/g_gates.log
