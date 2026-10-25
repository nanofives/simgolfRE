#!/bin/sh
# S3.2: lite --pass-frames recordings (the last terrain frame's entry and exit at each checkpoint) of every census scenario with a
# terrain-yes checkpoint that has no S3.2 recording yet; instance s3_2, hooks off, C3FT_AUDIO=none; up to 3 tries (Frida spawn hangs)
cd /c/Users/maria/Desktop/Proyectos/SimGolf
HAVE="sandbox_basic course_season cov_b_career_types cov_b_holes18 cov_l_camera cov_b_paths cov_k1_terrain cov_k3_camera cov_k1_paths"
for s in $(cat log/roadmap/S3/2_census_scenarios.txt); do
  case " $HAVE " in *" $s "*) continue;; esac
  [ -f port/corpus/traces/$s.trace ] || { echo "$s: no trace"; continue; }
  if [ -f log/p5/terrain/s32c_$s/index.json ] && grep -q '"rc": 0' log/p5/terrain/s32c_$s/index.json; then echo "$s: done"; continue; fi
  for try in 1 2 3; do
    SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none timeout 5400 ~/bin/py -3.12 re/tools/terrain_calls.py --scenario $s --instance s3_2 \
      --label s32c_$s --frames-lite > log/roadmap/S3/2_rec_c_$s.log 2>&1
    echo "$s try $try: $(tail -1 log/roadmap/S3/2_rec_c_$s.log)"
    powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='golf_clean.exe'\" | ? { \$_.CommandLine -match 'instances.s3_2' } | % { Stop-Process -Id \$_.ProcessId -Force }"
    if grep -q "rc 0" log/roadmap/S3/2_rec_c_$s.log; then
      ~/bin/py -3.12 re/tools/scenario_refs.py check --scenario $s --run s32c_$s 2>&1 | tail -1
      break
    fi
  done
done
