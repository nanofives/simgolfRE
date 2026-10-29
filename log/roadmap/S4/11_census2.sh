#!/bin/sh
# S4.11 final census: every S4 step's oracle keys (layouts A B C) on the mirror build WITH audio_crt, writing the re-recorded
# bundles and goldens into development directories (the tree's goldens stay as they are; 11_golden_compare.py compares them).
#   sh log/roadmap/S4/11_census2.sh <dll>
cd "$(dirname "$0")/../../.." || exit 1
DLL="$1"
export SG_PORTSIM_DLL="$DLL" SG_EMU_ORACLE=1
export SG_SOUND_BUNDLES="$PWD/port/corpus/audio_s411" SG_SOUND_GOLDENS="$PWD/log/roadmap/S4/11_goldens"
mkdir -p "$SG_SOUND_BUNDLES" "$SG_SOUND_GOLDENS"
for s in "S4.2" "S4.3" "S4.4" "S4.5" "S4.6" "S4.7" "S4.9" "S4.9b" "S4.8 --family ds" "S4.8 --family dev"; do
  n=$(echo "$s" | tr -d ' .-' | tr -d '[:upper:]' | cut -c1-12)
  ~/bin/py -3.12 -m port.test.sound_replay oracle --step $s --update --out "log/roadmap/S4/11_final_${n}.tsv" 2>&1 | tee "log/roadmap/S4/11_final_${n}.log"
done
echo DONE > "log/roadmap/S4/11_final.done"
