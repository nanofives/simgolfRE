#!/usr/bin/env bash
# S4.2b: the eleven audio test files of the wave, one per process (log/p38/suite_by_file.sh rule), default (bundles + goldens) on
# x64 and the Win32 backend selection, then the opt-in setVolume grid. They load the mirror build (sound_replay MIRROR).
cd "$(dirname "$0")/../../.."
PT="$HOME/bin/py -3.12 -m pytest -q -p no:cacheprovider"
O=log/roadmap/S4
for m in names core mixer chan ds dev codec seq sound wave soundfile; do
  f=port/test/test_audio_$m.py
  $PT $f 2>&1 | tail -4 | tee $O/2b_g2_pytest_${m}_x64.log | tail -1
  [ $m = names ] && continue
  $PT $f --backend win32 -k "win32 or x87_ or replay_key or spill" 2>&1 | tail -4 | tee $O/2b_g2_pytest_${m}_w32.log | tail -1
done
SG_EMU_ORACLE=1 $PT port/test/test_audio_ds.py -k setvolume_exhaustive 2>&1 | tail -4 | tee $O/2b_g2_pytest_setvolume_grid.log | tail -1
echo "pytest all done"
