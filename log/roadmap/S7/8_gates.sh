#!/bin/sh
# S7.8: the regression gates on the mirror port/build/S7_8_head (S7.5 mirror + S7.8), one at a time.
#   sh log/roadmap/S7/8_gates.sh [x64|win32]     (from the repo root)
B=${1:-x64}
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/port/build/S7_8_head
L=$R/log/roadmap/S7
cd "$M" || exit 1
export SG_PORTSIM_DLL=$M/port/build/S7_8/Release/portsim_c.dll
if [ "$B" = win32 ]; then
  export SG_BACKEND=win32 SG_PORTSIM_DLL32=$M/port/build/S7_8_w32/Release/portsim_c.dll SG_PORTSIM_RUNNER32=$M/port/build/S7_8_w32/Release/portsim_runner32.exe
  echo "[1/2] equivalence win32 START"
  ~/bin/py -3.12 -m port.test.live equiv --all --jobs 3 --backend win32 --out "$L/8_equiv_w32.tsv" 2>&1 | tee "$L/8_equiv_w32.log" | tail -3
  echo "[2/2] segments win32 START"
  ~/bin/py -3.12 -m port.test.integrator segments --backend win32 --jobs 3 --out "$L/8_segs_win32.tsv" 2>&1 | tee "$L/8_segs_win32.log" | tail -3
  exit 0
fi
echo "[1/3] equivalence x64 START"
~/bin/py -3.12 -m port.test.live equiv --all --jobs 3 --out "$L/8_equiv_x64.tsv" 2>&1 | tee "$L/8_equiv_x64.log" | tail -3
echo "[2/3] segments x64 START"
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll "$SG_PORTSIM_DLL" --jobs 3 --out "$L/8_segs_x64.tsv" 2>&1 | tee "$L/8_segs_x64.log" | tail -3
echo "[3/3] boot gate x64 START"
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out "$L/8_boot_gate.tsv" 2>&1 | tee "$L/8_boot_gate.log" | tail -3
echo "gates done"
