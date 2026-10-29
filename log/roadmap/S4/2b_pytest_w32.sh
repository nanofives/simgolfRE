#!/usr/bin/env bash
# S4.2b: the Win32-backend selections of the audio test files (one per process) and the opt-in setVolume grid, on the mirror
# build; the x64 runs of every audio file are part of the whole-tree suite (2b_suite_by_file.sh).
cd "$(dirname "$0")/../../.."
PT="$HOME/bin/py -3.12 -m pytest -q -p no:cacheprovider"
O=log/roadmap/S4
for m in core mixer chan ds dev codec seq sound wave soundfile crt exe; do
  $PT port/test/test_audio_$m.py --backend win32 -k "win32 or x87_ or replay_key or spill" > $O/2b_g3_pytest_${m}_w32.log 2>&1
  echo "$m w32: $(tail -n1 $O/2b_g3_pytest_${m}_w32.log)"
done
SG_EMU_ORACLE=1 $PT port/test/test_audio_ds.py -k setvolume_exhaustive > $O/2b_g3_pytest_setvolume_grid.log 2>&1
echo "grid: $(tail -n1 $O/2b_g3_pytest_setvolume_grid.log)"
echo "pytest w32 done"
