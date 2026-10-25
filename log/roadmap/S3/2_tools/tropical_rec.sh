#!/bin/sh
# S3.2: the Tropical theme (U-1330: the stock game dies at the first `Tree Cerc` load on every tropical course, so the original is
# recorded with S3.4's alias copies Flics/Trees/Tropic/Tree_Cerc -> `Tree Cerc` and Tree_Drac -> `Tree Drac` in the instance only,
# removed afterwards). Study scenario cov_b_s34_hawaii (log/roadmap/S3/4_scenarios.py: menu -> Sandbox Mode -> Hawaii), its schedule
# log/c3/alt/refs/cov_b_s34_hawaii/schedule.json (S3.4's alias run). Two runs on instance s3_2: the trace (outside port/corpus/traces,
# so the boot gate never sees it) and the --pass-frames recording.
cd /c/Users/maria/Desktop/Proyectos/SimGolf
I=instances/s3_2
export SCENARIO_REFS_EXTRA=$PWD/log/roadmap/S3/4_scenarios.py SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none
alias_on() { [ -d "$I/Flics/Trees/Tropic/Tree Cerc" ] || cp -r "$I/Flics/Trees/Tropic/Tree_Cerc" "$I/Flics/Trees/Tropic/Tree Cerc";
             [ -d "$I/Flics/Trees/Tropic/Tree Drac" ] || cp -r "$I/Flics/Trees/Tropic/Tree_Drac" "$I/Flics/Trees/Tropic/Tree Drac"; }
alias_off() { rm -rf "$I/Flics/Trees/Tropic/Tree Cerc" "$I/Flics/Trees/Tropic/Tree Drac"; }
kill_game() { powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='golf_clean.exe'\" | ? { \$_.CommandLine -match 'instances.s3_2' } | % { Stop-Process -Id \$_.ProcessId -Force }"; }
mkdir -p log/p5/terrain/s32t_cov_b_s34_hawaii
alias_on
for try in 1 2 3; do
  timeout 3000 ~/bin/py -3.12 re/tools/trace_tool.py record --scenario cov_b_s34_hawaii --label s32t_trace --instance s3_2 \
    --out log/p5/terrain/s32t_cov_b_s34_hawaii/trace.trace > log/roadmap/S3/2_rec_t_trace.log 2>&1
  echo "trace try $try: $(tail -2 log/roadmap/S3/2_rec_t_trace.log | tr '\n' ' ')"; kill_game
  [ -s log/p5/terrain/s32t_cov_b_s34_hawaii/trace.trace ] && break
done
for try in 1 2 3; do
  timeout 3000 ~/bin/py -3.12 re/tools/terrain_calls.py --scenario cov_b_s34_hawaii --instance s3_2 --label s32t_cov_b_s34_hawaii \
    --pass-frames > log/roadmap/S3/2_rec_t_frames.log 2>&1
  echo "frames try $try: $(tail -1 log/roadmap/S3/2_rec_t_frames.log)"; kill_game
  grep -q "rc 0" log/roadmap/S3/2_rec_t_frames.log && break
done
alias_off
ls "$I/Flics/Trees/Tropic/"
