#!/bin/sh
# S4.9b: the segment gate (x64, Win32) and the boot gate on the mirror port/build/S4_9b_mirror (pattern of 9_gates.sh)
cd "$(dirname "$0")/../../.." || exit 1
L=log/roadmap/S4
PY="$HOME/bin/py -3.12"
$PY -m port.test.integrator segments --dll port/build/S4_9b_mirror/x64/Release/portsim_c.dll --jobs 3 --out $L/9b_segments_mirror.tsv 2>&1 | tee $L/9b_segments_mirror.log
$PY -m port.test.integrator segments --backend win32 --dll32 port/build/S4_9b_mirror/w32/Release/portsim_c.dll --jobs 3 --out $L/9b_segments_mirror_w32.tsv 2>&1 | tee $L/9b_segments_mirror_w32.log
SG_PORTSIM_DLL=port/build/S4_9b_mirror/x64/Release/portsim_c.dll $PY -m port.test.boot --all --jobs 3 --out $L/9b_boot_gate_mirror.tsv 2>&1 | tee $L/9b_boot_gate_mirror.log
echo done > $L/9b_gates.done
