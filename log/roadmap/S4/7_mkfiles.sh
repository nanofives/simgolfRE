#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
{ echo port/src/sim/audio_sound.cpp; echo port/include/portsim/sim/audio_sound.h; echo port/test/test_audio_sound.py; echo port/test/golden_audio_sound.json
  echo log/roadmap/S4/s7_patches.py
  find log/roadmap/S4 -maxdepth 1 -type f -name '7_*'
  find log/roadmap/S4/7_conv log/roadmap/S4/7_parts -type f; } | sort -u > log/roadmap/S4/7_files.txt
wc -l log/roadmap/S4/7_files.txt
