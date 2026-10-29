#!/bin/sh
# S4.11 fallback census: every S4 step's oracle keys (layouts A B C) on a mirror build, no --update (the goldens stay).
#   sh log/roadmap/S4/11_census.sh <dll> <tag>        e.g. .../x64_nocrt/Release/portsim_c.dll nocrt
cd "$(dirname "$0")/../../.." || exit 1
DLL="$1"; TAG="$2"
export SG_PORTSIM_DLL="$DLL" SG_EMU_ORACLE=1
for s in "S4.2" "S4.3" "S4.4" "S4.5" "S4.6" "S4.7" "S4.9" "S4.9b" "S4.8 --family ds" "S4.8 --family dev"; do
  n=$(echo "$s" | tr -d ' .-' | tr -d '[:upper:]' | cut -c1-12)
  ~/bin/py -3.12 -m port.test.sound_replay oracle --step $s --out "log/roadmap/S4/11_census_${TAG}_${n}.tsv" 2>&1 | tee "log/roadmap/S4/11_census_${TAG}_${n}.log"
done
echo DONE > "log/roadmap/S4/11_census_${TAG}.done"
