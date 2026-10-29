#!/bin/sh
# S4.9 final gate chain on the final mirror build (one heavy run at a time): negctl, default replay x64 and Win32, sinklog and its
# import-level controls, coverage, pytest. Logs next to this file.
cd "$(dirname "$0")/../../.." || exit 1
L=log/roadmap/S4
PY="$HOME/bin/py -3.12"
SG_EMU_ORACLE=1 $PY $L/9_replay.py negctl --family wave --out $L/9_negctl_final.tsv > $L/9_negctl_final.log 2>&1
$PY $L/9_replay.py replay --family wave > $L/9_replay_x64.log 2>&1
$PY $L/9_replay.py replay --family wave --w32 > $L/9_replay_w32.log 2>&1
$PY $L/9_replay.py sinklog --family wave > $L/9_sinklog.log 2>&1
SG_SINK_NEGCTL=closeMmioHandle:2 $PY $L/9_replay.py sinklog --family wave --keys closeMmioHandle WaveInDevice_reset > $L/9_sinklog_controls.log 2>&1
SG_SINK_NEGCTL=WaveInDevice_reset:2 $PY $L/9_replay.py sinklog --family wave --keys closeMmioHandle WaveInDevice_reset >> $L/9_sinklog_controls.log 2>&1
SG_EMU_ORACLE=1 $PY $L/9_cover.py --out $L/9_coverage.tsv > $L/9_coverage.log 2>&1
$PY -m pytest port/test/test_audio_wave.py -q > $L/9_pytest_default.log 2>&1
echo done > $L/9_final.done
