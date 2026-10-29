#!/usr/bin/env bash
# S4.2b, resubmitted wave (S4.2 .. S4.11 on the consolidated core): every sound_replay / audio_stream gate, one run at a time, on
# the mirror build (port/build/S4_2b_mirror). Each step writes its own log (log/roadmap/S4/2b_g3_*).
# Usage: bash log/roadmap/S4/2b_gates2.sh [oracle|replay|negctl|sinklog|exe|all]
cd "$(dirname "$0")/../../.."
export SG_EMU_ORACLE=1
export SG_PORTSIM_DLL="$PWD/port/build/S4_2b_mirror/x64/Release/portsim_c.dll"
export SG_S4_MIRROR="$PWD/port/build/S4_2b_mirror"
SR="$HOME/bin/py -3.12 -m port.test.sound_replay"
AS="$HOME/bin/py -3.12 -m port.test.audio_stream"
O=log/roadmap/S4
what=${1:-all}
STEPS="S4.2 S4.3 S4.4 S4.5 S4.6 S4.7 S4.9 S4.9b S4.11"
t() { echo "${1//./}"; }

if [ $what = oracle ] || [ $what = all ]; then
  # every step re-recorded on the final build (11_merge.md section 5: with audio_crt the pinned fallbacks / pending go)
  for s in $STEPS; do
    $SR oracle --step $s --update --out $O/2b_g3_oracle_$(t $s).tsv > $O/2b_g3_oracle_$(t $s).log 2>&1
  done
  for f in ds dev; do
    $SR oracle --step S4.8 --family $f --update --out $O/2b_g3_oracle_S48$f.tsv > $O/2b_g3_oracle_S48$f.log 2>&1
  done
  $HOME/bin/py -3.12 $O/2b_golden_compare.py --out $O/2b_g3_golden_compare.tsv > $O/2b_g3_golden_compare.log 2>&1
fi

if [ $what = exe ] || [ $what = all ]; then
  # S4.10's own runner (the exe side): oracle (re-pinned), the segment streams x64 / Win32, the device-present run
  $AS oracle --update --out $O/2b_g3_exe_oracle.tsv > $O/2b_g3_exe_oracle.log 2>&1
  $AS stream --dll "$SG_PORTSIM_DLL" --jobs 3 --out $O/2b_g3_exe_streams_x64.tsv > $O/2b_g3_exe_streams_x64.log 2>&1
  $AS device --out $O/2b_g3_exe_device.txt > $O/2b_g3_exe_device.log 2>&1
fi

if [ $what = replay ] || [ $what = all ]; then
  for s in $STEPS; do
    $SR replay --step $s > $O/2b_g3_replay_x64_$(t $s).log 2>&1
    $SR replay --step $s --w32 > $O/2b_g3_replay_w32_$(t $s).log 2>&1
  done
  $SR replay --step S4.4 --w32 --soft > $O/2b_g3_replay_w32soft_S44.log 2>&1
  for f in ds dev; do
    $SR replay --step S4.8 --family $f > $O/2b_g3_replay_x64_S48$f.log 2>&1
    $SR replay --step S4.8 --family $f --w32 > $O/2b_g3_replay_w32_S48$f.log 2>&1
  done
fi

if [ $what = negctl ] || [ $what = all ]; then
  n() { local tag=$1; shift; $SR negctl "$@" --out $O/2b_g3_negctl_$tag.tsv > $O/2b_g3_negctl_$tag.log 2>&1; }
  n S42_m1 --step S4.2 --mode 1
  n S43_m1 --step S4.3 --mode 1
  n S43_m2 --step S4.3 --mode 2
  n S44_m1 --step S4.4 --mode 1
  n S44_m2 --step S4.4 --mode 2
  n S45_m1 --step S4.5 --mode 1
  n S45_m2 --step S4.5 --mode 2
  n S46_m1 --step S4.6 --mode 1
  n S46_m2 --step S4.6 --mode 2
  for m in 3 4 5 6 7 8; do n S46_vu_m$m --step S4.6 --mode $m --functions "Voice::update"; done
  n S47_m1 --step S4.7 --mode 1
  n S47_m2 --step S4.7 --mode 2           # Sound5b6a8_ctor: BLIND as accepted (U-1461, an equivalent mutant on these vectors)
  n S48ds_m1 --step S4.8 --family ds --mode 1
  n S48dev_m1 --step S4.8 --family dev --mode 1
  n S48ds_m2 --step S4.8 --family ds --mode 2
  n S48ds_m3 --step S4.8 --family ds --mode 3   # DsBuffer::setVolume mode 3: BLIND on its 30 vectors as accepted (the grid reads it)
  n S49_m1 --step S4.9 --mode 1
  for m in 1 2 3 4 5 6 7 8 9 10; do n S49b_m$m --step S4.9b --mode $m; done
  n S411_m1 --step S4.11 --mode 1
fi

if [ $what = sinklog ] || [ $what = all ]; then
  for f in ds dev; do $SR sinklog --step S4.8 --family $f > $O/2b_g3_sinklog_S48$f.log 2>&1; done
  $SR sinklog --step S4.9 > $O/2b_g3_sinklog_S49.log 2>&1
  $SR sinklog --step S4.9b > $O/2b_g3_sinklog_S49b.log 2>&1
  # S4.9's import-level controls: each must read DIFF
  $SR sinklog --step S4.9 --negctl snd.closeMmioHandle:2 > $O/2b_g3_sinklog_S49_closeMmioHandle2.log 2>&1
  $SR sinklog --step S4.9 --negctl snd.WaveInDevice_reset:2 > $O/2b_g3_sinklog_S49_WaveInDevice_reset2.log 2>&1
fi
echo "gates2 $what done" > $O/2b_g3_done_$what.txt
