#!/bin/sh
# S4.11: the default replay (no emulator) of every S4 step's re-recorded bundles against the re-pinned goldens (11_census2.sh's dev
# directories), x64 or Win32:   sh log/roadmap/S4/11_replay_all.sh <dll> [--w32]
cd "$(dirname "$0")/../../.." || exit 1
export SG_PORTSIM_DLL="$1" SG_SOUND_BUNDLES="$PWD/port/corpus/audio_s411" SG_SOUND_GOLDENS="$PWD/log/roadmap/S4/11_goldens"
export SG_S4_MIRROR="$PWD/port/build/S4_11_mirror"
W="$2"
LIST=${STEPS:-"S4.2|S4.3|S4.4|S4.5|S4.6|S4.7|S4.9|S4.9b|S4.8 --family ds|S4.8 --family dev"}   # STEPS="S4.9|S4.9b" to run some
OLDIFS=$IFS; IFS="|"
for s in $LIST; do
  IFS=$OLDIFS
  echo "== $s $W"
  ~/bin/py -3.12 -m port.test.sound_replay replay --step $s $W 2>&1 | tail -3
  IFS="|"
done
